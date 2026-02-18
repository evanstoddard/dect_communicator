/*
 * Copyright (C) Evan Stoddard
 */

/**
 * @file dect_data_layer_private.h
 * @author Evan Stoddard
 * @brief
 */

#ifndef dect_data_layer_private_h
#define dect_data_layer_private_h

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * Definitions
 *****************************************************************************/

#define DECT_DATA_LAYER_HEADER_VERSION (01U)

#define DECT_DATA_LAYER_HEADER_SIZE_BYTES (sizeof(dect_data_layer_header_t))

#define DECT_DATA_LAYER_MAX_PAYLOAD_SIZE_BYTES (94U)

#define DECT_DATA_LAYER_HEADER_MAGIC (0xDEC7DA7A)

/*****************************************************************************
 * Structs, Unions, Enums, & Typedefs
 *****************************************************************************/

/**
 * @typedef dect_data_layer_header_t
 * @brief Data layer header
 *
 */
typedef struct dect_data_layer_header_t {
    uint32_t magic;

    /**
     * @brief Version of data layer header (Currently always 1)
     */
    uint8_t version;

    /**
     * @brief Source short address (little endian)
     */
    uint16_t src_id;

    /**
     * @brief Destination short address (little endian)
     */
    uint16_t dst_id;

    uint16_t payload_size;
} __attribute__((__packed__)) dect_data_layer_header_t;

/**
 * @typedef dect_data_layer_frame_t
 * @brief Entire data layer frame
 *
 */
typedef struct dect_data_layer_frame_t {
    dect_data_layer_header_t header;
    uint8_t payload[DECT_DATA_LAYER_MAX_PAYLOAD_SIZE_BYTES];
} __attribute__((__packed__)) dect_data_layer_frame_t;

/**
 * @typedef dect_data_layer_tx_obj_t
 * @brief Encapsulating object for writing a data frame so meta data can be stored
 *
 */
typedef struct dect_data_layer_tx_obj_t {
    dect_data_layer_frame_t frame;
} dect_data_layer_tx_obj_t;

/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/

#ifdef __cplusplus
}
#endif
#endif /* dect_data_layer_private_h */
