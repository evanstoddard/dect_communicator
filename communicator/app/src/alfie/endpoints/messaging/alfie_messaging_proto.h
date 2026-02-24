/*
 * Copyright (C) Ovyl
 */

/**
 * @file alfie_messaging_proto.h
 * @author Evan Stoddard
 * @brief
 */

#ifndef alfie_messaging_proto_h
#define alfie_messaging_proto_h

#include "alfie/alfie_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * Definitions
 *****************************************************************************/

#define ALFIE_MESSAGE_UUID_SIZE_BYTES (16U)

#define ALFIE_MESSAGING_ENDPOINT_ENDPOINT_ID (0x0)

/*****************************************************************************
 * Structs, Unions, Enums, & Typedefs
 *****************************************************************************/

typedef enum {
    ALFIE_MESSAGING_PROTO_FRAME_TYPE_TEXT,
} __attribute__((__packed__)) alfie_messaging_proto_frame_type_t;

/**
 * @typedef alfie_messaging_proto_header_t
 * @brief Header for alfie messaging endpoint
 *
 */
typedef struct alfie_messaging_proto_header_t {
    alfie_proto_header_t alfie_header;
    uint8_t frame_type;
} __attribute__((__packed__)) alfie_messaging_proto_header_t;

/**
 * @typedef alfie_messaging_proto_text_frame_t
 * @brief Messaging proto frame containing a text message
 *
 */
typedef struct alfie_messaging_proto_text_frame_t {
    alfie_messaging_proto_header_t header;
    uint8_t uuid[ALFIE_MESSAGE_UUID_SIZE_BYTES];
    char message[];
} __attribute__((__packed__)) alfie_messaging_proto_text_frame_t;

/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/

#ifdef __cplusplus
}
#endif
#endif /* alfie_messaging_proto_h */
