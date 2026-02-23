/*
 * Copyright (C) Ovyl
 */

/**
 * @file dect_transport_layer_private.h
 * @author Evan Stoddard
 * @brief
 */

#ifndef dect_transport_layer_private_h
#define dect_transport_layer_private_h

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

typedef enum {
    DECT_TRANSPORT_LAYER_FRAME_TYPE_DATA,
    DECT_TRANSPORT_LAYER_FRAME_TYPE_DATA_ACK,
} __attribute__((__packed__)) dect_transport_layer_frame_type_t;

/**
 * @typedef dect_transport_layer_frame_header_t
 * @brief Base header for all communications over DECT transport layer
 *
 */
typedef struct dect_transport_layer_frame_header_t {
    uint8_t version;
    uint8_t frame_type;
} __attribute__((__packed__)) dect_transport_layer_frame_header_t;

/**
 * @typedef dect_transport_layer_ack_frame_t
 * @brief ACK for data frame
 *
 */
typedef struct dect_transport_layer_data_frame_ack_t {
    dect_transport_layer_frame_header_t header;
    uint16_t seq_id;
    uint8_t frag_idx;
} __attribute__((__packed__)) dect_transport_layer_ack_frame_t;

/**
 * @typedef dect_transport_layer_data_frame_t
 * @brief [TODO:description]
 *
 */
typedef struct dect_transport_layer_data_frame_t {
    dect_transport_layer_frame_header_t header;
    uint16_t seq_id;
    uint8_t frag_total;
    uint8_t frag_idx;
    uint16_t total_size_bytes;
    uint8_t payload[]
} __attribute__((__packed__)) dect_transport_layer_data_frame_t;

/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/

#ifdef __cplusplus
}
#endif
#endif /* dect_transport_layer_private_h */
