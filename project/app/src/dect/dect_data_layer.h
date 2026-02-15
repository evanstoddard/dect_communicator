/*
 * Copyright (C) Ovyl
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

int dect_data_layer_write(const uint16_t dst_id, const void *buf, const size_t buf_size_bytes);

#ifdef __cplusplus
}
#endif
#endif /* dect_data_layer_h */
