/*
 * Copyright (C) Ovyl
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

#define DECT_DATA_LAYER_MAX_PAYLOAD_SIZE_BYTES (64U)

/*****************************************************************************
 * Structs, Unions, Enums, & Typedefs
 *****************************************************************************/

/**
 * @typedef dect_data_layer_header_t
 * @brief Data layer header
 *
 */
typedef struct dect_data_layer_header_t {
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
} __attribute__((__packed__)) dect_data_layer_header_t;

/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/

#ifdef __cplusplus
}
#endif
#endif /* dect_data_layer_private_h */
