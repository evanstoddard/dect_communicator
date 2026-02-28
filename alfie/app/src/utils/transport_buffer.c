/*
 * Copyright (C) Evan Stoddard
 */

/**
 * @file transport_buffer.c
 * @author Evan Stoddard
 * @brief
 */

#include "transport_buffer.h"
#include "zephyr/net_buf.h"

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
transport_buffer_t *prv_buffer_for_index(transport_buffer_pool_t *inst, size_t idx)
{
    uint8_t *raw = (uint8_t *)(inst->contexts);

    if (idx >= inst->num_buffers) {
        return NULL;
    }

    return (transport_buffer_t *)&raw[idx * inst->context_size_bytes];
}

/*****************************************************************************
 * Functions
 *****************************************************************************/

int transport_buffer_pool_init(transport_buffer_pool_t *inst, struct net_buf_pool *pool, size_t num_buffers,
                               size_t buffer_size_bytes, void *contexts, size_t context_size_bytes,
                               uint32_t rx_timeout_ms, transport_buffer_pool_api_t *api)
{
    if (inst == NULL || pool == NULL || contexts == NULL || num_buffers == 0) {
        return -EINVAL;
    }

    if (context_size_bytes < sizeof(transport_buffer_t)) {
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

transport_buffer_t *transport_buffer_pool_get(transport_buffer_pool_t *inst, const uint16_t seq_id,
                                              const size_t total_size_bytes, const uint8_t frag_idx,
                                              const uint8_t frag_total, const void *additional_query_params)
{
    if (inst == NULL) {
        return NULL;
    }

    if (additional_query_params != NULL && (inst->api == NULL || inst->api->additional_query_cb == NULL)) {
        return NULL;
    }

    // First, iterate through and check for buffers that are in a receiving state but have timed out
    transport_buffer_t *buf = NULL;
    for (size_t i = 0; i < inst->num_buffers; i++) {
        buf = prv_buffer_for_index(inst, i);

        if (buf->state != TRANSPORT_BUFFER_STATE_RECEIVING) {
            continue;
        }

        uint32_t delta = k_uptime_get_32() - buf->last_rx_time_ms;

        if (delta >= inst->rx_timeout_ms) {
            net_buf_destroy(buf->buffer);
            buf->buffer = NULL;
            buf->state = TRANSPORT_BUFFER_STATE_FREE;
        }
    }

    // Next, check if a matching buffer has already been allocated
    for (size_t i = 0; i < inst->num_buffers; i++) {
        buf = prv_buffer_for_index(inst, i);

        if (buf->state != TRANSPORT_BUFFER_STATE_RECEIVING) {
            continue;
        }

        // Check base criteria
        if (seq_id != buf->seq_id || frag_total != buf->frag_total || total_size_bytes != buf->total_size_bytes) {
            continue;
        }

        // If base criteria matches, and there's no callback for additional opaque queries, then our job is done
        if (inst->api == NULL || inst->api->additional_query_cb == NULL) {
            return buf;
        }

        if (inst->api->additional_query_cb(buf, additional_query_params) == false) {
            continue;
        }

        return buf;
    }

    if (frag_idx != 0) {
        return NULL;
    }

    // If nothing was found, and this is the first fragment, then attempt to allocate and return buffer from pool
    for (size_t i = 0; i < inst->num_buffers; i++) {
        buf = prv_buffer_for_index(inst, i);

        if (buf->state == TRANSPORT_BUFFER_STATE_FREE) {
            buf->buffer = net_buf_alloc(inst->buffer_pool, K_NO_WAIT);
            if (buf->buffer == NULL) {
                // TODO: Something bad happened.  Should probably assert, collect core dump, and reset...
                return NULL;
            }

            buf->total_size_bytes = total_size_bytes;
            buf->seq_id = seq_id;
            buf->frag_idx = 0;
            buf->frag_total = frag_total;

            buf->last_rx_time_ms = k_uptime_get_32();

            buf->state = TRANSPORT_BUFFER_STATE_RECEIVING;
            return buf;
        }
    }

    // No buffers :(
    return NULL;
}

transport_buffer_write_ret_t transport_buffer_write(transport_buffer_t *buf, const uint8_t frag_idx, const void *data,
                                                    const size_t len_bytes)
{
    if (buf == NULL || data == NULL || len_bytes == 0) {
        return TRANSPORT_BUFFER_WRITE_RET_INVALID_ARGS;
    }

    if (frag_idx > buf->frag_idx) {
        return TRANSPORT_BUFFER_WRITE_RET_OUT_OF_ORDER_FRAGMENT;
    }

    if (frag_idx < buf->frag_idx) {
        return TRANSPORT_BUFFER_WRITE_RET_DUPLICATE;
    }

    if ((buf->buffer->len + len_bytes) > buf->total_size_bytes) {
        return TRANSPORT_BUFFER_WRITE_RET_OVERRUN;
    }

    net_buf_add_mem(buf->buffer, data, len_bytes);

    // Deliberately only updating on new data to avoid potentially getting stuck in a missed-ack-loop.
    buf->last_rx_time_ms = k_uptime_get_32();
    buf->frag_idx++;

    if (buf->frag_idx == buf->frag_total) {
        if (buf->buffer->len != buf->total_size_bytes) {
            return TRANSPORT_BUFFER_WRITE_RET_INVALID_FINAL_SIZE;
        }

        buf->state = TRANSPORT_BUFFER_STATE_REFERENCED;

        return TRANSPORT_BUFFER_WRITE_RET_COMPLETE;
    }

    return TRANSPORT_BUFFER_WRITE_RET_SUCCESS;
}

transport_buffer_t *transport_buffer_ref(transport_buffer_t *buf)
{
    if (buf == NULL || buf->buffer == NULL) {
        return NULL;
    }

    void *ret = net_buf_ref(buf->buffer);
    (void)ret;

    return buf;
}

void transport_buffer_unref(transport_buffer_t *buf)
{
    if (buf == NULL || buf->buffer == NULL) {
        return;
    }

    // NOTE: A bit of an edge case, but if we unref and the current reference counter is 0, then we should mark the
    // buffer state as free.  One could theoretically hook into the destroy callback, but since this module is not the
    // owner of the net_buf_pool, it would be irresponsible to potentially overwrite that.  Should evaluate this for
    // potential race conditions, but the contract within this existing firmware is fairly well defined... famous last
    // words...
    bool last_reference = (buf->buffer->ref == 1);

    net_buf_unref(buf->buffer);

    if (last_reference) {
        buf->state = TRANSPORT_BUFFER_STATE_FREE;
        buf->buffer = NULL;
    }
}
