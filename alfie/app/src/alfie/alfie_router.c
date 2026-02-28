/*
 * Copyright (C) Evan Stoddard
 */

/**
 * @file alfie_router.c
 * @author Evan Stoddard
 * @brief
 */

#include "alfie_router.h"
#include "alfie/alfie_protocol.h"
#include "alfie/alfie_transport.h"

#include <stdbool.h>

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>

#include <zephyr/sys/slist.h>

/*****************************************************************************
 * Definitions
 *****************************************************************************/

LOG_MODULE_REGISTER(alfie_router);

#define ALFIE_ROUTER_THREAD_PRIORITY 5

#define ALFIE_ROUTER_STACK_SIZE_BYTES (1024U)

/*****************************************************************************
 * Variables
 *****************************************************************************/

K_THREAD_STACK_DEFINE(prv_stack, ALFIE_ROUTER_STACK_SIZE_BYTES);

static struct {
    bool initialized;

    sys_slist_t endpoints;
    sys_slist_t transports;

    struct k_fifo from_upstream_fifo;
    struct k_fifo from_downstream_fifo;

    k_tid_t thread_id;
    struct k_thread thread;
} prv_inst;

/*****************************************************************************
 * Private Functions
 *****************************************************************************/

/**
 * @brief Handle incoming frame from transport layer
 *
 * @param transport Pointer to transport instance
 * @param buffer Pointer to buffer
 */
/**
 * @brief Notify matching endpoint of incoming frame
 *
 * @param header Pointer to alfie protocol header
 * @param buffer Pointer to buffer
 */
static void prv_notify_endpoint(const alfie_proto_header_t *header, transport_buffer_t *buffer)
{
    sys_snode_t *node;
    SYS_SLIST_FOR_EACH_NODE(&prv_inst.endpoints, node) {
        alfie_endpoint_t *endpoint = (alfie_endpoint_t *)node;
        if (endpoint->endpoint_id == header->endpoint_id) {
            endpoint->api.on_frame_rx(buffer);
            return;
        }
    }

    LOG_WRN("No endpoint registered for endpoint_id: 0x%02X", header->endpoint_id);
}

/**
 * @brief Forward buffer to all downstream transports and notify matching endpoint
 *
 * @param buffer Pointer to buffer
 */
static void prv_forward_to_downstream(transport_buffer_t *buffer)
{
    alfie_proto_header_t *header = (alfie_proto_header_t *)buffer->buffer->data;

    sys_snode_t *node;
    SYS_SLIST_FOR_EACH_NODE(&prv_inst.transports, node) {
        alfie_transport_t *transport = (alfie_transport_t *)node;
        if (transport->type == ALFIE_TRANSPORT_TYPE_DOWNSTREAM) {
            transport->api->write(header->dst_id, buffer->buffer->data, buffer->total_size_bytes);
        }
    }

    prv_notify_endpoint(header, buffer);
    transport_buffer_unref(buffer);
}

/**
 * @brief Forward buffer to all upstream transports and notify matching endpoint
 *
 * @param buffer Pointer to buffer
 */
static void prv_forward_to_upstream(transport_buffer_t *buffer)
{
    alfie_proto_header_t *header = (alfie_proto_header_t *)buffer->buffer->data;

    sys_snode_t *node;
    SYS_SLIST_FOR_EACH_NODE(&prv_inst.transports, node) {
        alfie_transport_t *transport = (alfie_transport_t *)node;
        if (transport->type == ALFIE_TRANSPORT_TYPE_UPSTREAM) {
            transport->api->write(header->dst_id, buffer->buffer->data, buffer->total_size_bytes);
        }
    }

    prv_notify_endpoint(header, buffer);
    transport_buffer_unref(buffer);
}

/**
 * @brief Handle incoming frame from transport layer
 *
 * @param transport Pointer to transport instance
 * @param buffer Pointer to buffer
 */
static void prv_handle_transport_rx(alfie_transport_t *transport, transport_buffer_t *buffer)
{
    if (buffer->total_size_bytes < sizeof(alfie_proto_header_t)) {
        LOG_WRN("Buffer too small for alfie protocol header.");
        return;
    }

    // Increment reference count
    (void)transport_buffer_ref(buffer);

    switch (transport->type) {
        case ALFIE_TRANSPORT_TYPE_UPSTREAM:
            k_fifo_put(&prv_inst.from_upstream_fifo, buffer);
            break;
        case ALFIE_TRANSPORT_TYPE_DOWNSTREAM:
            k_fifo_put(&prv_inst.from_downstream_fifo, buffer);
            break;
        default:
            LOG_WRN("Unknown transport type: 0x%02X", transport->type);
            break;
    }
}

static void prv_thread(void *arg1, void *arg2, void *arg3)
{
    struct k_poll_event events[2] = {
        K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_FIFO_DATA_AVAILABLE, K_POLL_MODE_NOTIFY_ONLY,
                                 &prv_inst.from_upstream_fifo),
        K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_FIFO_DATA_AVAILABLE, K_POLL_MODE_NOTIFY_ONLY,
                                 &prv_inst.from_downstream_fifo),
    };

    while (true) {
        k_poll(events, ARRAY_SIZE(events), K_FOREVER);

        if (events[0].state == K_POLL_STATE_FIFO_DATA_AVAILABLE) {
            transport_buffer_t *buffer = k_fifo_get(&prv_inst.from_upstream_fifo, K_NO_WAIT);
            if (buffer != NULL) {
                prv_forward_to_downstream(buffer);
            }
            events[0].state = K_POLL_STATE_NOT_READY;
        }

        if (events[1].state == K_POLL_STATE_FIFO_DATA_AVAILABLE) {
            transport_buffer_t *buffer = k_fifo_get(&prv_inst.from_downstream_fifo, K_NO_WAIT);
            if (buffer != NULL) {
                prv_forward_to_upstream(buffer);
            }
            events[1].state = K_POLL_STATE_NOT_READY;
        }
    }
}

/*****************************************************************************
 * Functions
 *****************************************************************************/

int alfie_router_init(void)
{
    if (prv_inst.initialized == true) {
        return -EALREADY;
    }

    sys_slist_init(&prv_inst.endpoints);
    sys_slist_init(&prv_inst.transports);

    k_fifo_init(&prv_inst.from_upstream_fifo);
    k_fifo_init(&prv_inst.from_downstream_fifo);

    prv_inst.thread_id = k_thread_create(&prv_inst.thread, prv_stack, K_THREAD_STACK_SIZEOF(prv_stack), prv_thread, NULL,
                                         NULL, NULL, ALFIE_ROUTER_THREAD_PRIORITY, 0, K_NO_WAIT);

    prv_inst.initialized = true;

    return 0;
}

int alfie_router_register_transport(alfie_transport_t *transport)
{
    if (prv_inst.initialized == false) {
        return -ENODEV;
    }

    if (transport == NULL || transport->api == NULL || transport->api->write == NULL
        || transport->api->register_rx_cb == NULL) {
        return -EINVAL;
    }

    int ret = transport->api->register_rx_cb(prv_handle_transport_rx);
    if (ret != 0) {
        LOG_ERR("Failed to register RX callback with transport: %d", ret);
        return ret;
    }

    sys_slist_append(&prv_inst.transports, (sys_snode_t *)transport);

    return 0;
}

int alfie_router_register_endpoint(alfie_endpoint_t *endpoint)
{
    if (prv_inst.initialized == false) {
        return -ENODEV;
    }

    if (endpoint == NULL) {
        return -EINVAL;
    }

    if (endpoint->api.on_frame_rx == NULL) {
        return -EINVAL;
    }

    sys_slist_append(&prv_inst.endpoints, (sys_snode_t *)endpoint);

    return 0;
}
