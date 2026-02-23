/*
 * Copyright (C) Ovyl
 */

/**
 * @file hail_transport.h
 * @author Evan Stoddard
 * @brief
 */

#ifndef hail_transport_h
#define hail_transport_h

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
    HAIL_TRANSPORT_TYPE_UPSTREAM,
    HAIL_TRANSPORT_TYPE_DOWNSTREAM,
} hail_transport_type_t;

struct hail_transport_t;

typedef void (*hail_transport_rx_callback_t)(struct hail_transport_t *transport,
                                             transport_buffer_t *buffer);

/**
 * @typedef hail_transport_api_t
 * @brief API definition for a HAIL transport layer
 *
 */
typedef struct hail_transport_api_t {
    int (*write)(const uint16_t src_id, const void *data, const size_t len_bytes);
    int (*register_rx_cb)(hail_transport_rx_callback_t callback);
} hail_transport_api_t;

/**
 * @typedef hail_transport_t
 * @brief Definition of a HAIL transport layer
 *
 */
typedef struct hail_transport_t {
    sys_snode_t node;
    hail_transport_api_t *api;
    hail_transport_type_t type;
} hail_transport_t;

/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/

#ifdef __cplusplus
}
#endif
#endif /* hail_transport_h */
