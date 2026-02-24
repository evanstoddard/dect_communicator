/*
 * Copyright (C) Ovyl
 */

/**
 * @file alfie_protocol.h
 * @author Evan Stoddard
 * @brief Alfie protocol definition
 */

#ifndef alfie_protocol_h
#define alfie_protocol_h

#include <stdint.h>

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
 * @typedef alfie_proto_header_t
 * @brief Header for Alfie protocol
 *
 */
typedef struct alfie_proto_header_t {
    uint8_t version;
    uint8_t endpoint_id;
    uint32_t src_id;
    uint32_t dst_id;
} __attribute__((__packed__)) alfie_proto_header_t;

/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/

#ifdef __cplusplus
}
#endif
#endif /* alfie_protocol_h */
