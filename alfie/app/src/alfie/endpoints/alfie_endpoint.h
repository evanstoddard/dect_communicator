/*
 * Copyright (C) Evan Stoddard
 */

/**
 * @file alfie_endpoint.h
 * @author Evan Stoddard
 * @brief Definition of an Alfie endpoint
 */

#ifndef alfie_endpoint_h
#define alfie_endpoint_h

#include <stdint.h>

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

struct alfie_endpoint_t;

/**
 * @typedef alfie_endpoint_api_t
 * @brief Definition of endpoint API
 *
 */
typedef struct alfie_endpoint_api_t {
    void (*on_frame_rx)(transport_buffer_t *buffer);
} alfie_endpoint_api_t;

/**
 * @typedef alfie_endpoint_t
 * @brief Defintion of an Alfie endpoint
 *
 */
typedef struct alfie_endpoint_t {
    sys_snode_t node;
    alfie_endpoint_api_t api;

    uint8_t endpoint_id;
} alfie_endpoint_t;

/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/

#ifdef __cplusplus
}
#endif
#endif /* alfie_endpoint_h */
