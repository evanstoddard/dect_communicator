/*
 * Copyright (C) Evan Stoddard
 */

/**
 * @file alfie_transport.h
 * @author Evan Stoddard
 * @brief
 */

#ifndef alfie_transport_h
#define alfie_transport_h

#include <stdint.h>
#include <stddef.h>

#include <zephyr/sys/slist.h>

#include "utils/transport_buffer.h"

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
    ALFIE_TRANSPORT_TYPE_UPSTREAM,
    ALFIE_TRANSPORT_TYPE_DOWNSTREAM,
} alfie_transport_type_t;

struct alfie_transport_t;

typedef void (*alfie_transport_rx_callback_t)(struct alfie_transport_t *transport,
                                             transport_buffer_t *buffer);

/**
 * @typedef alfie_transport_api_t
 * @brief API definition for an ALFIE transport layer
 *
 */
typedef struct alfie_transport_api_t {
    int (*write)(const uint32_t dst_id, const void *data, const size_t len_bytes);
    int (*register_rx_cb)(alfie_transport_rx_callback_t callback);
} alfie_transport_api_t;

/**
 * @typedef alfie_transport_t
 * @brief Definition of an ALFIE transport layer
 *
 */
typedef struct alfie_transport_t {
    sys_snode_t node;
    alfie_transport_api_t *api;
    alfie_transport_type_t type;
} alfie_transport_t;

/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/

#ifdef __cplusplus
}
#endif
#endif /* alfie_transport_h */
