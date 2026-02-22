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

/**
 * @typedef dect_data_layer_rx_handler_t
 * @brief RX handler registered with the data layer to receive incoming frames
 *
 */
typedef struct dect_data_layer_rx_handler_t {
    void (*handler)(const uint16_t src_id, const void *buf, const size_t len_bytes, void *ctx);
    void *ctx;
} dect_data_layer_rx_handler_t;

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
 * @brief Register a handler for incoming data layer frames
 *
 * @param rx_handler Pointer to the RX handler to register
 * @return 0 on success, -EINVAL if rx_handler or its handler function is NULL
 */
int dect_data_layer_register_rx_handler(dect_data_layer_rx_handler_t *rx_handler);

#ifdef __cplusplus
}
#endif
#endif /* dect_data_layer_h */
