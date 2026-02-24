/*
 * Copyright (C) Ovyl
 */

/**
 * @file alfie_messaging_endpoint.c
 * @author Evan Stoddard
 * @brief
 */

#include "alfie_messaging_endpoint.h"

#include <stdbool.h>
#include <stddef.h>

#include <zephyr/logging/log.h>

#include "alfie/endpoints/alfie_endpoint.h"
#include "alfie_messaging_proto.h"

#include "alfie/alfie_router.h"

/*****************************************************************************
 * Definitions
 *****************************************************************************/

LOG_MODULE_REGISTER(alfie_messaging_endpoint);

/*****************************************************************************
 * Variables
 *****************************************************************************/

/**
 * @brief Private instance
 */
static struct {
    alfie_endpoint_t endpoint;

    bool initialized;
} prv_inst;

/*****************************************************************************
 * Private Functions
 *****************************************************************************/

/**
 * @brief Handle test frame
 *
 * @param header Pointer to messaging frame header
 * @param len_bytes Length of frame entire frame in bytes, including all headers
 */
void prv_handle_text_frame(const alfie_messaging_proto_header_t *header, size_t len_bytes)
{
    alfie_messaging_proto_text_frame_t *frame = (alfie_messaging_proto_text_frame_t *)header;

    if (len_bytes < sizeof(alfie_messaging_proto_text_frame_t)) {
        LOG_WRN("Invalid text frame.");
        return;
    }

    size_t msg_len = len_bytes - sizeof(alfie_messaging_proto_text_frame_t);

    LOG_INF("Handling text frame:\r\n"
            "\tSrc ID: 0x%08X\r\n"
            "\tDst ID: 0x%08X\r\n"
            "\tMessage: %.*s",
            frame->header.alfie_header.src_id, frame->header.alfie_header.dst_id, msg_len, frame->message);
}

/**
 * @brief Handle incoming frame
 *
 * @param buffer Pointer to buffer
 */
void prv_handle_incoming_frame(transport_buffer_t *buffer)
{
    if (buffer->total_size_bytes < sizeof(alfie_messaging_proto_header_t)) {
        LOG_WRN("Invalid frame size.");
        return;
    }

    alfie_messaging_proto_header_t *header = (alfie_messaging_proto_header_t *)buffer->buffer->data;

    switch (header->frame_type) {
        case ALFIE_MESSAGING_PROTO_FRAME_TYPE_TEXT:
            prv_handle_text_frame(header, buffer->total_size_bytes);
            break;
        default:
            LOG_WRN("Unknown frame type: 0x%02X", header->frame_type);
            break;
    }
}

/*****************************************************************************
 * Functions
 *****************************************************************************/

int alfie_messaging_endpoint_init(void)
{
    if (prv_inst.initialized == true) {
        return -EALREADY;
    }

    prv_inst.endpoint.endpoint_id = ALFIE_MESSAGING_ENDPOINT_ENDPOINT_ID;
    prv_inst.endpoint.api.on_frame_rx = prv_handle_incoming_frame;

    int ret = alfie_router_register_endpoint((alfie_endpoint_t *)&prv_inst);
    if (ret != 0) {
        return ret;
    }

    prv_inst.initialized = true;

    return 0;
}
