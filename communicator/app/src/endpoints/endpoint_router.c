/*
 * Copyright (C) Ovyl
 */

/**
 * @file endpoint_router.c
 * @author Evan Stoddard
 * @brief
 */

#include "endpoint_router.h"
#include "endpoints/endpoint_transport.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <errno.h>

#include <zephyr/sys/slist.h>

#include <zephyr/logging/log.h>

/*****************************************************************************
 * Definitions
 *****************************************************************************/

LOG_MODULE_REGISTER(endpoint_router);

/*****************************************************************************
 * Variables
 *****************************************************************************/

static struct {
    bool initialized;

    sys_slist_t endpoints;
    sys_slist_t transports;
} prv_inst;

/*****************************************************************************
 * Private Functions
 *****************************************************************************/

/**
 * @brief RX callback for upstream transports, routing messages to matching endpoints
 *
 * @param src_id Source device ID of the sender
 * @param endpoint_id Endpoint ID the message is addressed to
 * @param ctx Pointer to the reassembly context containing the received data
 */
static void prv_on_upstream_transport_rx(uint16_t src_id, uint8_t endpoint_id, transport_reassem_ctx_t *ctx)
{
    sys_snode_t *node = NULL;
    SYS_SLIST_FOR_EACH_NODE(&prv_inst.endpoints, node)
    {
        endpoint_t *endpoint = (endpoint_t *)node;
        if (endpoint->endpoint_id != endpoint_id) {
            continue;
        }

        endpoint->on_from_upstream(src_id, ctx);
    }
}

/**
 * @brief RX callback for downstream transports, routing messages to matching endpoints
 *
 * @param src_id Source device ID of the sender
 * @param endpoint_id Endpoint ID the message is addressed to
 * @param ctx Pointer to the reassembly context containing the received data
 */
static void prv_on_downstream_transport_rx(uint16_t src_id, uint8_t endpoint_id, transport_reassem_ctx_t *ctx)
{
    sys_snode_t *node = NULL;
    SYS_SLIST_FOR_EACH_NODE(&prv_inst.endpoints, node)
    {
        endpoint_t *endpoint = (endpoint_t *)node;
        if (endpoint->endpoint_id != endpoint_id) {
            continue;
        }

        endpoint->on_from_downstream(src_id, ctx);
    }
}

/*****************************************************************************
 * Functions
 *****************************************************************************/

int endpoint_router_init(void)
{
    if (prv_inst.initialized == true) {
        return -EALREADY;
    }

    sys_slist_init(&prv_inst.endpoints);
    sys_slist_init(&prv_inst.transports);

    prv_inst.initialized = true;

    return 0;
}

int endpoint_router_register_endpoint(endpoint_t *endpoint)
{
    if (endpoint == NULL) {
        return -EINVAL;
    }

    if (endpoint->on_from_downstream == NULL || endpoint->on_from_upstream == NULL) {
        return -EINVAL;
    }

    // FIXME: Should probably guard with mutex...
    sys_slist_append(&prv_inst.endpoints, (sys_snode_t *)endpoint);

    return 0;
}

int endpoint_router_register_transport(endpoint_transport_t *transport)
{
    if (transport == NULL) {
        return -EINVAL;
    }

    if (transport->api.transport_write == NULL || transport->api.transport_register_rx_cb == NULL) {
        return -EINVAL;
    }

    // FIXME: Should probably guard with mutex...
    sys_slist_append(&prv_inst.transports, (sys_snode_t *)transport);

    if (transport->type == ENDPOINT_TRANSPORT_TYPE_UPSTREAM) {
        transport->api.transport_register_rx_cb(prv_on_upstream_transport_rx);
    }
    else {
        transport->api.transport_register_rx_cb(prv_on_downstream_transport_rx);
    }

    return 0;
}

int endpoint_router_write_to_upstream(const uint16_t dst_id, uint8_t endpoint_id, const void *data,
                                      const size_t len_bytes)
{
    if (data == NULL || len_bytes == 0) {
        return -EINVAL;
    }

    sys_snode_t *node = NULL;
    SYS_SLIST_FOR_EACH_NODE(&prv_inst.transports, node)
    {
        endpoint_transport_t *transport = (endpoint_transport_t *)node;

        if (transport->type != ENDPOINT_TRANSPORT_TYPE_UPSTREAM) {
            continue;
        }

        int ret = transport->api.transport_write(dst_id, endpoint_id, data, len_bytes);
        if (ret != 0) {
            LOG_WRN("Failed to write to upstream transport: %d", ret);
        }
    }

    // Right now just return 0.  Errors above or logged.  Need to decide a clean way to handle errors and multiple
    // potential upstreams are implemented
    return 0;
}

int endpoint_router_write_to_downstream(uint16_t dst_id, uint8_t endpoint_id, void *data, size_t len_bytes)
{
    if (data == NULL || len_bytes == 0) {
        return -EINVAL;
    }

    sys_snode_t *node = NULL;
    SYS_SLIST_FOR_EACH_NODE(&prv_inst.transports, node)
    {
        endpoint_transport_t *transport = (endpoint_transport_t *)node;

        if (transport->type != ENDPOINT_TRANSPORT_TYPE_DOWNSTREAM) {
            continue;
        }

        int ret = transport->api.transport_write(dst_id, endpoint_id, data, len_bytes);
        if (ret != 0) {
            LOG_WRN("Failed to write to downstream transport: %d", ret);
        }
    }

    // Right now just return 0.  Errors above or logged.  Need to decide a clean way to handle errors and multiple
    // potential upstreams are implemented
    return 0;
}
