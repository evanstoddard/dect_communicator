/*
 * Copyright (C) Ovyl
 */

/**
 * @file dect_transport_layer.h
 * @author Evan Stoddard
 * @brief Module to provide reliable data transfer over DECT
 */

#ifndef dect_transport_layer_h
#define dect_transport_layer_h

#include <stdint.h>
#include <stddef.h>

// NOTE: This definitely couples the transport layer with the higher level "hail" layer, but fine for this MVP. This can
// easily be refactored later on as a majority of this layer is fairly decoupled from upper levels of the stack
#include "hail/hail_transport.h"
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

/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/

/**
 * @brief Initialize DECT transport layer
 *
 * @return Returns 0 on success
 */
int dect_transport_layer_init(void);

/**
 * @brief Write data over DECT transport layer (Synchronous and blocking call)
 *
 * @param dst_id Destination ID
 * @param data Pointer to buffer to be written
 * @param len_bytes Length of buffer in bytes
 * @return Returns 0 on success or negative errno
 */
int dect_transport_layer_write(const uint16_t dst_id, const void *data, size_t len_bytes);

/**
 * @brief Register RX callback. NOTE: Will overwrite previously registered callback
 *
 * @param callback Pointer to callback
 * @return Returns 0 on success
 */
int dect_transport_layer_register_rx_callback(hail_transport_rx_callback_t callback);

#ifdef __cplusplus
}
#endif
#endif /* dect_transport_layer_h */
