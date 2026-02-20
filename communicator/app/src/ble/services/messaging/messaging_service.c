/*
 * Copyright (C) Ovyl
 */

/**
 * @file messaging_service.c
 * @author Evan Stoddard
 * @brief
 */

#include "messaging_service.h"

#include <stddef.h>
#include <stdint.h>

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>

#include <zephyr/net_buf.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/conn.h>

#include "messaging_service_protocol.h"

/*****************************************************************************
 * Definitions
 *****************************************************************************/

LOG_MODULE_REGISTER(messaging_service);

#define MESSAGING_SERVICE_UUID_VAL           BT_UUID_128_ENCODE(0xc1534fa3, 0x5211, 0x4e32, 0xa176, 0xd1af04513305)
#define MESSAGING_SERVICE_DATA_CHAR_UUID_VAL BT_UUID_128_ENCODE(0xc1534fa4, 0x5211, 0x4e32, 0xa176, 0xd1af04513305)

static struct bt_uuid_128 messaging_service_uuid = BT_UUID_INIT_128(MESSAGING_SERVICE_UUID_VAL);
static struct bt_uuid_128 messaging_service_data_char_uuid = BT_UUID_INIT_128(MESSAGING_SERVICE_DATA_CHAR_UUID_VAL);

#define MESSAGING_SERVICE_UUID           ((const struct bt_uuid *)&messaging_service_uuid)
#define MESSAGING_SERVICE_DATA_CHAR_UUID ((const struct bt_uuid *)&messaging_service_data_char_uuid)

#define MESSAGING_SERVICE_MAX_RX_REASSEM_CONTEXTS       (4U)
#define MESSAGING_SERVICE_MAX_RX_REASSEM_BUF_SIZE_BYTES (512U)

#define MESSAGING_SERVICE_MAX_TX_ATTEMPTS (4U)

#define MESSAGING_SERVICE_RX_TIMEOUT_MS     (10000U)
#define MESSAGING_SERVICE_TX_ACK_TIMEOUT_MS (MESSAGING_SERVICE_RX_TIMEOUT_MS / 4)

/*****************************************************************************
 * Variables
 *****************************************************************************/

typedef enum {
    MESSAGING_SERVICE_REASSEM_STATE_FREE,
    MESSAGING_SERVICE_REASSEM_STATE_RECEIVING,
    MESSAGING_SERVICE_REASSEM_WITH_ENDPOINT,
} messaging_service_reassem_state_t;

/**
 * @typedef messaging_service_reassem_ctx_t
 * @brief Reassemply context definition
 *
 */
typedef struct messaging_service_reassem_ctx_t {
    volatile messaging_service_reassem_state_t state;
    uint16_t seq_id;
    uint8_t frag_total;
    uint8_t frag_idx;
    uint32_t last_rx_ms;

    struct net_buf *buf;
} messaging_service_reassem_ctx_t;

static struct {
    struct bt_conn *conn;

    messaging_service_reassem_ctx_t contexts[MESSAGING_SERVICE_MAX_RX_REASSEM_CONTEXTS];

    struct k_sem ack_sem;
} prv_inst;

NET_BUF_POOL_DEFINE(prv_rx_buf_pool, MESSAGING_SERVICE_MAX_RX_REASSEM_CONTEXTS,
                    MESSAGING_SERVICE_MAX_RX_REASSEM_BUF_SIZE_BYTES, sizeof(uintptr_t), NULL);

/*****************************************************************************
 * BLE Bindings
 *****************************************************************************/

/**
 * @brief [TODO:description]
 *
 * @param conn [TODO:parameter]
 * @param attr [TODO:parameter]
 * @param buf [TODO:parameter]
 * @param len [TODO:parameter]
 * @param offset [TODO:parameter]
 * @param flags [TODO:parameter]
 * @return [TODO:return]
 */
static ssize_t prv_on_data_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len,
                                 uint16_t offset, uint8_t flags);

/**
 * @brief [TODO:description]
 *
 * @param attr [TODO:parameter]
 * @param value [TODO:parameter]
 */
static void prv_on_data_char_config_changed(const struct bt_gatt_attr *attr, uint16_t value);

/**
 * @brief [TODO:description]
 *
 * @param conn [TODO:parameter]
 * @param err [TODO:parameter]
 */
static void prv_device_connected(struct bt_conn *conn, uint8_t err);

/**
 * @brief Callback called when device disconnected
 *
 * @param conn Pointer to connection
 * @param reason Reason for disconnection
 */
static void prv_device_disconnected(struct bt_conn *conn, uint8_t reason);

BT_GATT_SERVICE_DEFINE(prv_message_service, BT_GATT_PRIMARY_SERVICE(MESSAGING_SERVICE_UUID),
                       BT_GATT_CHARACTERISTIC(MESSAGING_SERVICE_DATA_CHAR_UUID,
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
 * @brief Get (or allocate) context for incoming frame
 *
 * @param frame Pointer to incoming frame
 * @return Returns pointer to reassembly context
 */
static messaging_service_reassem_ctx_t *prv_get_reassembly_context(const messaging_service_frame_header_t *header)
{
    messaging_service_reassem_ctx_t *ctx = NULL;

    // Perform some garbage collection on potential expired contexts
    for (size_t i = 0; i < MESSAGING_SERVICE_MAX_RX_REASSEM_CONTEXTS; i++) {
        ctx = &prv_inst.contexts[i];

        if (ctx->state != MESSAGING_SERVICE_REASSEM_STATE_RECEIVING) {
            continue;
        }

        uint32_t delta_ms = k_uptime_get_32() - ctx->last_rx_ms;

        if (delta_ms >= MESSAGING_SERVICE_RX_TIMEOUT_MS) {
            ctx->state = MESSAGING_SERVICE_REASSEM_STATE_FREE;
            net_buf_unref(ctx->buf);
        }
    }

    // Now check if there is a receiving context with a matching sequence ID
    for (size_t i = 0; i < MESSAGING_SERVICE_MAX_RX_REASSEM_CONTEXTS; i++) {
        ctx = &prv_inst.contexts[i];

        if (ctx->state != MESSAGING_SERVICE_REASSEM_STATE_RECEIVING) {
            continue;
        }

        if (ctx->seq_id == header->seq_id) {
            return ctx;
        }
    }

    // Handle case there's no context matching sequence ID and we're not receiving a brand new message
    if (header->frag_idx != 0) {
        return NULL;
    }

    // If we've reach this point, it's a brand new frame and we need to allocate a new context
    for (size_t i = 0; i < MESSAGING_SERVICE_MAX_RX_REASSEM_CONTEXTS; i++) {
        ctx = &prv_inst.contexts[i];

        if (ctx->state != MESSAGING_SERVICE_REASSEM_STATE_FREE) {
            continue;
        }
        ctx->frag_idx = 0;
        ctx->frag_total = header->frag_total;
        ctx->seq_id = header->seq_id;

        ctx->buf = net_buf_alloc(&prv_rx_buf_pool, K_NO_WAIT);
        if (ctx->buf == NULL) {
            return NULL;
        }

        ctx->last_rx_ms = k_uptime_get_32();
        ctx->state = MESSAGING_SERVICE_REASSEM_STATE_RECEIVING;

        return ctx;
    }

    // No available contexts :(
    return NULL;
}

/**
 * @brief Handling incoming ACK
 *
 * @param header Pointer to header
 */
static void prv_handle_incoming_ack(const messaging_service_frame_header_t *header)
{
}

/**
 * @brief [TODO:description]
 *
 * @param ctx [TODO:parameter]
 * @param header [TODO:parameter]
 * @param payload_len [TODO parameter]
 * @return [TODO:return]
 */
static int prv_handling_incoming_frame(const messaging_service_reassem_ctx_t *ctx,
                                       const messaging_service_frame_header_t *header, size_t payload_len)
{
    return -ENOTSUP;
}

/*****************************************************************************
 * Private BLE Bindings Functions
 *****************************************************************************/

static ssize_t prv_on_data_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len,
                                 uint16_t offset, uint8_t flags)
{
    messaging_service_frame_header_t *header = (messaging_service_frame_header_t *)buf;

    if (header->frame_type == MESSAGING_SERVICE_FRAME_TYPE_ACK) {
        prv_handle_incoming_ack(header);
        return len;
    }

    messaging_service_reassem_ctx_t *reassem_ctx = prv_get_reassembly_context(header);
    if (reassem_ctx == NULL) {
        LOG_WRN("Unable to find or allocate reassembly context for incoming frame.");
        return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
    }

    int ret = prv_handling_incoming_frame(reassem_ctx, header, len);

    if (ret != 0) {
        LOG_ERR("Failed to handling incoming frame: %d", ret);
        return BT_GATT_ERR(BT_ATT_ERR_WRITE_REQ_REJECTED);
    }

    return len;
}

static void prv_on_data_char_config_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    LOG_INF("Data characteristic changed.");
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
