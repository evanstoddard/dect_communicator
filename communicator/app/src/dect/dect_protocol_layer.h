/*
 * Copyright (C) Evan Stoddard
 */

/**
 * @file dect_protocol_layer.h
 * @author Evan Stoddard
 * @brief
 */

#ifndef dect_protocol_layer_h
#define dect_protocol_layer_h

#include <stdint.h>
#include <stddef.h>

#include "endpoints/endpoint_transport.h"

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
 * @brief Initialize protocol layer
 *
 * @return Return status
 */
int dect_protocol_layer_init(void);

/**
 * @brief Write data over the protocol layer with fragmentation and ACK-based reliability
 *
 * @param dst_id Destination device ID
 * @param frame_type Frame type identifier (e.g. ACK or endpoint)
 * @param endpoint_id Target endpoint ID
 * @param data Pointer to data to send
 * @param len_bytes Length of data in bytes
 * @return 0 on success, negative errno on failure
 */
int dect_protocol_layer_write(const uint16_t dst_id, const uint8_t frame_type, const uint8_t endpoint_id,
                              const void *data, const size_t len_bytes);

/**
 * @brief Write an endpoint frame over the protocol layer
 *
 * @param dst_id Destination device ID
 * @param endpoint_id Target endpoint ID
 * @param data Pointer to data to send
 * @param len_bytes Length of data in bytes
 * @return 0 on success, negative errno on failure
 */
int dect_protocol_layer_write_endpoint_frame(uint16_t dst_id, uint8_t endpoint_id, const void *data, size_t len_bytes);

/**
 * @brief Register a callback for received protocol layer messages
 *
 * @param callback Callback invoked when a complete message is received
 * @return 0 on success, -EINVAL if callback is NULL
 */
int dect_protocol_layer_register_rx_callback(endpoint_transport_rx_cb_t callback);

#ifdef __cplusplus
}
#endif
#endif /* dect_protocol_layer_h */
