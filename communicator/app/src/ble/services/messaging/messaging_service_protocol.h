/*
 * Copyright (C) Ovyl
 */

/**
 * @file messaging_service_protocol.h
 * @author Evan Stoddard
 * @brief
 */

#ifndef messaging_service_protocol_h
#define messaging_service_protocol_h

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * Definitions
 *****************************************************************************/

#define MESSAGING_SERVICE_MAX_PAYLOAD_SIZE_BYTES (251 - sizeof(messaging_service_frame_header_t))

/*****************************************************************************
 * Structs, Unions, Enums, & Typedefs
 *****************************************************************************/

typedef enum {
    MESSAGING_SERVICE_FRAME_TYPE_ACK = 0x0,
    MESSAGING_SERVICE_FRAME_TYPE_PAYLOAD = 0x1,
    MESSAGING_SERVICE_FRAME_TYPE_RESET = 0xFF,
} __attribute__((__packed__)) messaging_service_frame_type_t;

/**
 * @typedef messaging_service_frame_header_t
 * @brief Header for messaging BLE service frame
 *
 */
typedef struct messaging_service_frame_header_t {
    uint8_t version;
    uint8_t frame_type;
    uint8_t frag_total;
    uint8_t frag_idx;
    uint16_t seq_id;
} __attribute__((__packed__)) messaging_service_frame_header_t;

/**
 * @typedef messaging_service_payload_frame_t
 * @brief Messaging service payload frame
 *
 */
typedef struct messaging_service_payload_frame_t {
    messaging_service_frame_header_t header;
    uint8_t payload[MESSAGING_SERVICE_MAX_PAYLOAD_SIZE_BYTES];
} __attribute__((__packed__)) messaging_service_payload_frame_t;

/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/

#ifdef __cplusplus
}
#endif
#endif /* messaging_service_protocol_h */
