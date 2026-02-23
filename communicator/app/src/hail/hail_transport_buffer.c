/*
 * Copyright (C) Ovyl
 */

/**
 * @file hail_transport_buffer.c
 * @author Evan Stoddard
 * @brief
 */

#include "hail_transport_buffer.h"

#include <errno.h>

#include <zephyr/kernel.h>

/*****************************************************************************
 * Definitions
 *****************************************************************************/

/*****************************************************************************
 * Variables
 *****************************************************************************/

/*****************************************************************************
 * Private Functions
 *****************************************************************************/

/**
 * @brief Return pointer to buffer for a given index
 *
 * @param inst Pool instance
 * @param idx Index of buffer
 * @return Returns pointer to buffer
 */
hail_transport_buffer_t *prv_buffer_for_index(hail_transport_buffer_pool_t *inst, size_t idx)
{
    uint8_t *raw = (uint8_t *)(inst->contexts);

    if (idx >= inst->num_buffers) {
        return NULL;
    }

    return (hail_transport_buffer_t *)&raw[idx * inst->context_size_bytes];
}

/*****************************************************************************
 * Functions
 *****************************************************************************/

int hail_transport_buffer_pool_init(hail_transport_buffer_pool_t *inst, struct net_buf_pool *pool, size_t num_buffers,
                                    size_t buffer_size_bytes, void *contexts, size_t context_size_bytes,
                                    uint32_t rx_timeout_ms, hail_transport_buffer_pool_api_t *api)
{
    if (inst == NULL || pool == NULL || api == NULL) {
        return -EINVAL;
    }

    if (context_size_bytes < sizeof(hail_transport_buffer_t)) {
        return -EINVAL;
    }

    inst->buffer_pool = pool;

    inst->num_buffers = num_buffers;
    inst->buffer_size_bytes = buffer_size_bytes;

    inst->contexts = contexts;
    inst->context_size_bytes = context_size_bytes;

    inst->rx_timeout_ms = rx_timeout_ms;

    inst->api = api;

    return 0;
}

hail_transport_buffer_t *hail_transport_buffer_pool_get(hail_transport_buffer_pool_t *inst, const uint16_t seq_id,
                                                        const size_t total_size_bytes, const uint8_t frag_idx,
                                                        const uint8_t frag_total, const void *additional_query_params)
{
    if (inst == NULL) {
        return NULL;
    }

    if (additional_query_params != NULL && inst->api->additional_query_cb == NULL) {
        return NULL;
    }

    // First, iterate through and check for buffers that are in a receiving state but have timed out
    hail_transport_buffer_t *buf = NULL;
    for (size_t i = 0; i < inst->num_buffers; i++) {
        buf = prv_buffer_for_index(inst, i);

        if (buf->state != HAIL_TRANSPORT_BUFFER_STATE_RECEIVING) {
            continue;
        }

        uint32_t delta = k_uptime_get_32() - buf->last_rx_time_ms;

        if (delta >= inst->rx_timeout_ms) {
            net_buf_destroy(buf->buffer);
            buf->state = HAIL_TRANSPORT_BUFFER_STATE_FREE;
        }
    }

    // Next, check if a matching buffer has already been allocated
    for (size_t i = 0; i < inst->num_buffers; i++) {
        buf = prv_buffer_for_index(inst, i);

        if (buf->state != HAIL_TRANSPORT_BUFFER_STATE_RECEIVING) {
            continue;
        }

        // Check base criteria
        if (seq_id != buf->seq_id || frag_total != buf->frag_total || total_size_bytes != buf->total_size_bytes) {
            continue;
        }

        // If base criteria matches, and there's no callback for additional opaque queries, then our job is done
        if (inst->api->additional_query_cb == NULL) {
            return buf;
        }

        if (inst->api->additional_query_cb(buf, additional_query_params) == false) {
            continue;
        }
    }

    // If nothing was found, and this is the first fragment, then attempt to allocate and return buffer from pool
    for (size_t i = 0; i < inst->num_buffers; i++) {
        buf = prv_buffer_for_index(inst, i);

        if (buf->state == HAIL_TRANSPORT_BUFFER_STATE_FREE) {
            if (net_buf_alloc(inst->buffer_pool, K_NO_WAIT) == NULL) {
                // TODO: Something bad happened.  Should probably assert, collect core dump, and reset...
                return NULL;
            }

            buf->total_size_bytes = total_size_bytes;
            buf->seq_id = seq_id;
            buf->frag_idx = 0;
            buf->frag_total = frag_total;

            buf->last_rx_time_ms = k_uptime_get_32();

            buf->state = HAIL_TRANSPORT_BUFFER_STATE_RECEIVING;
            return buf;
        }
    }

    // No buffers :(
    return NULL;
}

hail_transport_buffer_write_ret_t hail_transport_buffer_write(hail_transport_buffer_t *buf, const uint8_t frag_idx,
                                                              const void *data, const size_t len_bytes)
{
}

hail_transport_buffer_t *hail_transport_buffer_ref(hail_transport_buffer_t *buf)
{
}

void hail_transport_buffer_unref(hail_transport_buffer_t *buf)
{
}
