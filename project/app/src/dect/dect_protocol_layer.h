/*
 * Copyright (C) Ovyl
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
 * @brief [TODO:description]
 *
 * @param dst_id [TODO:parameter]
 * @param msg_type [TODO:parameter]
 * @param data [TODO:parameter]
 * @param len_bytes [TODO:parameter]
 * @return [TODO:return]
 */
int dect_protocol_layer_write(const uint16_t dst_id, const uint8_t msg_type, const void *data, const size_t len_bytes);

#ifdef __cplusplus
}
#endif
#endif /* dect_protocol_layer_h */
