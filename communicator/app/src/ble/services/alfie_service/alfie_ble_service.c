/*
 * Copyright (C) Ovyl
 */

/**
 * @file alfie_ble_service.c
 * @author Evan Stoddard
 * @brief
 */

#include "alfie_ble_service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>

#include <zephyr/random/random.h>

#include <zephyr/sys/util.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/conn.h>

#include "ble/services/alfie_service/alfie_ble_service_protocol.h"

#include "utils/transport_buffer.h"

#include "alfie/alfie_transport.h"

/*****************************************************************************
 * Definitions
 *****************************************************************************/

LOG_MODULE_REGISTER(alfie_ble_service);

#define ALFIE_BLE_SERVICE_UUID_VAL           BT_UUID_128_ENCODE(0xc1534fa3, 0x5211, 0x4e32, 0xa176, 0xd1af04513305)
#define ALFIE_BLE_SERVICE_DATA_CHAR_UUID_VAL BT_UUID_128_ENCODE(0xc1534fa4, 0x5211, 0x4e32, 0xa176, 0xd1af04513305)

static struct bt_uuid_128 prv_alfie_ble_service_uuid = BT_UUID_INIT_128(ALFIE_BLE_SERVICE_UUID_VAL);
static struct bt_uuid_128 prv_alfie_ble_service_data_char_uuid = BT_UUID_INIT_128(ALFIE_BLE_SERVICE_DATA_CHAR_UUID_VAL);

#define ALFIE_BLE_SERVICE_UUID           ((const struct bt_uuid *)&prv_alfie_ble_service_uuid)
#define ALFIE_BLE_SERVICE_DATA_CHAR_UUID ((const struct bt_uuid *)&prv_alfie_ble_service_data_char_uuid)

#define ALFIE_BLE_SERVICE_NUM_RX_BUFFERS (4U)

#define ALFIE_BLE_SERVICE_MAX_RX_BUFFER_SIZE_BYTES (512U)

#define ALFIE_BLE_SERVICE_RX_TIMEOUT_MS (10000U)

#define ALFIE_BLE_SERVICE_TX_ATTEMPTS (4U)

#define ALFIE_BLE_SERVICE_ACK_TIMEOUT_MS (ALFIE_BLE_SERVICE_RX_TIMEOUT_MS / ALFIE_BLE_SERVICE_TX_ATTEMPTS)

NET_BUF_POOL_DEFINE(prv_rx_buffer_pool, ALFIE_BLE_SERVICE_NUM_RX_BUFFERS, ALFIE_BLE_SERVICE_MAX_RX_BUFFER_SIZE_BYTES, 0,
                    NULL);

/*****************************************************************************
 * Variables
 *****************************************************************************/

typedef enum {
    ALFIE_BLE_SERVICE_GATT_ATTR_SERVICE = 0,
    ALFIE_BLE_SERVICE_GATT_ATTR_DATA_CHRC = 1,
    ALFIE_BLE_SERVICE_GATT_ATTR_DATA = 2,
    ALFIE_BLE_SERVICE_GATT_ATTR_DATA_CCC = 3,
} alfie_service_gatt_attr_idx_t;

/**
 * @brief Private instance
 */
static struct {
    bool initialized;

    transport_buffer_pool_t rx_pool;
    transport_buffer_t rx_buffers[ALFIE_BLE_SERVICE_NUM_RX_BUFFERS];

    alfie_transport_rx_callback_t transport_rx_callback;

    alfie_transport_t transport;
    alfie_transport_api_t transport_api;

    struct bt_conn *conn;

    struct k_sem ack_sem;
} prv_inst;

/*****************************************************************************
 * BLE Bindings
 *****************************************************************************/

/**
 * @brief Called when data written to data characteristic by connected device
 *
 * @param conn Pointer to connection
 * @param attr Pointer to attribute
 * @param buf Pointer to incoming RX buffer
 * @param len Length of RX buffer in bytes
 * @param offset Offset of total transmission (currently not used)
 * @param flags Unused flags
 * @return Returns len on success or negative BT_ATT_ERR
 */
static ssize_t prv_on_data_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len,
                                 uint16_t offset, uint8_t flags);

/**
 * @brief Called when configuration of data characteristic changed (e.g. subscription)
 *
 * @param attr Pointer to attribute
 * @param value Updated value (ignored)
 */
static void prv_on_data_char_config_changed(const struct bt_gatt_attr *attr, uint16_t value);

/**
 * @brief Called when new BLE connection is made
 *
 * @param conn Pointer to connection
 * @param err Error when connection is being established
 */
static void prv_device_connected(struct bt_conn *conn, uint8_t err);

/**
 * @brief Called when BLE connection is closed
 *
 * @param conn Connection closed
 * @param reason Reason for closed connection
 */
static void prv_device_disconnected(struct bt_conn *conn, uint8_t reason);

BT_GATT_SERVICE_DEFINE(prv_alfie_ble_service, BT_GATT_PRIMARY_SERVICE(ALFIE_BLE_SERVICE_UUID),
                       BT_GATT_CHARACTERISTIC(ALFIE_BLE_SERVICE_DATA_CHAR_UUID,
                                              BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_WRITE, NULL,
                                              prv_on_data_write, NULL),
                       BT_GATT_CCC(prv_on_data_char_config_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE));

BT_CONN_CB_DEFINE(prv_conn_callbacks) = {
    .connected = prv_device_connected,
    .disconnected = prv_device_disconnected,
};

/*****************************************************************************
 * Private Functions
 *****************************************************************************/

/**
 * @brief Acknowledge received data frame
 *
 * @param frame Pointer to received data frame
 */
static void prv_write_data_ack(const alfie_ble_service_proto_data_frame_t *frame)
{
    alfie_ble_service_proto_data_ack_frame_t ack_frame = {
        .header.version = 0,
        .header.frame_type = ALFIE_BLE_SERVICE_PROTO_FRAME_TYPE_DATA_ACK,
        .seq_id = frame->seq_id,
        .frag_idx = frame->frag_idx,
    };

    int ret = bt_gatt_notify(prv_inst.conn, &attr_prv_alfie_ble_service[ALFIE_BLE_SERVICE_GATT_ATTR_DATA], &ack_frame,
                             sizeof(ack_frame));

    if (ret != 0) {
        LOG_ERR("Failed to write ACK: %d", ret);
        return;
    }
}

/**
 * @brief Write data frame to BLE data characteristic, handling retries and ACKs
 *
 * @param frame Pointer to outbound frame
 * @param len_bytes Total length of frame (including header and payload) in bytes
 * @return Returns 0 on success or negative -errno value
 */
static int prv_write_data_frame(const alfie_ble_service_proto_data_frame_t *frame, const size_t len_bytes)
{
    int ret = 0;

    for (uint8_t i = 0; i < ALFIE_BLE_SERVICE_TX_ATTEMPTS; i++) {
        k_sem_reset(&prv_inst.ack_sem);

        ret = bt_gatt_notify(prv_inst.conn, &attr_prv_alfie_ble_service[ALFIE_BLE_SERVICE_GATT_ATTR_DATA], frame,
                             len_bytes);

        if (ret != 0) {
            return ret;
        }

        ret = k_sem_take(&prv_inst.ack_sem, K_MSEC(ALFIE_BLE_SERVICE_ACK_TIMEOUT_MS));
        if (ret == 0) {
            return 0;
        }
    }

    return ret;
}

/**
 * @brief Called when a complete transaction has been received
 *
 * @param buffer Pointer to buffer
 */
static void prv_handle_complete_incoming_data_transaction(transport_buffer_t *buffer)
{
    if (prv_inst.transport_rx_callback == NULL) {
        return;
    }

    prv_inst.transport_rx_callback(&prv_inst.transport, buffer);
}

/**
 * @brief Handling incoming data frame
 *
 * @param header Pointer to header of incoming frame
 * @param len_bytes Size of RX buffer
 */
static void prv_handle_incoming_data_frame(const alfie_ble_service_proto_frame_header_t *header, const size_t len_bytes)
{
    if (len_bytes < sizeof(alfie_ble_service_proto_data_frame_t)) {
        LOG_WRN("Invalid data frame size.");
        return;
    }

    const alfie_ble_service_proto_data_frame_t *frame = (alfie_ble_service_proto_data_frame_t *)header;

    transport_buffer_t *buffer = transport_buffer_pool_get(&prv_inst.rx_pool, frame->seq_id, frame->total_size_bytes,
                                                           frame->frag_idx, frame->frag_total, NULL);

    if (buffer == NULL) {
        LOG_WRN("No available RX buffer.");
        return;
    }

    transport_buffer_write_ret_t ret = transport_buffer_write(buffer, frame->frag_idx, frame->payload,
                                                              len_bytes - sizeof(alfie_ble_service_proto_data_frame_t));

    switch (ret) {
        case TRANSPORT_BUFFER_WRITE_RET_SUCCESS:
        case TRANSPORT_BUFFER_WRITE_RET_DUPLICATE:
            prv_write_data_ack(frame);
            break;
        case TRANSPORT_BUFFER_WRITE_RET_COMPLETE:
            prv_write_data_ack(frame);
            prv_handle_complete_incoming_data_transaction(buffer);
            break;
        default:
            LOG_ERR("Error buffering incoming frame: %u", ret);
            break;
    }
}

/**
 * @brief Handling incoming data ACK
 *
 * @param header Pointer to header of incoming ACK
 * @param len_bytes Length of incoming buffer for validation
 */
static void prv_handle_incoming_data_ack(const alfie_ble_service_proto_frame_header_t *header, size_t len_bytes)
{
    if (len_bytes < sizeof(alfie_ble_service_proto_data_ack_frame_t)) {
        LOG_WRN("Invalid data ACK frame size.");
        return;
    }

    // FIXME: Actually validate data...
    k_sem_give(&prv_inst.ack_sem);
}

/*****************************************************************************
 * BLE Private Functions
 *****************************************************************************/

static ssize_t prv_on_data_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len,
                                 uint16_t offset, uint8_t flags)
{
    if (len < sizeof(alfie_ble_service_proto_frame_header_t)) {
        LOG_WRN("Invalid frame header.");
        return BT_GATT_ERR(BT_ATT_ERR_WRITE_REQ_REJECTED);
    }

    alfie_ble_service_proto_frame_header_t *header = (alfie_ble_service_proto_frame_header_t *)buf;

    switch (header->frame_type) {
        case ALFIE_BLE_SERVICE_PROTO_FRAME_TYPE_DATA:
            prv_handle_incoming_data_frame(header, len);
            break;
        case ALFIE_BLE_SERVICE_PROTO_FRAME_TYPE_DATA_ACK:
            prv_handle_incoming_data_ack(header, len);
            break;
        default:
            LOG_WRN("Unexpected frame type: 0x%02X", header->frame_type);
            return BT_GATT_ERR(BT_ATT_ERR_WRITE_REQ_REJECTED);
    }

    // FIXME: Right now, blindly returning length, but potentially missing out on BLE stack error handling, so will
    // want to revisit this.
    return len;
}

static void prv_on_data_char_config_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    // Currently do nothing...
}

static void prv_device_connected(struct bt_conn *conn, uint8_t err)
{
    if (err != 0) {
        return;
    }

    prv_inst.conn = bt_conn_ref(conn);
}

static void prv_device_disconnected(struct bt_conn *conn, uint8_t reason)
{
    bt_conn_unref(conn);
    prv_inst.conn = NULL;
}

/*****************************************************************************
 * Functions
 *****************************************************************************/

int alfie_ble_service_init(void)
{
    if (prv_inst.initialized == true) {
        return -EALREADY;
    }

    k_sem_init(&prv_inst.ack_sem, 0, 1);

    int ret = transport_buffer_pool_init(&prv_inst.rx_pool, &prv_rx_buffer_pool, ALFIE_BLE_SERVICE_NUM_RX_BUFFERS,
                                         ALFIE_BLE_SERVICE_MAX_RX_BUFFER_SIZE_BYTES, prv_inst.rx_buffers,
                                         sizeof(transport_buffer_t), ALFIE_BLE_SERVICE_RX_TIMEOUT_MS, NULL);
    if (ret != 0) {
        LOG_ERR("Failed to initialize transport buffer pool: %d", ret);
        return ret;
    }

    prv_inst.transport_api.register_rx_cb = alfie_ble_service_register_rx_callback;
    prv_inst.transport_api.write = alfie_ble_service_write;

    prv_inst.transport = (alfie_transport_t){.api = &prv_inst.transport_api, .type = ALFIE_TRANSPORT_TYPE_UPSTREAM};

    // TODO: Register with alfie router

    prv_inst.initialized = true;

    return 0;
}

int alfie_ble_service_write(const uint16_t dst_id, const void *data, size_t len_bytes)
{
    if (data == NULL || len_bytes == 0) {
        return -EINVAL;
    }

    if (len_bytes > ALFIE_BLE_SERVICE_MAX_RX_BUFFER_SIZE_BYTES) {
        return -ENOMEM;
    }

    if (prv_inst.initialized == false) {
        return -ENETDOWN;
    }

    if (prv_inst.conn == NULL) {
        return -ENOTCONN;
    }

    (void)dst_id;

    uint8_t buffer[ALFIE_BLE_SERVICE_PROTO_MAX_DATA_FRAME_PAYLOAD_SIZE_BYTES
                   + sizeof(alfie_ble_service_proto_data_frame_t)] = {0};

    alfie_ble_service_proto_data_frame_t *frame = (alfie_ble_service_proto_data_frame_t *)buffer;

    uint8_t frag_total = (len_bytes + ALFIE_BLE_SERVICE_PROTO_MAX_DATA_FRAME_PAYLOAD_SIZE_BYTES - 1)
                         / ALFIE_BLE_SERVICE_PROTO_MAX_DATA_FRAME_PAYLOAD_SIZE_BYTES;

    frame->header.version = 0;
    frame->header.frame_type = ALFIE_BLE_SERVICE_PROTO_FRAME_TYPE_DATA;
    frame->seq_id = sys_rand16_get();
    frame->frag_total = frag_total;
    frame->total_size_bytes = len_bytes;

    for (uint8_t i = 0; i < frag_total; i++) {
        frame->frag_idx = i;

        size_t payload_size_bytes = MIN((len_bytes - (ALFIE_BLE_SERVICE_PROTO_MAX_DATA_FRAME_PAYLOAD_SIZE_BYTES * i)),
                                        ALFIE_BLE_SERVICE_PROTO_MAX_DATA_FRAME_PAYLOAD_SIZE_BYTES);

        memcpy(frame->payload, &((uint8_t *)data)[i * ALFIE_BLE_SERVICE_PROTO_MAX_DATA_FRAME_PAYLOAD_SIZE_BYTES],
               payload_size_bytes);

        int ret = prv_write_data_frame(frame, (sizeof(alfie_ble_service_proto_data_frame_t) + payload_size_bytes));
        if (ret != 0) {
            LOG_ERR("Failed to write data frame: %d", ret);
            return ret;
        }
    }

    return 0;
}

int alfie_ble_service_register_rx_callback(alfie_transport_rx_callback_t callback)
{
    if (prv_inst.initialized == false) {
        return -ENETDOWN;
    }

    if (callback == NULL) {
        return -EINVAL;
    }

    prv_inst.transport_rx_callback = callback;
    return 0;
}
