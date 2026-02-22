/*
 * Copyright (C) Ovyl
 */

/**
 * @file messaging_endpoint.c
 * @author Evan Stoddard
 * @brief
 */

#include "messaging_endpoint.h"

#include <stdbool.h>
#include <stddef.h>

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

#include "messaging_endpoint_protocol.h"

#include "endpoints/endpoint_id.h"
#include "endpoints/endpoint_router.h"

/*****************************************************************************
 * Definitions
 *****************************************************************************/

LOG_MODULE_REGISTER(messaging_endpoint);

#define MESSAGING_ENDPOINT_STACK_SIZE_BYTES (1024U)

#define MESSAGING_ENDPOINT_THREAD_PRIORITY (8)

/*****************************************************************************
 * Variables
 *****************************************************************************/

static struct {
    bool initialized;

    struct k_thread thread;
    k_tid_t thread_id;

    struct k_fifo from_upstream_rx_fifo;
    struct k_fifo from_downstream_rx_fifo;
} prv_inst;

K_THREAD_STACK_DEFINE(prv_messaging_endpoint_thread_stack, MESSAGING_ENDPOINT_STACK_SIZE_BYTES);

/*****************************************************************************
 * Private Functions
 *****************************************************************************/

/**
 * @brief Callback for messages received from upstream (BLE/phone side)
 *
 * @param src_id Source device ID of the sender
 * @param ctx Pointer to the reassembly context containing the received data
 * @return 0 on success
 */
static int prv_on_from_upstream(uint16_t src_id, transport_reassem_ctx_t *ctx)
{
    struct net_buf *ref __unused = net_buf_ref(ctx->buffer);
    k_fifo_put(&prv_inst.from_upstream_rx_fifo, ctx);

    return 0;
}

/**
 * @brief Callback for messages received from downstream (DECT NR+ radio side)
 *
 * @param src_id Source device ID of the sender
 * @param ctx Pointer to the reassembly context containing the received data
 * @return 0 on success
 */
static int prv_on_from_downstream(uint16_t src_id, transport_reassem_ctx_t *ctx)
{
    struct net_buf *ref __unused = net_buf_ref(ctx->buffer);
    k_fifo_put(&prv_inst.from_downstream_rx_fifo, ctx);

    return 0;
}

/**
 * @brief Process a queued message from upstream and forward to downstream (DECT)
 */
static void prv_handle_rx_from_upstream(void)
{
    transport_reassem_ctx_t *ctx = k_fifo_get(&prv_inst.from_upstream_rx_fifo, K_NO_WAIT);
    if (ctx == NULL) {
        return;
    }

    LOG_HEXDUMP_INF(ctx->buffer->data, ctx->buffer->len, "Received message from upstream:");

    if (ctx->buffer->len < sizeof(messaging_endpoint_header_t)) {
        LOG_WRN("Message too small.");
        goto release;
    }

    messaging_endpoint_header_t *header = (messaging_endpoint_header_t *)ctx->buffer->data;

    if (header->msg_type == MESSAGING_ENDPOINT_MSG_TYPE_TEXT) {
        if (ctx->buffer->len
            < sizeof(messaging_endpoint_header_t) + sizeof(messaging_endpoint_text_message_metadata_t)) {
            LOG_WRN("Text message too small.");
            goto release;
        }

        messaging_endpoint_text_message_t *msg = (messaging_endpoint_text_message_t *)ctx->buffer->data;

        endpoint_router_write_to_downstream(msg->meta.dst_id, ENDPOINT_ID_MESSAGING_ENDPOINT, ctx->buffer->data,
                                            ctx->buffer->len);
    }

release:
    net_buf_unref(ctx->buffer);
}

/**
 * @brief Process a queued message from downstream and forward to upstream (BLE/phone)
 */
static void prv_handle_rx_from_downstream(void)
{
    transport_reassem_ctx_t *ctx = k_fifo_get(&prv_inst.from_downstream_rx_fifo, K_NO_WAIT);
    if (ctx == NULL) {
        return;
    }

    LOG_HEXDUMP_INF(ctx->buffer->data, ctx->buffer->len, "Received message from downstream:");

    // Forward to upstream (BLE → phone)
    endpoint_router_write_to_upstream(ctx->dst_id, ENDPOINT_ID_MESSAGING_ENDPOINT, ctx->buffer->data, ctx->buffer->len);

    net_buf_unref(ctx->buffer);
}

/**
 * @brief Messaging endpoint processing thread
 *
 * @param arg1 Unused
 * @param arg2 Unused
 * @param arg3 Unused
 */
static void prv_thread(void *arg1, void *arg2, void *arg3)
{
    static struct k_poll_event events[] = {
        K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_FIFO_DATA_AVAILABLE, K_POLL_MODE_NOTIFY_ONLY,
                                        &prv_inst.from_upstream_rx_fifo, 0),
        K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_FIFO_DATA_AVAILABLE, K_POLL_MODE_NOTIFY_ONLY,
                                        &prv_inst.from_downstream_rx_fifo, 0),
    };

    while (true) {
        k_poll(events, ARRAY_SIZE(events), K_FOREVER);

        if (events[0].state == K_POLL_STATE_FIFO_DATA_AVAILABLE) {
            prv_handle_rx_from_upstream();
        }

        if (events[1].state == K_POLL_STATE_FIFO_DATA_AVAILABLE) {
            prv_handle_rx_from_downstream();
        }

        // Reset states
        events[0].state = K_POLL_STATE_NOT_READY;
        events[1].state = K_POLL_STATE_NOT_READY;
    }
}

/*****************************************************************************
 * Functions
 *****************************************************************************/

int messaging_endpoint_init(void)
{
    if (prv_inst.initialized == true) {
        return -EALREADY;
    }

    k_fifo_init(&prv_inst.from_upstream_rx_fifo);
    k_fifo_init(&prv_inst.from_downstream_rx_fifo);

    prv_inst.thread_id = k_thread_create(&prv_inst.thread, prv_messaging_endpoint_thread_stack,
                                         K_THREAD_STACK_SIZEOF(prv_messaging_endpoint_thread_stack), prv_thread, NULL,
                                         NULL, NULL, MESSAGING_ENDPOINT_THREAD_PRIORITY, 0, K_NO_WAIT);

    static endpoint_t endpoint = {
        .endpoint_id = ENDPOINT_ID_MESSAGING_ENDPOINT,
        .on_from_upstream = prv_on_from_upstream,
        .on_from_downstream = prv_on_from_downstream,
    };

    endpoint_router_register_endpoint(&endpoint);

    prv_inst.initialized = true;

    return 0;
}
