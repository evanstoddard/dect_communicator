/*
 * Copyright (C) Evan Stoddard
 */

/**
 * @file dect_link_layer_private.h
 * @author Evan Stoddard
 * @brief
 */

#ifndef dect_link_layer_private_h
#define dect_link_layer_private_h

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * Definitions
 *****************************************************************************/

#define DECT_LINK_LAYER_HEADER_VERSION (01U)

#define DECT_LINK_LAYER_HEADER_SIZE_BYTES (sizeof(dect_link_layer_header_t))

#define DECT_LINK_LAYER_MTU_SIZE_BYTES (37U)

#define DECT_LINK_LAYER_MAX_PAYLOAD_SIZE_BYTES (DECT_LINK_LAYER_MTU_SIZE_BYTES - DECT_LINK_LAYER_HEADER_SIZE_BYTES)

#define DECT_LINK_LAYER_HEADER_MAGIC (0xDEC7DA7A)

/*****************************************************************************
 * Structs, Unions, Enums, & Typedefs
 *****************************************************************************/

/**
 * @typedef dect_link_layer_header_t
 * @brief Link layer header
 *
 */
typedef struct dect_link_layer_header_t {
    uint32_t magic;

    /**
     * @brief Version of link layer header (Currently always 1)
     */
    uint8_t version;

    /**
     * @brief Source address (little endian)
     */
    uint32_t src_id;

    /**
     * @brief Destination address (little endian)
     */
    uint32_t dst_id;

    uint16_t payload_size;
} __attribute__((__packed__)) dect_link_layer_header_t;

/**
 * @typedef dect_link_layer_frame_t
 * @brief Entire link layer frame
 *
 */
typedef struct dect_link_layer_frame_t {
    dect_link_layer_header_t header;
    uint8_t payload[DECT_LINK_LAYER_MAX_PAYLOAD_SIZE_BYTES];
} __attribute__((__packed__)) dect_link_layer_frame_t;

/**
 * @typedef dect_link_layer_tx_obj_t
 * @brief Encapsulating object for writing a link layer frame so meta data can be stored
 *
 */
typedef struct dect_link_layer_tx_obj_t {
    dect_link_layer_frame_t frame;
} dect_link_layer_tx_obj_t;

/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/

#ifdef __cplusplus
}
#endif
#endif /* dect_link_layer_private_h */
