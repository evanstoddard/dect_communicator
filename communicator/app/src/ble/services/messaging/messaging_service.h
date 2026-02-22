/*
 * Copyright (C) Ovyl
 */

/**
 * @file messaging_service.h
 * @author Evan Stoddard
 * @brief
 */

#ifndef messaging_service_h
#define messaging_service_h

#include "messaging_service_protocol.h"

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
 * @brief Initialize the BLE messaging service and register its transport with the endpoint router
 *
 * @return 0 on success, negative errno on failure
 */
int messaging_service_init(void);

/**
 * @brief Write data to a connected BLE client via the messaging service GATT characteristic
 *
 * @param dst_id Destination device ID (unused, BLE writes to connected client)
 * @param endpoint_id Target endpoint ID (must be ENDPOINT_ID_MESSAGING_ENDPOINT)
 * @param data Pointer to data to send
 * @param len_bytes Length of data in bytes
 * @return 0 on success, negative errno on failure
 */
int messaging_service_write(uint16_t dst_id, uint8_t endpoint_id, const void *data, const size_t len_bytes);

/**
 * @brief Register a callback for messages received from the BLE client
 *
 * @param callback Callback invoked when a complete message is received
 * @return 0 on success, -EINVAL if callback is NULL
 */
int messaging_service_register_rx_callback(endpoint_transport_rx_cb_t callback);

#ifdef __cplusplus
}
#endif
#endif /* messaging_service_h */
