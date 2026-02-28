/*
 * Copyright (C) Evan Stoddard
 */

/**
 * @file transport_buffer.h
 * @author Evan Stoddard
 * @brief
 */

#ifndef transport_buffer_h
#define transport_buffer_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/net_buf.h>

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * Definitions
 *****************************************************************************/

/*****************************************************************************
 * Structs, Unions, Enums, & Typedefs
 *****************************************************************************/

typedef enum {
    TRANSPORT_BUFFER_WRITE_RET_SUCCESS,
    TRANSPORT_BUFFER_WRITE_RET_DUPLICATE,
    TRANSPORT_BUFFER_WRITE_RET_COMPLETE,
    TRANSPORT_BUFFER_WRITE_RET_OVERRUN,
    TRANSPORT_BUFFER_WRITE_RET_INVALID_FINAL_SIZE,
    TRANSPORT_BUFFER_WRITE_RET_OUT_OF_ORDER_FRAGMENT,
    TRANSPORT_BUFFER_WRITE_RET_INVALID_ARGS,
} transport_buffer_write_ret_t;

typedef enum {
    TRANSPORT_BUFFER_STATE_FREE,
    TRANSPORT_BUFFER_STATE_RECEIVING,
    TRANSPORT_BUFFER_STATE_REFERENCED,
} transport_buffer_state_t;

struct transport_buffer_pool_t;

struct transport_buffer_t;

/**
 * @typedef transport_buffer_pool_api_t
 * @brief API definition for transport buffer pool
 *
 */
typedef struct transport_buffer_pool_api_t {
    bool (*additional_query_cb)(const struct transport_buffer_t *buffer, const void *additional_query_params);
} transport_buffer_pool_api_t;

/**
 * @typedef transport_buffer_t
 * @brief Individual transport buffer containing metadata and reassembly state
 *
 */
typedef struct transport_buffer_t {
    void *fifo_reserved; // NOTE: While net_buf can directly be placed in a fifo, opt for explicitly provide reserved
                         // space for FIFO usage.  Subject to change

    struct net_buf *buffer;

    volatile transport_buffer_state_t state;

    uint16_t seq_id;

    uint16_t total_size_bytes;

    uint8_t frag_total;
    uint8_t frag_idx;

    uint32_t last_rx_time_ms;

} transport_buffer_t;

/**
 * @typedef transport_buffer_pool_t
 * @brief Pool managing a collection of transport buffers
 *
 */
typedef struct transport_buffer_pool_t {
    struct net_buf_pool *buffer_pool;

    size_t num_buffers;
    size_t buffer_size_bytes;
    size_t context_size_bytes;

    uint32_t rx_timeout_ms;

    void *contexts;

    transport_buffer_pool_api_t *api;
} transport_buffer_pool_t;

/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/

/**
 * @brief Initialize transport buffer pool
 *
 * @param inst Pointer to instance to initialize
 * @param pool Pointer to net_buf_pool
 * @param num_buffers Number of buffers available (1:1 for number of allocatable buffers)
 * @param buffer_size_bytes Max size of buffer for a context
 * @param contexts Pointer to contiguous transport_buffer_t array (This must be sized to num_buffers *
 * sizeof(transport_buffer_t)).  transport_buffer_t is a base type, so it can be augmented, but the size of
 * the array must match num_buffers * sizeof(some_augmented_transport_buffer_t).
 * @param context_size_bytes Size of buffer struct (Usually sizeof(transport_buffer_t) unless extending it.  Then
 * you MUST provide the size of augmented struct)
 * @param rx_timeout_ms RX timeout in ms.  Any buffers that are in a receiving state where the timeout is expired will
 * be released to the pool next time transport_buffer_pool_get is called.
 * @param api Pointer to API function pointers (must remain in scope)
 * @return Returns 0 on success or negative errno on error
 */
int transport_buffer_pool_init(transport_buffer_pool_t *inst, struct net_buf_pool *pool, size_t num_buffers,
                                size_t buffer_size_bytes, void *contexts, size_t context_size_bytes,
                                uint32_t rx_timeout_ms, transport_buffer_pool_api_t *api);

/**
 * @brief Find existing transport buffer or allocate new one if required
 *
 * @param inst Pointer to transport buffer pool instance
 * @param seq_id Sequence ID to check for
 * @param total_size_bytes Total size of incoming message (used when allocating a new buffer)
 * @param frag_idx Fragment index of incoming message (if this is 0 and there are no matches, will attempt to allocate
 * from pool)
 * @param frag_total Total number of expected fragments
 * @param additional_query_params Additional query parameters.  Must not be NULL if query callback is registered.
 * Ignored if query callback is NULL
 * @return Returns pointer to buffer if one is found or allocated.  Returns NULL if no buffer exists, previous buffer
 * timed-out, or one couldn't be allocated.
 */
transport_buffer_t *transport_buffer_pool_get(transport_buffer_pool_t *inst, const uint16_t seq_id,
                                                const size_t total_size_bytes, const uint8_t frag_idx,
                                                const uint8_t frag_total, const void *additional_query_params);

/**
 * @brief Write fragment to buffer
 *
 * @param buf Pointer to buffer
 * @param frag_idx Index of incoming fragment
 * @param data Pointer to data
 * @param len_bytes Length of incoming data
 * @return Return status of write operation
 */
transport_buffer_write_ret_t transport_buffer_write(transport_buffer_t *buf, const uint8_t frag_idx,
                                                      const void *data, const size_t len_bytes);

/**
 * @brief Increments internal reference counter of buffer
 *
 * @param buf Pointer to buffer
 * @return Returns pointer to buffer
 */
transport_buffer_t *transport_buffer_ref(transport_buffer_t *buf);

/**
 * @brief Decrements internal reference counter of buffer.  NOTE: THIS MUST BE CALLED IF transport_buffer_ref IS
 * CALLED
 *
 * @param buf Pointer to buffer
 */
void transport_buffer_unref(transport_buffer_t *buf);

#ifdef __cplusplus
}
#endif
#endif /* transport_buffer_h */
