/*
 * Copyright (C) Ovyl
 */

/**
 * @file messaging_endpoint_protocol.h
 * @author Evan Stoddard
 * @brief
 */

#ifndef messaging_endpoint_protocol_h
#define messaging_endpoint_protocol_h

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * Definitions
 *****************************************************************************/

/**
 * @brief Max size of a messaging endpoint message (including header)
 */
#define MESSAGING_ENDPOINT_MAX_SIZE_BYTES (512U)

#define MESSAGING_ENDPOINT_TEXT_MESSAGE_MAX_SIZE_BYTES                                                                 \
    (MESSAGING_ENDPOINT_MAX_SIZE_BYTES - sizeof(messaging_endpoint_header_t)                                           \
     - sizeof(messaging_endpoint_text_message_metadata_t))

#define MESSAGING_ENDPOINT_MESSAGE_UUID_SIZE_BYTES (16U)

/*****************************************************************************
 * Structs, Unions, Enums, & Typedefs
 *****************************************************************************/

typedef enum {
    MESSAGING_ENDPOINT_MSG_TYPE_TEXT,
} __attribute__((__packed__)) messaging_endpoint_msg_type_t;

/**
 * @typedef messaging_endpoint_header_t
 * @brief [TODO:description]
 *
 */
typedef struct messaging_endpoint_header_t {
    uint8_t version;
    uint8_t msg_type;
} __attribute__((__packed__)) messaging_endpoint_header_t;

typedef struct messaging_endpoint_text_message_metadata_t {
    uint16_t dst_id;
    uint8_t uuid[MESSAGING_ENDPOINT_MESSAGE_UUID_SIZE_BYTES];
} __attribute__((__packed__)) messaging_endpoint_text_message_metadata_t;

/**
 * @typedef messaging_endpoint_text_message_t
 * @brief [TODO:description]
 *
 */
typedef struct messaging_endpoint_text_message_t {
    messaging_endpoint_header_t header;
    messaging_endpoint_text_message_metadata_t meta;
    char payload[MESSAGING_ENDPOINT_TEXT_MESSAGE_MAX_SIZE_BYTES];
} __attribute__((__packed__)) messaging_endpoint_text_message_t;

#ifdef __cplusplus
}
#endif
#endif /* messaging_endpoint_protocol_h */
