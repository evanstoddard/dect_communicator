/*
 * Copyright (C) Ovyl
 */

/**
 * @file alfie_ble_service_protocol.h
 * @author Evan Stoddard
 * @brief
 */

#ifndef alfie_ble_service_protocol_h
#define alfie_ble_service_protocol_h

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * Definitions
 *****************************************************************************/

#define ALFIE_BLE_SERVICE_PROTO_MAX_DATA_FRAME_PAYLOAD_SIZE_BYTES                                                      \
    (CONFIG_BT_L2CAP_TX_MTU - sizeof(alfie_ble_service_proto_data_frame_t))

/*****************************************************************************
 * Structs, Unions, Enums, & Typedefs
 *****************************************************************************/

typedef enum {
    ALFIE_BLE_SERVICE_PROTO_FRAME_TYPE_DATA,
    ALFIE_BLE_SERVICE_PROTO_FRAME_TYPE_DATA_ACK,
} __attribute__((__packed__)) alfie_ble_service_proto_frame_type_t;

/**
 * @typedef alfie_ble_service_proto_frame_header_t
 * @brief Header for Alfie BLE service transport layer
 *
 */
typedef struct alfie_ble_service_proto_frame_header_t {
    uint8_t version;
    uint8_t frame_type;
} __attribute__((__packed__)) alfie_ble_service_proto_frame_header_t;

/**
 * @typedef alfie_ble_service_proto_data_frame_t
 * @brief Data frame definition for Alfie BLE service transport layer
 *
 */
typedef struct alfie_ble_service_proto_data_frame_t {
    alfie_ble_service_proto_frame_header_t header;
    uint16_t seq_id;
    uint16_t total_size_bytes;
    uint8_t frag_idx;
    uint8_t frag_total;
    uint8_t payload[];
} __attribute__((__packed__)) alfie_ble_service_proto_data_frame_t;

/**
 * @typedef alfie_ble_service_proto_data_ack_frame_t
 * @brief ACK for data frame
 *
 */
typedef struct alfie_ble_service_proto_data_ack_frame_t {
    alfie_ble_service_proto_frame_header_t header;
    uint16_t seq_id;
    uint8_t frag_idx;
} __attribute__((__packed__)) alfie_ble_service_proto_data_ack_frame_t;

/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/

#ifdef __cplusplus
}
#endif
#endif /* alfie_ble_service_protocol_h */
