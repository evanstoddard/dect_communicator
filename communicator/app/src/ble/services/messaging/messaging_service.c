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

#include <zephyr/random/random.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/conn.h>

#include "endpoints/endpoint_transport.h"
#include "endpoints/endpoint_id.h"
#include "messaging_service_protocol.h"

#include "utils/device_id.h"
#include "utils/transport_reassembly_buffer/transport_reassembly_buffer.h"

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
    MESSAGING_SERVICE_GATT_ATTR_SERVICE = 0,
    MESSAGING_SERVICE_GATT_ATTR_DATA_CHRC = 1,
    MESSAGING_SERVICE_GATT_ATTR_DATA = 2,
    MESSAGING_SERVICE_GATT_ATTR_DATA_CCC = 3,
} messaging_service_gatt_attr_idx_t;

static struct {
    bool initialized;

    struct bt_conn *conn;

    transport_reassem_ctx_t contexts[MESSAGING_SERVICE_MAX_RX_REASSEM_CONTEXTS];

    transport_reassem_buffer_t reassem_buf;

    struct k_sem ack_sem;

    endpoint_transport_rx_cb_t rx_callback;

} prv_inst;

NET_BUF_POOL_DEFINE(prv_rx_buf_pool, MESSAGING_SERVICE_MAX_RX_REASSEM_CONTEXTS,
                    MESSAGING_SERVICE_MAX_RX_REASSEM_BUF_SIZE_BYTES, sizeof(uintptr_t),
                    transport_reassem_net_buf_destroy);

/*****************************************************************************
 * BLE Bindings
 *****************************************************************************/

/**
 * @brief GATT write callback for the messaging data characteristic
 *
 * @param conn Pointer to the BLE connection
 * @param attr Pointer to the GATT attribute being written
 * @param buf Pointer to the write data buffer
 * @param len Length of the write data in bytes
 * @param offset Write offset
 * @param flags GATT write flags
 * @return Number of bytes consumed on success, BT_GATT_ERR on failure
 */
static ssize_t prv_on_data_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len,
                                 uint16_t offset, uint8_t flags);

/**
 * @brief Callback for CCC descriptor configuration changes on the data characteristic
 *
 * @param attr Pointer to the GATT attribute
 * @param value New CCC value (notification/indication enable flags)
 */
static void prv_on_data_char_config_changed(const struct bt_gatt_attr *attr, uint16_t value);

/**
 * @brief Callback called when a BLE device connects
 *
 * @param conn Pointer to the BLE connection
 * @param err Error code, 0 on success
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
 * @brief Match callback for transport reassembly buffer
 *
 * @param inst Pointer to reassembly buffer
 * @param ctx Pointer to reassembly context
 * @param input Pointer to input data (messaging_service_frame_header_t)
 * @return Returns true if context matches input
 */
static bool prv_reassem_match_callback(const struct transport_reassem_buffer_t *inst,
                                       const struct transport_reassem_ctx_t *ctx, const void *input)
{
    const messaging_service_frame_header_t *header = (const messaging_service_frame_header_t *)input;

    return ctx->seq_id == header->seq_id;
}

/**
 * @brief Context allocated callback for transport reassembly buffer
 *
 * @param reassem_buf Pointer to reassembly buffer
 * @param reassem_ctx Pointer to newly allocated reassembly context
 * @param input Pointer to input data (messaging_service_frame_header_t)
 */
static void prv_context_allocated_callback(const struct transport_reassem_buffer_t *reassem_buf,
                                           struct transport_reassem_ctx_t *reassem_ctx, void *input)
{
    const messaging_service_frame_header_t *header = (const messaging_service_frame_header_t *)input;

    reassem_ctx->seq_id = header->seq_id;
    reassem_ctx->frag_total = header->frag_total;
    reassem_ctx->frag_idx = 0;
    reassem_ctx->src_id = device_id();

    // FIXME: Set to actual transport ID
    reassem_ctx->transport_id = 0;
}

/**
 * @brief Handling incoming ACK
 *
 * @param header Pointer to header
 */
static void prv_handle_incoming_ack(const messaging_service_frame_header_t *header)
{
    // FIXME: Validate frame meta data.  Right now, this only supports handling one frame in flight
    k_sem_give(&prv_inst.ack_sem);
}

/**
 * @brief Validate that an incoming frame's header matches its reassembly context
 *
 * @param ctx Pointer to the reassembly context
 * @param frame Pointer to the incoming payload frame
 * @return true if the frame header is consistent with the context
 */
static bool prv_validate_incoming_frame(const transport_reassem_ctx_t *ctx,
                                        const messaging_service_payload_frame_t *frame)
{
    if (frame->header.frag_total != ctx->frag_total) {
        return false;
    }

    return true;
}

/**
 * @brief Send an ACK notification back to the BLE client for a received frame
 *
 * @param ctx Pointer to the reassembly context
 * @param frame Pointer to the frame being acknowledged
 */
static void prv_ack_incoming_frame(const transport_reassem_ctx_t *ctx, const messaging_service_payload_frame_t *frame)
{
    messaging_service_frame_header_t header = {
        .frame_type = MESSAGING_SERVICE_FRAME_TYPE_ACK,
        .frag_total = frame->header.frag_total,
        .frag_idx = frame->header.frag_idx,
        .seq_id = frame->header.seq_id,
        .version = 0,
    };

    int ret = bt_gatt_notify(prv_inst.conn, &attr_prv_message_service[MESSAGING_SERVICE_GATT_ATTR_DATA], &header,
                             sizeof(header));

    if (ret != 0) {
        LOG_ERR("Failed to ACK incoming frame: %d", ret);
    }
}

/**
 * @brief Handle a fully reassembled message received from the BLE client
 *
 * @param ctx Pointer to the completed reassembly context
 */
static void prv_handle_complete_message(transport_reassem_ctx_t *ctx)
{
    ctx->state = TRANSPORT_REASSEMBLY_CTX_STATE_IN_USE;

    if (prv_inst.rx_callback == NULL) {
        return;
    }

    prv_inst.rx_callback(ctx->src_id, ENDPOINT_ID_MESSAGING_ENDPOINT, ctx);
}

/**
 * @brief Process an incoming payload frame, writing it to the reassembly buffer
 *
 * @param ctx Pointer to the reassembly context
 * @param header Pointer to the frame header
 * @param payload_len Length of the frame payload in bytes (excluding header)
 * @return 0 on success, -EBADMSG on validation failure
 */
static int prv_handling_incoming_frame(transport_reassem_ctx_t *ctx, const messaging_service_frame_header_t *header,
                                       size_t payload_len)
{
    messaging_service_payload_frame_t *frame = (messaging_service_payload_frame_t *)header;

    if (prv_validate_incoming_frame(ctx, frame) == false) {
        LOG_WRN("Frame header doesn't match reassembly context metadata.");
        return -EBADMSG;
    }

    // If we've received a previous frame, chances are, our ACK was dropped, so simply re-ack
    if (frame->header.frag_idx < ctx->frag_idx) {
        prv_ack_incoming_frame(ctx, frame);
        ctx->last_rx_ms = k_uptime_get_32();
        return 0;
    }

    // We currently don't support out-of-order frames, so if we're receiving a frame from the future, silently
    // fail and let sender timeout or send correct fragment
    if (frame->header.frag_idx != ctx->frag_idx) {
        LOG_WRN("A time travelling fragment as arrived.");
        return -EBADMSG;
    }

    net_buf_add_mem(ctx->buffer, frame->payload, payload_len);

    ctx->frag_idx++;
    ctx->last_rx_ms = k_uptime_get_32();

    if (ctx->frag_idx == ctx->frag_total) {
        prv_handle_complete_message(ctx);
    }

    prv_ack_incoming_frame(ctx, frame);

    return 0;
}

/**
 * @brief Write a single frame to the BLE client with ACK-based retransmission
 *
 * @param frame Pointer to the payload frame to transmit
 * @param len_bytes Total size of the frame in bytes (header + payload)
 * @return 0 on success, negative errno on failure or timeout
 */
static int prv_write_frame(const messaging_service_payload_frame_t *frame, const size_t len_bytes)
{
    k_sem_reset(&prv_inst.ack_sem);

    int ret = 0;

    for (uint8_t i = 0; i < MESSAGING_SERVICE_MAX_TX_ATTEMPTS; i++) {
        ret = bt_gatt_notify(prv_inst.conn, &attr_prv_message_service[MESSAGING_SERVICE_GATT_ATTR_DATA], frame,
                             len_bytes);

        if (ret != 0) {
            LOG_ERR("Failed to dispatch notification to BLE stack: %d", ret);
            return ret;
        }

        ret = k_sem_take(&prv_inst.ack_sem, K_MSEC(MESSAGING_SERVICE_TX_ACK_TIMEOUT_MS));
        if (ret == 0) {
            return 0;
        }
    }

    return ret;
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

    transport_reassem_ctx_t *reassem_ctx =
        transport_reassem_buffer_get_context(&prv_inst.reassem_buf, header, header->frag_idx);
    if (reassem_ctx == NULL) {
        LOG_WRN("Unable to find or allocate reassembly context for incoming frame.");
        return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
    }

    int ret = prv_handling_incoming_frame(reassem_ctx, header, len - sizeof(messaging_service_frame_header_t));

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

int messaging_service_init(void)
{
    if (prv_inst.initialized == true) {
        return -EALREADY;
    }

    k_sem_init(&prv_inst.ack_sem, 0, 1);

    transport_reassem_buffer_params_t reassem_params = {.num_buffers = MESSAGING_SERVICE_MAX_RX_REASSEM_CONTEXTS,
                                                        .buffer_size_bytes =
                                                            MESSAGING_SERVICE_MAX_RX_REASSEM_BUF_SIZE_BYTES,
                                                        .context_size_bytes = sizeof(transport_reassem_ctx_t),
                                                        .rx_timeout_ms = MESSAGING_SERVICE_RX_TIMEOUT_MS,
                                                        .match_cb = prv_reassem_match_callback,
                                                        .ctx_allocated_cb = prv_context_allocated_callback};

    int ret =
        transport_reassem_buffer_init(&prv_inst.reassem_buf, &prv_rx_buf_pool, prv_inst.contexts, &reassem_params);
    if (ret != 0) {
        LOG_ERR("Failed to initialize reassembly buffer: %d", ret);
        return ret;
    }

    static endpoint_transport_t transport = {
        .api = {.transport_register_rx_cb = messaging_service_register_rx_callback,
                .transport_write = messaging_service_write},
        .medium = ENDPOINT_TRANSPORT_MEDIUM_BLE,
        .type = ENDPOINT_TRANSPORT_TYPE_UPSTREAM,
    };

    endpoint_router_register_transport(&transport);

    prv_inst.initialized = true;

    return 0;
}

int messaging_service_write(uint16_t dst_id, uint8_t endpoint_id, const void *data, const size_t len_bytes)
{
    if (data == NULL || len_bytes == 0) {
        return -EINVAL;
    }

    if (endpoint_id != ENDPOINT_ID_MESSAGING_ENDPOINT) {
        LOG_ERR("Invalid endpoint ID.");
        return -ENOTSUP;
    }

    uint16_t seq_id = sys_rand16_get();

    uint8_t frag_total = len_bytes / MESSAGING_SERVICE_MAX_PAYLOAD_SIZE_BYTES;
    if ((len_bytes % MESSAGING_SERVICE_MAX_PAYLOAD_SIZE_BYTES) > 0) {
        frag_total++;
    }

    for (uint8_t i = 0; i < frag_total; i++) {
        messaging_service_payload_frame_t frame = {.header.version = 0,
                                                   .header.frame_type = MESSAGING_SERVICE_FRAME_TYPE_PAYLOAD,
                                                   .header.frag_total = frag_total,
                                                   .header.frag_idx = i,
                                                   .header.seq_id = seq_id};

        size_t frag_size_bytes = len_bytes - (i * MESSAGING_SERVICE_MAX_PAYLOAD_SIZE_BYTES);
        if (frag_size_bytes > MESSAGING_SERVICE_MAX_PAYLOAD_SIZE_BYTES) {
            frag_size_bytes = MESSAGING_SERVICE_MAX_PAYLOAD_SIZE_BYTES;
        }

        memcpy(frame.payload, &((uint8_t *)data)[i * MESSAGING_SERVICE_MAX_PAYLOAD_SIZE_BYTES], frag_size_bytes);
        int ret = prv_write_frame(&frame, sizeof(messaging_service_frame_header_t) + frag_size_bytes);

        if (ret != 0) {
            LOG_ERR("Failed to write frame: %d", ret);
            return ret;
        }
    }

    LOG_INF("Successfully forwarded message!");

    return 0;
}

int messaging_service_register_rx_callback(endpoint_transport_rx_cb_t callback)
{
    if (callback == NULL) {
        return -EINVAL;
    }

    prv_inst.rx_callback = callback;

    return 0;
}

