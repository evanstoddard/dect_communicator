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
 * @brief [TODO:description]
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
 * @brief [TODO:description]
 *
 * @param dst_id [TODO:parameter]
 * @param buf [TODO:parameter]
 * @param buf_size_bytes [TODO:parameter]
 * @return [TODO:return]
 */
int dect_data_layer_write(const uint16_t dst_id, const void *buf, const size_t buf_size_bytes);

/**
 * @brief [TODO:description]
 *
 * @param rx_handler [TODO:parameter]
 * @return [TODO:return]
 */
int dect_data_layer_register_rx_handler(dect_data_layer_rx_handler_t *rx_handler);

#ifdef __cplusplus
}
#endif
#endif /* dect_data_layer_h */
