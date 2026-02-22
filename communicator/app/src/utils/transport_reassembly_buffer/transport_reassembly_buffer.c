/*
 * Copyright (C) Ovyl
 */

/**
 * @file transport_reassembly_buffer.c
 * @author Evan Stoddard
 * @brief
 */

#include "transport_reassembly_buffer.h"

#include <errno.h>

#include <zephyr/logging/log.h>

/*****************************************************************************
 * Definitions
 *****************************************************************************/

LOG_MODULE_REGISTER(transport_reassem_buffer);

/*****************************************************************************
 * Variables
 *****************************************************************************/

/*****************************************************************************
 * Private Functions
 *****************************************************************************/

/**
 * @brief Get a reassembly context pointer by index into the context array
 *
 * @param inst Pointer to the reassembly buffer instance
 * @param index Index of the context to retrieve
 * @return Pointer to the context, or NULL if index is out of bounds
 */
static transport_reassem_ctx_t *prv_context_for_index(const transport_reassem_buffer_t *inst, size_t index)
{
    uint8_t *ctx_buffer = (uint8_t *)(inst->contexts);

    if (index >= inst->params.num_buffers) {
        return NULL;
    }

    return (transport_reassem_ctx_t *)(&ctx_buffer[index]);
}

/**
 * @brief Initialize all reassembly contexts to the free state
 *
 * @param inst Pointer to the reassembly buffer instance
 */
static void prv_initialize_reassembly_contexts(const transport_reassem_buffer_t *inst)
{
    for (size_t i = 0; i < inst->params.num_buffers; i++) {
        transport_reassem_ctx_t *ctx = prv_context_for_index(inst, i);
        ctx->state = TRANSPORT_REASSEMBLY_CTX_STATE_FREE;
    }
}

/*****************************************************************************
 * Functions
 *****************************************************************************/

int transport_reassem_buffer_init(transport_reassem_buffer_t *reassem_buffer, struct net_buf_pool *buffer_pool,
                                  void *reassem_contexts, transport_reassem_buffer_params_t *params)
{
    if (reassem_buffer == NULL || buffer_pool == NULL || params == NULL) {
        return -EINVAL;
    }

    if (params->context_size_bytes < sizeof(transport_reassem_ctx_t)) {
        LOG_ERR("Context size param is less than base context type.");
        return -EINVAL;
    }

    if (params->match_cb == NULL || params->ctx_allocated_cb == NULL) {
        LOG_ERR("Callbacks required.");
        return -EINVAL;
    }

    reassem_buffer->buffer_pool = buffer_pool;
    reassem_buffer->contexts = reassem_contexts;
    reassem_buffer->params = *params;

    prv_initialize_reassembly_contexts(reassem_buffer);

    return 0;
}

transport_reassem_ctx_t *transport_reassem_buffer_get_context(transport_reassem_buffer_t *reassem_buffer, void *input,
                                                              uint8_t frag_idx)
{
    transport_reassem_ctx_t *ctx = NULL;

    for (size_t i = 0; i < reassem_buffer->params.num_buffers; i++) {
        ctx = prv_context_for_index(reassem_buffer, i);

        if (ctx->state != TRANSPORT_REASSEMBLY_CTX_STATE_IN_USE) {
            continue;
        }

        uint32_t delta_ms = k_uptime_get_32() - ctx->last_rx_ms;

        if (delta_ms >= reassem_buffer->params.rx_timeout_ms) {
            ctx->state = TRANSPORT_REASSEMBLY_CTX_STATE_FREE;
            net_buf_unref(ctx->buffer);
        }
    }

    for (size_t i = 0; i < reassem_buffer->params.num_buffers; i++) {
        ctx = prv_context_for_index(reassem_buffer, i);

        if (ctx->state != TRANSPORT_REASSEMBLY_CTX_STATE_RECEIVING) {
            continue;
        }

        if (reassem_buffer->params.match_cb(reassem_buffer, ctx, input) == true) {
            return ctx;
        }
    }

    if (frag_idx != 0) {
        return NULL;
    }

    for (size_t i = 0; i < reassem_buffer->params.num_buffers; i++) {
        ctx = prv_context_for_index(reassem_buffer, i);

        if (ctx->state != TRANSPORT_REASSEMBLY_CTX_STATE_FREE) {
            continue;
        }

        ctx->buffer = net_buf_alloc(reassem_buffer->buffer_pool, K_NO_WAIT);
        if (ctx->buffer == NULL) {
            return NULL;
        }

        reassem_buffer->params.ctx_allocated_cb(reassem_buffer, ctx, input);

        ctx->state = TRANSPORT_REASSEMBLY_CTX_STATE_RECEIVING;
        ctx->last_rx_ms = k_uptime_get_32();

        *((uintptr_t *)net_buf_user_data(ctx->buffer)) = (uintptr_t)ctx;

        return ctx;
    }

    return NULL;
}

void transport_reassem_net_buf_destroy(struct net_buf *buf)
{
    transport_reassem_ctx_t *ctx = *(transport_reassem_ctx_t **)net_buf_user_data(buf);
    net_buf_destroy(buf);
    ctx->state = TRANSPORT_REASSEMBLY_CTX_STATE_FREE;
}
