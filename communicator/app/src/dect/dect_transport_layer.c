/*
 * Copyright (C) Ovyl
 */

/**
 * @file dect_transport_layer.c
 * @author Evan Stoddard
 * @brief Module to provide reliable data transfer over DECT
 */

#include "dect_transport_layer.h"

#include <stdbool.h>
#include <errno.h>

/*****************************************************************************
 * Definitions
 *****************************************************************************/

#define DECT_TRANSPORT_LAYER_NUM_RX_BUFFERS (4U)

#define DECT_TRANSPORT_LAYER_MAX_RX_BUFFER_SIZE_BYTES (512U)

/*****************************************************************************
 * Variables
 *****************************************************************************/

/**
 * @brief Private instance
 */
static struct {
    bool initialized;
} prv_inst;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

/*****************************************************************************
 * Functions
 *****************************************************************************/

int dect_transport_layer_init(void)
{
    if (prv_inst.initialized == true) {
        return -EALREADY;
    }

    prv_inst.initialized = true;

    return 0;
}

int dect_transport_layer_write(const uint16_t dst_id, const void *data, size_t len_bytes)
{
}

int dect_transport_layer_register_rx_callback(hail_transport_rx_callback_t callback)
{
}
