/*
 * Copyright (C) Evan Stoddard
 */

/**
 * @file dect_protocol_layer_private.h
 * @author Evan Stoddard
 * @brief
 */

#ifndef dect_protocol_layer_private_h
#define dect_protocol_layer_private_h

#include <stdint.h>
#include <stddef.h>

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
 * @typedef dect_protocol_layer_header_t
 * @brief [TODO:description]
 *
 */
typedef struct dect_protocol_layer_header_t {
    uint8_t version;
    uint8_t msg_type;
    uint8_t flags;
    uint8_t frag_total;
    uint8_t frag_idx;
    uint16_t seq_id;
} __attribute__((__packed__)) dect_protocol_layer_header_t;

/**
 * @typedef dect_protocol_layer_fragment_t
 * @brief [TODO:description]
 *
 */
typedef struct dect_protocol_layer_fragment_t {
    dect_protocol_layer_header_t header;
    uint8_t payload[CONFIG_DECT_PROTO_MAX_FRAGMENT_SIZE_BYTES];
} __attribute__((__packed__)) dect_protocol_layer_fragment_t;

/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/

#ifdef __cplusplus
}
#endif
#endif /* dect_protocol_layer_private_h */
