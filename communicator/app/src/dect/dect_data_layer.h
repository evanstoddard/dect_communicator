/*
 * Copyright (C) Evan Stoddard
 */

/**
 * @file dect_data_layer.h
 * @author Evan Stoddard
 * @brief
 */

#ifndef dect_data_layer_h
#define dect_data_layer_h

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

typedef void (*dect_data_layer_rx_cb_t)(const uint16_t src_id, const void *buf, const size_t len_bytes);

/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/

/**
 * @brief Initialize data layer for DECT NR+
 *
 * @return Return status of initialization
 * @retval -EALREADY Already initialized
 * @retval 0 Successful initializaion
 */
int dect_data_layer_init(void);

/**
 * @brief Write data over the DECT NR+ PHY data layer
 *
 * @param dst_id Destination device ID
 * @param buf Pointer to data buffer to transmit
 * @param buf_size_bytes Size of the data buffer in bytes
 * @return 0 on success, negative errno on failure
 */
int dect_data_layer_write(const uint16_t dst_id, const void *buf, const size_t buf_size_bytes);

/**
 * @brief Register RX callback with data layer. Calling this will overwrite any previously registered callback
 *
 * @param callback Pointer to callback
 * @return 0 on success, negative errno on failure
 */
int dect_data_layer_register_rx_callback(dect_data_layer_rx_cb_t callback);

#ifdef __cplusplus
}
#endif
#endif /* dect_data_layer_h */
