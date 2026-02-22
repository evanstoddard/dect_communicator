/*
 * Copyright (C) Ovyl
 */

/**
 * @file endpoint_router.h
 * @author Evan Stoddard
 * @brief
 */

#ifndef endpoint_router_h
#define endpoint_router_h

#include <stddef.h>
#include <stdint.h>

#include <zephyr/sys/slist.h>

#include "endpoints/endpoint_transport.h"
#include "utils/transport_reassembly_buffer/transport_reassembly_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * Definitions
 *****************************************************************************/

/*****************************************************************************
 * Structs, Unions, Enums, & Typedefs
 *****************************************************************************/

/**
 * @typedef endpoint_t
 * @brief Registered endpoint with callbacks for handling upstream and downstream messages
 *
 */
typedef struct endpoint_t {
    sys_snode_t node;
    int (*on_from_upstream)(uint16_t src_id, transport_reassem_ctx_t *reassem_ctx);
    int (*on_from_downstream)(uint16_t src_id, transport_reassem_ctx_t *reassem_ctx);
    uint8_t endpoint_id;
} endpoint_t;

/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/

/**
 * @brief Initialize endpoint router
 *
 * @return 0 on success, -EALREADY if already initialized
 */
int endpoint_router_init(void);

/**
 * @brief Register an endpoint with the router
 *
 * @param endpoint Pointer to the endpoint to register
 * @return 0 on success, -EINVAL if endpoint or its callbacks are NULL
 */
int endpoint_router_register_endpoint(endpoint_t *endpoint);

/**
 * @brief Write data to all registered upstream transports (e.g. BLE to phone)
 *
 * @param dst_id Destination device ID
 * @param endpoint_id Target endpoint ID
 * @param data Pointer to data to send
 * @param len_bytes Length of data in bytes
 * @return 0 on success, -EINVAL if data is NULL or len_bytes is 0
 */
int endpoint_router_write_to_upstream(const uint16_t dst_id, const uint8_t endpoint_id, const void *data,
                                      const size_t len_bytes);

/**
 * @brief Write data to all registered downstream transports (e.g. DECT NR+ radio)
 *
 * @param dst_id Destination device ID
 * @param endpoint_id Target endpoint ID
 * @param data Pointer to data to send
 * @param len_bytes Length of data in bytes
 * @return 0 on success, -EINVAL if data is NULL or len_bytes is 0
 */
int endpoint_router_write_to_downstream(uint16_t dst_id, const uint8_t endpoint_id, void *data, size_t len_bytes);

#ifdef __cplusplus
}
#endif
#endif /* endpoint_router_h */
