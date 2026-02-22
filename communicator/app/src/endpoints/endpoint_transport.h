/*
 * Copyright (C) Ovyl
 */

/**
 * @file endpoint_transport.h
 * @author Evan Stoddard
 * @brief
 */

#ifndef endpoint_transport_h
#define endpoint_transport_h

#include <stdint.h>
#include <stddef.h>

#include <zephyr/sys/slist.h>

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

typedef enum {
    ENDPOINT_TRANSPORT_TYPE_UPSTREAM,
    ENDPOINT_TRANSPORT_TYPE_DOWNSTREAM,

    // TODO: Currently not used, but wanted to keep here to plan out functionality of a repeater
    ENDPOINT_TRANSPORT_TYPE_REPEATER,
} endpoint_transport_type_t;

typedef enum {
    ENDPOINT_TRANSPORT_MEDIUM_DECT,
    ENDPOINT_TRANSPORT_MEDIUM_BLE,
} endpoint_transport_medium_t;

typedef void (*endpoint_transport_rx_cb_t)(uint16_t src_id, uint8_t endpoint_id, transport_reassem_ctx_t *reassem_ctx);

/**
 * @typedef endpoint_transport_api_t
 * @brief API vtable for a transport layer (write and register RX callback)
 *
 */
typedef struct endpoint_transport_api_t {
    int (*transport_write)(uint16_t dst_id, uint8_t endpoint_id, const void *data, size_t len_bytes);
    int (*transport_register_rx_cb)(endpoint_transport_rx_cb_t callback);
} endpoint_transport_api_t;

/**
 * @typedef endpoint_transport_t
 * @brief Registered transport providing upstream or downstream connectivity
 *
 */
typedef struct endpoint_transport_t {
    sys_snode_t node;
    endpoint_transport_type_t type;
    endpoint_transport_medium_t medium;
    endpoint_transport_api_t api;
} endpoint_transport_t;

/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/

/**
 * @brief Register a transport with the endpoint router
 *
 * @param transport Pointer to the transport to register
 * @return 0 on success, -EINVAL if transport or its API functions are NULL
 */
int endpoint_router_register_transport(endpoint_transport_t *transport);

#ifdef __cplusplus
}
#endif
#endif /* endpoint_transport_h */
