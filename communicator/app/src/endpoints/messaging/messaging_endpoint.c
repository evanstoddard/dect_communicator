/*
 * Copyright (C) Ovyl
 */

/**
 * @file messaging_endpoint.c
 * @author Evan Stoddard
 * @brief
 */

#include "messaging_endpoint.h"

#include <stdbool.h>
#include <stddef.h>

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

#include "ble/services/messaging/messaging_service.h"

#include "messaging_endpoint_protocol.h"

/*****************************************************************************
 * Definitions
 *****************************************************************************/

LOG_MODULE_REGISTER(messaging_endpoint);

#define MESSAGING_ENDPOINT_STACK_SIZE_BYTES (1024U)

#define MESSAGING_ENDPOINT_THREAD_PRIORITY (8)

/*****************************************************************************
 * Variables
 *****************************************************************************/

static struct {
    bool initialized;

    struct k_thread thread;
    k_tid_t thread_id;

    // TODO: Can this be better generalized into an "upstream"/"downstream" channeling pattern?  What if our "upstream"
    // is not BLE or we have multiple "upstreams?"
    struct k_fifo from_ble_fifo;
    struct k_fifo from_dect_fifo;
} prv_inst;

K_THREAD_STACK_DEFINE(prv_messaging_endpoint_thread_stack, MESSAGING_ENDPOINT_STACK_SIZE_BYTES);

/*****************************************************************************
 * Private Functions
 *****************************************************************************/

/**
 * @brief [TODO:description]
 *
 * @param buf [TODO:parameter]
 * @param user_data [TODO:parameter]
 */
static void prv_on_ble_rx(struct net_buf *buf, void *user_data)
{
    k_fifo_put(&prv_inst.from_ble_fifo, buf);
}

/**
 * @brief [TODO:description]
 */
static void prv_handle_rx_from_ble(void)
{
    messaging_endpoint_header_t *header = (messaging_endpoint_header_t *)k_fifo_get(&prv_inst.from_ble_fifo, K_NO_WAIT);

    LOG_INF("Received BLE message with msg ID of: 0x%02X", header->msg_type);
}

/**
 * @brief [TODO:description]
 */
static void prv_handle_rx_from_dect(void)
{
}

/**
 * @brief [TODO:description]
 *
 * @param arg1 [TODO:parameter]
 * @param arg2 [TODO:parameter]
 * @param arg3 [TODO:parameter]
 */
static void prv_thread(void *arg1, void *arg2, void *arg3)
{
    static struct k_poll_event events[] = {
        K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_FIFO_DATA_AVAILABLE, K_POLL_MODE_NOTIFY_ONLY,
                                        &prv_inst.from_ble_fifo, 0),
        K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_FIFO_DATA_AVAILABLE, K_POLL_MODE_NOTIFY_ONLY,
                                        &prv_inst.from_dect_fifo, 0),
    };

    while (true) {
        k_poll(events, ARRAY_SIZE(events), K_FOREVER);

        if (events[0].state == K_POLL_STATE_FIFO_DATA_AVAILABLE) {
            prv_handle_rx_from_ble();
        }

        if (events[1].state == K_POLL_STATE_FIFO_DATA_AVAILABLE) {
            prv_handle_rx_from_dect();
        }

        // Reset states
        events[0].state = K_POLL_STATE_NOT_READY;
        events[1].state = K_POLL_STATE_NOT_READY;
    }
}

/*****************************************************************************
 * Functions
 *****************************************************************************/

int messaging_endpoint_init(void)
{
    if (prv_inst.initialized == true) {
        return -EALREADY;
    }

    k_fifo_init(&prv_inst.from_ble_fifo);
    k_fifo_init(&prv_inst.from_dect_fifo);

    static messaging_service_on_payload_frame_callback_t ble_rx_frame_cb = {.callback = prv_on_ble_rx,
                                                                            .user_ctx = NULL};

    messaging_service_register_on_payload_frame(&ble_rx_frame_cb);

    prv_inst.thread_id = k_thread_create(&prv_inst.thread, prv_messaging_endpoint_thread_stack,
                                         K_THREAD_STACK_SIZEOF(prv_messaging_endpoint_thread_stack), prv_thread, NULL,
                                         NULL, NULL, MESSAGING_ENDPOINT_THREAD_PRIORITY, 0, K_NO_WAIT);

    prv_inst.initialized = true;

    return 0;
}
