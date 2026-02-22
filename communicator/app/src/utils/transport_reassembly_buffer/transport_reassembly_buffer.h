/*
 * Copyright (C) Ovyl
 */

/**
 * @file transport_reassembly_buffer.h
 * @author Evan Stoddard
 * @brief
 */

#ifndef transport_reassembly_buffer_h
#define transport_reassembly_buffer_h

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <zephyr/net_buf.h>

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * Definitions
 *****************************************************************************/

#define TRANSPORT_REASSEM_CTX_BASE_SIZE_BYTES (sizeof(transport_reassem_ctx_t))

/*****************************************************************************
 * Structs, Unions, Enums, & Typedefs
 *****************************************************************************/

typedef enum {
    TRANSPORT_REASSEMBLY_CTX_STATE_FREE,
    TRANSPORT_REASSEMBLY_CTX_STATE_RECEIVING,
    TRANSPORT_REASSEMBLY_CTX_STATE_IN_USE,
} transport_reassembly_ctx_state_t;

struct transport_reassem_ctx_t;

struct transport_reassem_buffer_t;

typedef bool (*transport_reassem_buffer_match_cb_t)(const struct transport_reassem_buffer_t *reassem_buf,
                                                    const struct transport_reassem_ctx_t *reassem_ctx,
                                                    const void *data);

typedef void (*transport_reassem_buffer_ctx_allocated_cb_t)(const struct transport_reassem_buffer_t *reassem_buf,
                                                            struct transport_reassem_ctx_t *reassem_ctx, void *input);

/**
 * @typedef transport_reassem_ctx_t
 * @brief Base reassembly context tracking the state of an in-progress fragmented transfer
 *
 */
typedef struct transport_reassem_ctx_t {
    transport_reassembly_ctx_state_t state;

    uint32_t transport_id;

    uint16_t src_id;
    uint16_t dst_id;

    uint32_t last_rx_ms;
    uint16_t seq_id;
    uint8_t frag_total;
    uint8_t frag_idx;

    struct net_buf *buffer;
} transport_reassem_ctx_t;

/**
 * @typedef transport_reassem_buffer_params_t
 * @brief Configuration parameters for initializing a transport reassembly buffer
 *
 */
typedef struct transport_reassem_buffer_params_t {
    size_t context_size_bytes;
    size_t buffer_size_bytes;

    size_t num_buffers;

    uint32_t rx_timeout_ms;

    transport_reassem_buffer_match_cb_t match_cb;
    transport_reassem_buffer_ctx_allocated_cb_t ctx_allocated_cb;
} transport_reassem_buffer_params_t;

/**
 * @typedef transport_reassem_buffer_t
 * @brief Transport reassembly buffer managing multiple reassembly contexts and their backing buffers
 *
 */
typedef struct transport_reassem_buffer_t {
    void *contexts;
    transport_reassem_buffer_params_t params;
    struct net_buf_pool *buffer_pool;
} transport_reassem_buffer_t;

/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/

/**
 * @brief Initialize a transport reassembly buffer
 *
 * @param reassem_buffer Pointer to the reassembly buffer to initialize
 * @param buffer_pool Pointer to the net_buf pool backing reassembly contexts
 * @param reassem_contexts Pointer to the array of reassembly contexts
 * @param params Pointer to configuration parameters
 * @return 0 on success, -EINVAL on invalid arguments
 */
int transport_reassem_buffer_init(transport_reassem_buffer_t *reassem_buffer, struct net_buf_pool *buffer_pool,
                                  void *reassem_contexts, transport_reassem_buffer_params_t *params);

/**
 * @brief Get or allocate a reassembly context matching the given input
 *
 * @param reaseem_buffer Pointer to the reassembly buffer
 * @param input Pointer to transport-specific input data used for matching
 * @return Pointer to the matching or newly allocated context, or NULL if unavailable
 */
transport_reassem_ctx_t *transport_reassem_buffer_get_context(transport_reassem_buffer_t *reaseem_buffer, void *input,
                                                              uint8_t frag_idx);

/**
 * @brief net_buf destroy callback that resets the reassembly context state.
 *        Pass this to NET_BUF_POOL_DEFINE as the destroy callback.
 *
 * @param buf Pointer to net_buf being destroyed
 */
void transport_reassem_net_buf_destroy(struct net_buf *buf);

#ifdef __cplusplus
}
#endif
#endif /* transport_reassembly_buffer_h */
