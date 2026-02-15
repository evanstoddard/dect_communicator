/*
 * Copyright (C) Ovyl
 */

/**
 * @file dect_data_layer.c
 * @author Evan Stoddard
 * @brief
 */

#include "dect_data_layer.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>

#include <zephyr/kernel.h>

#include "dect_data_layer_private.h"

/*****************************************************************************
 * Definitions
 *****************************************************************************/

#define DECT_DATA_LAYER_STACK_SIZE_BYTES (1024U)

#define DECT_DATA_LAYER_THREAD_PRIORITY (8)

#define DECT_DATA_LAYER_RX_WINDOW_MS (500U)

#define DECT_DATA_LAYER_TX_MSG_QUEUE_DEPTH (8U)

/*****************************************************************************
 * Variables
 *****************************************************************************/

/**
 * @brief [TODO:description]
 */
static struct {
    bool initialized;

    struct k_thread thread;
    k_tid_t thread_id;

    uint8_t tx_queue_buf[DECT_DATA_LAYER_MAX_PAYLOAD_SIZE_BYTES * DECT_DATA_LAYER_TX_MSG_QUEUE_DEPTH];
    struct k_msgq tx_queue;
} prv_inst;

K_THREAD_STACK_DEFINE(prv_dect_data_layer_thread_stack, DECT_DATA_LAYER_STACK_SIZE_BYTES);

/*****************************************************************************
 * Private Functions
 *****************************************************************************/

/**
 * @brief Data layer processing thread
 *
 * @param arg1 Unused
 * @param arg2 Unused
 * @param arg3 Unused
 */
static void prv_data_layer_thread(void *arg1, void *arg2, void *arg3)
{
    while (true) {
        k_sleep(K_FOREVER);
    }
}

/*****************************************************************************
 * Functions
 *****************************************************************************/

int dect_data_layer_init(void)
{
    if (prv_inst.initialized == true) {
        return -EALREADY;
    }

    prv_inst.thread_id = k_thread_create(&prv_inst.thread, prv_dect_data_layer_thread_stack,
                                         K_THREAD_STACK_SIZEOF(prv_dect_data_layer_thread_stack), prv_data_layer_thread,
                                         NULL, NULL, NULL, DECT_DATA_LAYER_THREAD_PRIORITY, 0, K_NO_WAIT);

    k_msgq_init(&prv_inst.tx_queue, prv_inst.tx_queue_buf, DECT_DATA_LAYER_MAX_PAYLOAD_SIZE_BYTES,
                DECT_DATA_LAYER_TX_MSG_QUEUE_DEPTH);

    prv_inst.initialized = true;
    return 0;
}

int dect_data_layer_write(const uint16_t dst_id, const void *buf, const size_t buf_size_bytes)
{
    if (prv_inst.initialized == false) {
        return -ENOTCONN;
    }

    if (buf == NULL) {
        return -EINVAL;
    }

    if (buf_size_bytes > DECT_DATA_LAYER_MAX_PAYLOAD_SIZE_BYTES) {
        return -ENOMEM;
    }

    return 0;
}
