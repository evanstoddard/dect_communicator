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
 * @brief [TODO:description]
 *
 * @param inst [TODO:parameter]
 * @param index [TODO:parameter]
 * @return [TODO:return]
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
 * @brief [TODO:description]
 *
 * @param inst [TODO:parameter]
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

int transport_reassem_buffer_release_buffer(transport_reassem_buffer_t *buffer, transport_reassem_ctx_t *context)
{
    if (buffer == NULL || context == NULL) {
        return -EINVAL;
    }

    if (context->state != TRANSPORT_REASSEMBLY_CTX_STATE_IN_USE) {
        return -EINVAL;
    }

    net_buf_unref(context->buffer);
    context->state = TRANSPORT_REASSEMBLY_CTX_STATE_FREE;

    return 0;
}
