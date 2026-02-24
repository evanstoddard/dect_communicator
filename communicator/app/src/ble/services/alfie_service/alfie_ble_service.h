/*
 * Copyright (C) Ovyl
 */

/**
 * @file alfie_ble_service.h
 * @author Evan Stoddard
 * @brief Implements BLE based transport layer for Alfie protocol
 */

#ifndef alfie_ble_service_h
#define alfie_ble_service_h

#include <stddef.h>
#include <stdint.h>

#include "alfie/alfie_transport.h"
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
 * @brief Initialize Alfie BLE service
 *
 * @return Returns 0 on success or negative errno on error
 */
int alfie_ble_service_init(void);

/**
 * @brief Write data over BLE transport layer (Synchronous and blocking)
 *
 * @param dst_id Destination ID (Currently ignored by this module)
 * @param data Pointer to buffer to be written
 * @param len_bytes Length of buffer in bytes
 * @return Returns 0 on success or negative errno
 */
int alfie_ble_service_write(const uint32_t dst_id, const void *data, size_t len_bytes);

/**
 * @brief Register RX callback. NOTE: Will overwrite previously registered callback.
 *
 * @param callback Pointer to callback
 * @return Returns 0 on success
 */
int alfie_ble_service_register_rx_callback(alfie_transport_rx_callback_t callback);

/**
 * @brief Get transport instance for registration with router
 *
 * @return Pointer to transport instance, or NULL if not initialized
 */
alfie_transport_t *alfie_ble_service_get_transport(void);

#ifdef __cplusplus
}
#endif
#endif /* alfie_ble_service_h */
