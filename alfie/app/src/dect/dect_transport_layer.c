/*
 * Copyright (C) Evan Stoddard
 */

/**
 * @file dect_transport_layer.c
 * @author Evan Stoddard
 * @brief Module to provide reliable data transfer over DECT
 */

#include "dect_transport_layer.h"

#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include <zephyr/logging/log.h>

#include <zephyr/random/random.h>

#include <zephyr/sys/util.h>

#include "dect/dect_link_layer_private.h"
#include "dect/dect_transport_layer_private.h"
#include "dect_link_layer.h"

#include "alfie/alfie_transport.h"
#include "utils/transport_buffer.h"

/*****************************************************************************
 * Definitions
 *****************************************************************************/

LOG_MODULE_REGISTER(dect_transport_layer);

#define DECT_TRANSPORT_LAYER_NUM_RX_BUFFERS (4U)

#define DECT_TRANSPORT_LAYER_MAX_RX_BUFFER_SIZE_BYTES (512U)

#define DECT_TRANSPORT_LAYER_RX_TIMEOUT_MS (10000U)

#define DECT_TRANSPORT_LAYER_TX_ATTEMPTS (4U)

#define DECT_TRANSPORT_LAYER_ACK_TIMEOUT_MS (DECT_TRANSPORT_LAYER_RX_TIMEOUT_MS / DECT_TRANSPORT_LAYER_TX_ATTEMPTS)

#define DECT_TRANSPORT_LAYER_DATA_FRAME_MAX_PAYLOAD_SIZE_BYTES                                                         \
    (DECT_LINK_LAYER_MAX_PAYLOAD_SIZE_BYTES - sizeof(dect_transport_layer_data_frame_t))

/*****************************************************************************
 * Variables
 *****************************************************************************/

/**
 * @typedef dect_transport_buffer_t
 * @brief Augmented transport buffer
 *
 */
typedef struct dect_transport_buffer_t {
    transport_buffer_t base;
    uint32_t src_id;
} dect_transport_buffer_t;

/**
 * @brief Private instance
 */
static struct {
    bool initialized;

    transport_buffer_pool_t rx_pool;
    dect_transport_buffer_t rx_buffers[DECT_TRANSPORT_LAYER_NUM_RX_BUFFERS];
    transport_buffer_pool_api_t rx_buffer_api;

    alfie_transport_rx_callback_t transport_rx_callback;

    alfie_transport_t transport;
    alfie_transport_api_t transport_api;

    // TODO: Limits to one transmission at a time.  Will move to outbound transport buffer pool
    struct k_sem ack_sem;
} prv_inst;

NET_BUF_POOL_DEFINE(prv_rx_buffer_pool, DECT_TRANSPORT_LAYER_NUM_RX_BUFFERS,
                    DECT_TRANSPORT_LAYER_MAX_RX_BUFFER_SIZE_BYTES, 0, NULL);

/*****************************************************************************
 * Private Functions
 *****************************************************************************/

/**
 * @brief Write ACK of incoming data frame
 *
 * @param dst_id Destination ID to send ACK to
 * @param frame Pointer to received data frame
 */
static void prv_write_data_ack(uint32_t dst_id, const dect_transport_layer_data_frame_t *frame)
{
    dect_transport_layer_data_frame_ack_t ack_frame = {
        .header.version = 0,
        .header.frame_type = DECT_TRANSPORT_LAYER_FRAME_TYPE_DATA_ACK,
        .frag_idx = frame->frag_idx,
        .seq_id = frame->seq_id,
    };

    int ret = dect_link_layer_write(dst_id, &ack_frame, sizeof(ack_frame));

    if (ret != 0) {
        LOG_ERR("Failed to write ACK: %d", ret);
        return;
    }
}

/**
 * @brief Handle receiving complete data transaction
 *
 * @param buffer Pointer to buffer
 */
static void prv_handle_complete_incoming_data_transaction(dect_transport_buffer_t *buffer)
{
    if (prv_inst.transport_rx_callback != NULL) {
        prv_inst.transport_rx_callback(&prv_inst.transport, (transport_buffer_t *)buffer);
    }

    // Release the transport layer's reference from the initial net_buf_alloc.
    // If a callback was invoked, the consumer (router) will have taken its own
    // reference, so this drops the allocation ref. If no callback, this frees
    // the buffer back to the pool.
    transport_buffer_unref((transport_buffer_t *)buffer);
}

/**
 * @brief Handle incoming data frame
 *
 * @param src_id Source ID of originating message
 * @param header Pointer to frame header
 * @param len_bytes Total length of incoming frame
 */
static void prv_handle_data_frame(uint32_t src_id, const dect_transport_layer_frame_header_t *header, size_t len_bytes)
{
    dect_transport_layer_data_frame_t *data_frame = (dect_transport_layer_data_frame_t *)header;

    if (len_bytes < sizeof(dect_transport_layer_data_frame_t)) {
        LOG_WRN("Invalid data frame size.");
        return;
    }

    dect_transport_buffer_t *buffer = (dect_transport_buffer_t *)transport_buffer_pool_get(
        &prv_inst.rx_pool, data_frame->seq_id, data_frame->total_size_bytes, data_frame->frag_idx,
        data_frame->frag_total, &src_id);

    if (buffer == NULL) {
        LOG_WRN("No available RX buffer.");
        return;
    }

    buffer->src_id = src_id;

    transport_buffer_write_ret_t ret =
        transport_buffer_write((transport_buffer_t *)buffer, data_frame->frag_idx, data_frame->payload,
                               len_bytes - sizeof(dect_transport_layer_data_frame_t));

    switch (ret) {
        case TRANSPORT_BUFFER_WRITE_RET_SUCCESS:
        case TRANSPORT_BUFFER_WRITE_RET_DUPLICATE:
            prv_write_data_ack(src_id, data_frame);
            break;
        case TRANSPORT_BUFFER_WRITE_RET_COMPLETE:
            prv_write_data_ack(src_id, data_frame);
            prv_handle_complete_incoming_data_transaction(buffer);
            break;
        default:
            LOG_WRN("Error buffering incoming frame: %u", ret);
            break;
    }
}

/**
 * @brief Handle incoming data ACK frame
 *
 * @param header Pointer to frame header
 * @param len_bytes Total length of incoming frame
 */
static void prv_handle_data_ack_frame(const dect_transport_layer_frame_header_t *header, const size_t len_bytes)
{
    // FIXME: Actually validate ACK against outbound transmission
    (void)header;
    (void)len_bytes;

    k_sem_give(&prv_inst.ack_sem);
}

/**
 * @brief Handle incoming data from lower data layer
 *
 * @param src_id Source ID of incoming data
 * @param buf Pointer to incoming data
 * @param len_bytes Length of incoming data
 */
static void prv_on_data_layer_rx(const uint32_t src_id, const void *buf, const size_t len_bytes)
{
    if (len_bytes < sizeof(dect_transport_layer_frame_header_t)) {
        LOG_WRN("Invalid transport layer frame header.");
        return;
    }

    dect_transport_layer_frame_header_t *header = (dect_transport_layer_frame_header_t *)buf;

    switch (header->frame_type) {
        case DECT_TRANSPORT_LAYER_FRAME_TYPE_DATA:
            prv_handle_data_frame(src_id, header, len_bytes);
            break;
        case DECT_TRANSPORT_LAYER_FRAME_TYPE_DATA_ACK:
            prv_handle_data_ack_frame(header, len_bytes);
            break;
        default:
            LOG_WRN("Unexpected frame type: 0x%02X", header->frame_type);
            break;
    }
}

/**
 * @brief Handles writing data frame with retries and awaits ACK
 *
 * @param dst_id Destination ID for frame
 * @param frame Pointer to frame
 * @param len_bytes Length of frame, including header and payload
 * @return Returns 0 on success or negative errno on failure
 */
static int prv_write_data_frame(const uint32_t dst_id, dect_transport_layer_data_frame_t *frame, size_t len_bytes)
{
    int ret = 0;

    for (uint8_t i = 0; i < DECT_TRANSPORT_LAYER_TX_ATTEMPTS; i++) {
        k_sem_reset(&prv_inst.ack_sem);

        ret = dect_link_layer_write(dst_id, frame, len_bytes);

        if (ret != 0) {
            return ret;
        }

        ret = k_sem_take(&prv_inst.ack_sem, K_MSEC(DECT_TRANSPORT_LAYER_ACK_TIMEOUT_MS));
        if (ret == 0) {
            return 0;
        }
    }

    return ret;
}

/**
 * @brief Callback from transport buffer when iterating through buffers trying to match buffer to input parameters
 *
 * @param buffer Pointer to buffer
 * @param additional_params Pointer to additional parameters
 * @return Returns true if there's a match
 */
static bool prv_additional_query_matches(const transport_buffer_t *buffer, const void *additional_params)
{
    const dect_transport_buffer_t *buf = (const dect_transport_buffer_t *)buffer;
    const uint32_t *src_id = (const uint32_t *)additional_params;

    return (*src_id == buf->src_id);
}

/*****************************************************************************
 * Functions
 *****************************************************************************/

int dect_transport_layer_init(void)
{
    if (prv_inst.initialized == true) {
        return -EALREADY;
    }

    prv_inst.rx_buffer_api.additional_query_cb = prv_additional_query_matches;
    int ret = transport_buffer_pool_init(&prv_inst.rx_pool, &prv_rx_buffer_pool, DECT_TRANSPORT_LAYER_NUM_RX_BUFFERS,
                                         DECT_TRANSPORT_LAYER_MAX_RX_BUFFER_SIZE_BYTES, prv_inst.rx_buffers,
                                         sizeof(dect_transport_buffer_t), DECT_TRANSPORT_LAYER_RX_TIMEOUT_MS,
                                         &prv_inst.rx_buffer_api);
    if (ret != 0) {
        LOG_ERR("Failed to initialize RX buffer pool: %d", ret);
        return ret;
    }

    (void)dect_link_layer_register_rx_callback(prv_on_data_layer_rx);

    // FIXME: Ideally, this part is done outside this module.  This is the only "tight-coupling" between this lower
    // level layer and the higher level alfie layer
    prv_inst.transport_api = (alfie_transport_api_t){.register_rx_cb = dect_transport_layer_register_rx_callback,
                                                    .write = dect_transport_layer_write};
    prv_inst.transport = (alfie_transport_t){.type = ALFIE_TRANSPORT_TYPE_DOWNSTREAM, .api = &prv_inst.transport_api};
    // TODO: Register transport with higher layer

    k_sem_init(&prv_inst.ack_sem, 0, 1);

    prv_inst.initialized = true;

    return 0;
}

int dect_transport_layer_write(const uint32_t dst_id, const void *data, size_t len_bytes)
{
    if (prv_inst.initialized == false) {
        return -ENOLINK;
    }

    if (data == NULL || len_bytes == 0) {
        return -EINVAL;
    }

    if (len_bytes > DECT_TRANSPORT_LAYER_MAX_RX_BUFFER_SIZE_BYTES) {
        return -ENOMEM;
    }

    uint8_t tx_buffer[DECT_LINK_LAYER_MAX_PAYLOAD_SIZE_BYTES] = {0};

    uint8_t frag_total = (len_bytes + DECT_TRANSPORT_LAYER_DATA_FRAME_MAX_PAYLOAD_SIZE_BYTES - 1)
                         / DECT_TRANSPORT_LAYER_DATA_FRAME_MAX_PAYLOAD_SIZE_BYTES;

    uint16_t seq_id = sys_rand16_get();

    dect_transport_layer_data_frame_t *frame = (dect_transport_layer_data_frame_t *)tx_buffer;

    frame->header.version = 0;
    frame->header.frame_type = DECT_TRANSPORT_LAYER_FRAME_TYPE_DATA;
    frame->seq_id = seq_id;
    frame->frag_total = frag_total;
    frame->total_size_bytes = len_bytes;

    for (uint8_t i = 0; i < frag_total; i++) {
        frame->frag_idx = i;

        size_t payload_size_bytes = MIN((len_bytes - (DECT_TRANSPORT_LAYER_DATA_FRAME_MAX_PAYLOAD_SIZE_BYTES * i)),
                                        DECT_TRANSPORT_LAYER_DATA_FRAME_MAX_PAYLOAD_SIZE_BYTES);

        memcpy(frame->payload, &((uint8_t *)data)[i * DECT_TRANSPORT_LAYER_DATA_FRAME_MAX_PAYLOAD_SIZE_BYTES],
               payload_size_bytes);

        int ret = prv_write_data_frame(dst_id, frame, (sizeof(dect_transport_layer_data_frame_t) + payload_size_bytes));
        if (ret != 0) {
            LOG_ERR("Failed to write data frame: %d", ret);
            return ret;
        }
    }

    return 0;
}

int dect_transport_layer_register_rx_callback(alfie_transport_rx_callback_t callback)
{
    if (prv_inst.initialized == false) {
        return -ENODEV;
    }

    if (callback == NULL) {
        return -EINVAL;
    }

    prv_inst.transport_rx_callback = callback;
    return 0;
}

alfie_transport_t *dect_transport_layer_get_transport(void)
{
    if (prv_inst.initialized == false) {
        return NULL;
    }

    return &prv_inst.transport;
}
