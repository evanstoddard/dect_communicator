/*
 * Copyright (C) Evan Stoddard
 */

/**
 * @file dect_protocol_layer.c
 * @author Evan Stoddard
 * @brief
 */

#include "dect_data_layer.h"
#include "dect_protocol_layer_private.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

#include <zephyr/net_buf.h>

#include <zephyr/random/random.h>

/*****************************************************************************
 * Definitions
 *****************************************************************************/

LOG_MODULE_REGISTER(dect_protocol_layer);

#define DECT_PROTOCOL_LAYER_RX_TIMEOUT_MS (10000U)

#define DECT_PROTOCOL_LAYER_NUM_WRITE_ATTEMPTS (4U)

#define DECT_PROTOCOL_LAYER_ACK_TIMEOUT_MS (DECT_PROTOCOL_LAYER_RX_TIMEOUT_MS / 4)

/*****************************************************************************
 * Variables
 *****************************************************************************/

typedef enum {
    DECT_PROTOCOL_LAYER_REASSEM_STATE_FREE,
    DECT_PROTOCOL_LAYER_REASSEM_STATE_RECEIVING,
    DECT_PROTOCOL_LAYER_REASSEM_IN_USE,
} dect_protocol_layer_reassem_state_t;

typedef enum {
    DECT_PROTOCOL_LAYER_FRAME_TYPE_ACK,
    DECT_PROTOCOL_LAYER_FRAME_TYPE_ENDPOINT,
} dect_protocol_layer_frame_type_t;

/**
 * @typedef dect_protocol_layer_reassem_ctx_t
 * @brief Reassembly context
 *
 */
typedef struct dect_protocol_layer_reassem_ctx_t {
    volatile dect_protocol_layer_reassem_state_t state;
    uint16_t src_id;
    uint16_t seq_id;
    uint8_t frag_total;
    uint8_t frag_idx;
    uint8_t frame_type;
    uint8_t endpoint_id;
    struct net_buf *buf;
    uint32_t last_rx_ms;
} dect_protocol_layer_reassem_ctx_t;

/**
 * @brief Private instance
 */
static struct {
    bool initialized;

    dect_data_layer_rx_handler_t data_layer_rx_handler;

    dect_protocol_layer_reassem_ctx_t contexts[CONFIG_DECT_PROTO_MAX_REASSEM_CONTEXTS];

    struct k_sem ack_sem;
} prv_inst;

NET_BUF_POOL_DEFINE(prv_rx_buf_pool, CONFIG_DECT_PROTO_MAX_REASSEM_CONTEXTS,
                    (CONFIG_DECT_PROTO_MAX_FRAGMENTS_PER_CONTEXT * CONFIG_DECT_PROTO_MAX_FRAGMENT_SIZE_BYTES),
                    sizeof(uintptr_t), NULL);

/*****************************************************************************
 * Private Functions
 *****************************************************************************/

/**
 * @brief Get (or allocate) context for incoming fragment
 *
 * @param src_id Source ID of fragment origin
 * @param fragment Pointer to incoming fragment
 * @return Returns pointer to reassembly context
 */
static dect_protocol_layer_reassem_ctx_t *prv_get_reassembly_context(const uint16_t src_id,
                                                                 const dect_protocol_layer_fragment_t *fragment)
{
    dect_protocol_layer_reassem_ctx_t *ctx = NULL;

    // Perform some garbage collection on potential expired contexts
    for (size_t i = 0; i < CONFIG_DECT_PROTO_MAX_REASSEM_CONTEXTS; i++) {
        ctx = &prv_inst.contexts[i];

        if (ctx->state != DECT_PROTOCOL_LAYER_REASSEM_STATE_RECEIVING) {
            continue;
        }

        uint32_t delta_ms = k_uptime_get_32() - ctx->last_rx_ms;

        if (delta_ms >= DECT_PROTOCOL_LAYER_RX_TIMEOUT_MS) {
            ctx->state = DECT_PROTOCOL_LAYER_REASSEM_STATE_FREE;
            net_buf_unref(ctx->buf);
        }
    }

    // Now check if there is a receiving context with a matching sequence and source ID
    for (size_t i = 0; i < CONFIG_DECT_PROTO_MAX_REASSEM_CONTEXTS; i++) {
        ctx = &prv_inst.contexts[i];

        if (ctx->state != DECT_PROTOCOL_LAYER_REASSEM_STATE_RECEIVING) {
            continue;
        }

        if (ctx->src_id == src_id && ctx->seq_id == fragment->header.seq_id) {
            return ctx;
        }
    }

    // Handle case there's no context matching sequence ID and we're not receiving a brand new message
    if (fragment->header.frag_idx != 0) {
        return NULL;
    }

    // If we've reach this point, it's a brand new message and we need to allocate a new context
    for (size_t i = 0; i < CONFIG_DECT_PROTO_MAX_REASSEM_CONTEXTS; i++) {
        ctx = &prv_inst.contexts[i];

        if (ctx->state != DECT_PROTOCOL_LAYER_REASSEM_STATE_FREE) {
            continue;
        }
        ctx->src_id = src_id;
        ctx->frag_idx = 0;
        ctx->frag_total = fragment->header.frag_total;
        ctx->seq_id = fragment->header.seq_id;
        ctx->frame_type = fragment->header.frame_type;
        ctx->endpoint_id = fragment->header.endpoint_id;

        ctx->buf = net_buf_alloc(&prv_rx_buf_pool, K_NO_WAIT);
        if (ctx->buf == NULL) {
            return NULL;
        }

        ctx->last_rx_ms = k_uptime_get_32();
        ctx->state = DECT_PROTOCOL_LAYER_REASSEM_STATE_RECEIVING;

        return ctx;
    }

    // No available contexts :(
    return NULL;
}

/**
 * @brief Handling incoming ACK
 *
 * @param fragment Pointer to incoming ACK
 */
static void prv_handle_incoming_ack(const dect_protocol_layer_fragment_t *fragment)
{
    // FIXME: Validate ACK metadata against latest sent fragment header

    k_sem_give(&prv_inst.ack_sem);
}

/**
 * @brief Ensure fragment header matches existing context
 *
 * @param ctx Pointer to context
 * @param fragment Pointer to fragment
 * @return Returns true of fragment header matches existing context meta data
 */
static bool prv_validate_fragment_header(const dect_protocol_layer_reassem_ctx_t *ctx,
                                         const dect_protocol_layer_fragment_t *fragment)
{
    if (fragment->header.frag_total != ctx->frag_total) {
        return false;
    }

    if (fragment->header.frame_type != ctx->frame_type) {
        return false;
    }

    return true;
}

/**
 * @brief [TODO:description]
 *
 * @param ctx [TODO:parameter]
 * @param fragment [TODO:parameter]
 */
static void prv_ack_fragment(const dect_protocol_layer_reassem_ctx_t *ctx, const dect_protocol_layer_fragment_t *fragment)
{
    dect_protocol_layer_fragment_t ack = {.header = fragment->header};

    ack.header.frame_type = DECT_PROTOCOL_LAYER_FRAME_TYPE_ACK;

    int ret = dect_data_layer_write(ctx->src_id, &ack, sizeof(dect_protocol_layer_header_t));
    if (ret != 0) {
        LOG_ERR("Failed to write ACK: %d", ret);
    }
}

/**
 * @brief [TODO:description]
 *
 * @param ctx [TODO:parameter]
 */
static void prv_handle_complete_protocol_message(dect_protocol_layer_reassem_ctx_t *ctx)
{
    ctx->state = DECT_PROTOCOL_LAYER_REASSEM_IN_USE;

    LOG_INF("Received complete protocol message:\r\n"
            "\tSrc ID: 0x%04X\r\n"
            "\tSeq ID: 0x%04X\r\n"
            "\tFrame Type: 0x%02X\r\n"
            "\tEndpoint ID: 0x%02X\r\n"
            "\tPayload: %s",
            ctx->src_id, ctx->seq_id, ctx->frame_type, ctx->endpoint_id, (char *)ctx->buf->data);

    net_buf_unref(ctx->buf);
    ctx->state = DECT_PROTOCOL_LAYER_REASSEM_STATE_FREE;
}

/**
 * @brief Write incoming fragment to buffer
 *
 * @param ctx Pointer to reassembly context
 * @param fragment Pointer to fragment
 * @param fragment_size_bytes Size of fragment payload in bytes
 */
static void prv_writing_incoming_fragment(dect_protocol_layer_reassem_ctx_t *ctx,
                                          const dect_protocol_layer_fragment_t *fragment, size_t fragment_size_bytes)
{
    if (prv_validate_fragment_header(ctx, fragment) == false) {
        LOG_WRN("Fragment header doesn't match reassembly context metadata.");
        return;
    }

    // If we've received a previous fragment, chances are, our ACK was dropped, so simply re-ack
    if (fragment->header.frag_idx < ctx->frag_idx) {
        prv_ack_fragment(ctx, fragment);
        ctx->last_rx_ms = k_uptime_get_32();
        return;
    }

    // We currently don't support out-of-order fragments, so if we're receiving a fragment from the future, silently
    // fail and let sender timeout or send correct fragment
    if (fragment->header.frag_idx != ctx->frag_idx) {
        LOG_WRN("A time travelling fragment as arrived.");
        return;
    }

    net_buf_add_mem(ctx->buf, fragment->payload, fragment_size_bytes - sizeof(dect_protocol_layer_header_t));

    ctx->frag_idx++;
    ctx->last_rx_ms = k_uptime_get_32();

    if (ctx->frag_idx == ctx->frag_total) {
        prv_handle_complete_protocol_message(ctx);
    }

    prv_ack_fragment(ctx, fragment);
}

/**
 * @brief [TODO:description]
 *
 * @param src_id [TODO:parameter]
 * @param data [TODO:parameter]
 * @param len_bytes [TODO:parameter]
 * @param ctx [TODO:parameter]
 */
static void prv_on_data_layer_rx(const uint16_t src_id, const void *data, const size_t len_bytes, void *ctx)
{
    if (len_bytes < sizeof(dect_protocol_layer_header_t)) {
        LOG_WRN("Incoming data size less than protocol layer header.");
        return;
    }

    const dect_protocol_layer_fragment_t *fragment = (dect_protocol_layer_fragment_t *)data;

    // Special case for handling incoming ACK.  No need to put into a network buffer
    if (fragment->header.frame_type == DECT_PROTOCOL_LAYER_FRAME_TYPE_ACK) {
        prv_handle_incoming_ack(fragment);
        return;
    }

    if (fragment->header.frag_total > CONFIG_DECT_PROTO_MAX_FRAGMENTS_PER_CONTEXT) {
        LOG_WRN("Invalid total fragment count.");
        return;
    }

    dect_protocol_layer_reassem_ctx_t *reassm_ctx = prv_get_reassembly_context(src_id, fragment);
    if (reassm_ctx == NULL) {
        LOG_WRN("Unable to find or allocate reassembly context for incoming fragment.");
        return;
    }

    prv_writing_incoming_fragment(reassm_ctx, fragment, len_bytes);
}

/**
 * @brief [TODO:description]
 *
 * @param dst_id
 * @param fragment [TODO:parameter]
 * @param payload_size
 * @return [TODO:return]
 */
static int prv_write_fragment(const uint16_t dst_id, const dect_protocol_layer_fragment_t *fragment,
                              size_t payload_size)
{
    int ret = 0;

    for (uint8_t i = 0; i < DECT_PROTOCOL_LAYER_NUM_WRITE_ATTEMPTS; i++) {
        k_sem_reset(&prv_inst.ack_sem);

        ret = dect_data_layer_write(dst_id, fragment, sizeof(dect_protocol_layer_header_t) + payload_size);
        if (ret != 0) {
            LOG_ERR("Failed to queue up fragment to phy: %d", ret);
            return ret;
        }

        ret = k_sem_take(&prv_inst.ack_sem, K_MSEC(DECT_PROTOCOL_LAYER_ACK_TIMEOUT_MS));

        if (ret == 0) {
            return 0;
        }
    }

    return ret;
}

/*****************************************************************************
 * Functions
 *****************************************************************************/

int dect_protocol_layer_init(void)
{
    prv_inst.data_layer_rx_handler.ctx = NULL;
    prv_inst.data_layer_rx_handler.handler = prv_on_data_layer_rx;

    k_sem_init(&prv_inst.ack_sem, 0, 1);

    int ret = dect_data_layer_register_rx_handler(&prv_inst.data_layer_rx_handler);
    if (ret != 0) {
        return ret;
    }

    prv_inst.initialized = true;

    return 0;
}

int dect_protocol_layer_write(const uint16_t dst_id, const uint8_t frame_type, const uint8_t endpoint_id,
                              const void *data, const size_t len_bytes)
{
    if (data == NULL) {
        return -EINVAL;
    }

    uint16_t seq_id = sys_rand16_get();

    uint8_t frag_total = len_bytes / CONFIG_DECT_PROTO_MAX_FRAGMENT_SIZE_BYTES;
    if ((len_bytes % CONFIG_DECT_PROTO_MAX_FRAGMENT_SIZE_BYTES) > 0) {
        frag_total++;
    }
    for (uint8_t i = 0; i < frag_total; i++) {
        dect_protocol_layer_fragment_t fragment = {.header.frame_type = frame_type,
                                                   .header.endpoint_id = endpoint_id,
                                                   .header.seq_id = seq_id,
                                                   .header.version = 1,
                                                   .header.flags = 0,
                                                   .header.frag_total = frag_total,
                                                   .header.frag_idx = i};

        size_t frag_size_bytes = len_bytes - (i * CONFIG_DECT_PROTO_MAX_FRAGMENT_SIZE_BYTES);
        if (frag_size_bytes > CONFIG_DECT_PROTO_MAX_FRAGMENT_SIZE_BYTES) {
            frag_size_bytes = CONFIG_DECT_PROTO_MAX_FRAGMENT_SIZE_BYTES;
        }

        memcpy(fragment.payload, &((uint8_t *)data)[i * CONFIG_DECT_PROTO_MAX_FRAGMENT_SIZE_BYTES], frag_size_bytes);
        int ret = prv_write_fragment(dst_id, &fragment, frag_size_bytes);

        if (ret != 0) {
            LOG_ERR("Failed to write fragment: %d", ret);
            return ret;
        }
    }

    return 0;
}

/**
 * @brief Debug command to write raw data over PHY
 *
 * @param shell Pointer to shell
 * @param argc Number of arguments
 * @param argv Argument list
 * @return Return status
 */
static int prv_shell_cmd_write(const struct shell *shell, size_t argc, char **argv)
{
    uint16_t device_id = atoi(argv[1]);
    char *message = argv[2];

    int ret = dect_protocol_layer_write(device_id, DECT_PROTOCOL_LAYER_FRAME_TYPE_ENDPOINT, 1, message,
                                         strlen(message) + 1);

    if (ret != 0) {
        shell_error(shell, "Failed to write message to device ID 0x%04X: %d", device_id, ret);
        return ret;
    }

    shell_print(shell, "Successfully wrote message to device ID 0x%04X.", device_id);

    return 0;
}

SHELL_SUBCMD_SET_CREATE(sub_protocol_layer, (protocol_layer));
SHELL_CMD_REGISTER(protocol_layer, &sub_protocol_layer, "Protocol Layer Commands", NULL);

SHELL_SUBCMD_ADD((protocol_layer), write, NULL,
                 "Write data over protocol layer.\n"
                 "Usage: protocol_layer write <dest_id> <message>\n",
                 prv_shell_cmd_write, 3, 0);
