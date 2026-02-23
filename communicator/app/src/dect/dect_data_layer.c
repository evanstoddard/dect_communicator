/*
 * Copyright (C) Evan Stoddard
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
#include <stdlib.h>

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>

#include <nrf_modem_dect_phy.h>
#include <modem/nrf_modem_lib.h>

#include "dect_data_layer_private.h"

#include "utils/device_id.h"

/*****************************************************************************
 * Definitions
 *****************************************************************************/

LOG_MODULE_REGISTER(dect_data_layer);

#define DECT_DATA_LAYER_STACK_SIZE_BYTES (1024U)

#define DECT_DATA_LAYER_THREAD_PRIORITY (8)

#define DECT_DATA_LAYER_RX_WINDOW_MS (100U)

#define DECT_DATA_LAYER_TX_MSG_QUEUE_DEPTH (8U)

/*****************************************************************************
 * Variables
 *****************************************************************************/

struct phy_ctrl_field_common {
    uint32_t packet_length : 4;
    uint32_t packet_length_type : 1;
    uint32_t header_format : 3;
    uint32_t short_network_id : 8;
    uint32_t transmitter_id_hi : 8;
    uint32_t transmitter_id_lo : 8;
    uint32_t df_mcs : 3;
    uint32_t reserved : 1;
    uint32_t transmit_power : 4;
    uint32_t pad : 24;
};

/**
 * @brief Private instance
 */
static struct {
    bool initialized;

    struct k_thread thread;
    k_tid_t thread_id;

    dect_data_layer_tx_obj_t tx_queue_buf[DECT_DATA_LAYER_TX_MSG_QUEUE_DEPTH];
    struct k_msgq tx_queue;

    volatile bool op_failed;
    volatile bool op_cancelled;
    volatile int op_return_code;

    volatile bool valid_rx_data;

    dect_data_layer_frame_t rx_frame;
    volatile size_t rx_frame_size_bytes;

    struct k_sem op_sem;

    dect_data_layer_rx_cb_t rx_callback;
} prv_inst;

/**
 * @brief Base DECT PHY Params
 */
static struct nrf_modem_dect_phy_config_params prv_dect_config_params = {
    .band_group_index = ((CONFIG_CARRIER >= 525 && CONFIG_CARRIER <= 551)) ? 1 : 0,
    .harq_rx_process_count = 4,
    .harq_rx_expiry_time_us = 5000000,
};

K_THREAD_STACK_DEFINE(prv_dect_data_layer_thread_stack, DECT_DATA_LAYER_STACK_SIZE_BYTES);

/*****************************************************************************
 * PHY Control & Handlers
 *****************************************************************************/

/**
 * @brief Event handler for PHY initialization
 *
 * @param event Pointer to event
 */
static void prv_on_phy_init_event(const struct nrf_modem_dect_phy_init_event *event)
{
    if (event->err) {
        prv_inst.op_failed = true;
        prv_inst.op_return_code = event->err;
    }

    k_sem_give(&prv_inst.op_sem);
}

/**
 * @brief Event handler for PHY configuration
 *
 * @param event Pointer to event
 */
static void prv_on_phy_config_event(const struct nrf_modem_dect_phy_configure_event *event)
{
    if (event->err) {
        prv_inst.op_failed = true;
        prv_inst.op_return_code = event->err;
    }

    k_sem_give(&prv_inst.op_sem);
}

/**
 * @brief Event handler for PHY operation completion
 *
 * @param event Pointer to event
 */
static void prv_on_phy_op_completed(const struct nrf_modem_dect_phy_op_complete_event *event)
{
    if (event->err) {
        prv_inst.op_failed = true;
        prv_inst.op_return_code = event->err;
    }

    k_sem_give(&prv_inst.op_sem);
}

/**
 * @brief Event handler for PHY activation
 *
 * @param event Pointer to event
 */
static void prv_on_phy_activated(const struct nrf_modem_dect_phy_activate_event *event)
{
    if (event->err) {
        prv_inst.op_failed = true;
        prv_inst.op_return_code = event->err;
    }

    k_sem_give(&prv_inst.op_sem);
}

/**
 * @brief Event handler for PHY physical data channel (PDC) reception
 *
 * @param event Pointer to PDC event containing received data
 */
static void prv_on_phy_pdc_event(const struct nrf_modem_dect_phy_pdc_event *event)
{
    if (event->len < sizeof(dect_data_layer_header_t) || event->len > sizeof(dect_data_layer_frame_t)) {
        LOG_WRN("Invalid data layer frame size.");
        return;
    }

    dect_data_layer_frame_t *frame = (dect_data_layer_frame_t *)event->data;
    if (frame->header.magic != DECT_DATA_LAYER_HEADER_MAGIC) {
        LOG_WRN("Invalid data layer magic.");
        return;
    }

    // Check if frame is for us
    if (frame->header.dst_id != device_id()) {
        return;
    }

    memcpy(&prv_inst.rx_frame, frame, event->len);
    prv_inst.rx_frame_size_bytes = event->len;
    prv_inst.valid_rx_data = true;
}

/*****************************************************************************
 * Private Functions
 *****************************************************************************/

/**
 * @brief Initialize DECT PHY
 *
 * @return Return status
 */
static int prv_init_phy(void)
{
    LOG_INF("Initializing PHY...");

    prv_inst.op_return_code = 0;
    prv_inst.op_cancelled = false;
    prv_inst.op_failed = false;

    int ret = nrf_modem_dect_phy_init();
    if (ret != 0) {
        return ret;
    }

    k_sem_take(&prv_inst.op_sem, K_FOREVER);

    if (prv_inst.op_cancelled) {
        return -ECANCELED;
    }

    if (prv_inst.op_failed) {
        return prv_inst.op_return_code;
    }

    return 0;
}

/**
 * @brief Configure PHY
 *
 * @return Return status
 */
static int prv_config_phy(void)
{
    LOG_INF("Configuring PHY...");

    prv_inst.op_return_code = 0;
    prv_inst.op_cancelled = false;
    prv_inst.op_failed = false;

    int ret = nrf_modem_dect_phy_configure(&prv_dect_config_params);
    if (ret != 0) {
        return ret;
    }

    k_sem_take(&prv_inst.op_sem, K_FOREVER);

    if (prv_inst.op_cancelled) {
        return -ECANCELED;
    }

    if (prv_inst.op_failed) {
        return prv_inst.op_return_code;
    }

    return 0;
}

/**
 * @brief Activate PHY
 *
 * @return Return status
 */
static int prv_phy_activate(void)
{
    LOG_INF("Activating PHY...");

    prv_inst.op_return_code = 0;
    prv_inst.op_cancelled = false;
    prv_inst.op_failed = false;

    int ret = nrf_modem_dect_phy_activate(NRF_MODEM_DECT_PHY_RADIO_MODE_LOW_LATENCY);
    if (ret != 0) {
        return ret;
    }

    k_sem_take(&prv_inst.op_sem, K_FOREVER);

    if (prv_inst.op_cancelled) {
        return -ECANCELED;
    }

    if (prv_inst.op_failed) {
        return prv_inst.op_return_code;
    }

    return 0;
}

/**
 * @brief DECT NR+ Phy Event Handler
 *
 * @param event Pointer to event
 */
static void prv_dect_phy_event_handler(const struct nrf_modem_dect_phy_event *event)
{
    switch (event->id) {
        case NRF_MODEM_DECT_PHY_EVT_INIT:
            prv_on_phy_init_event(&event->init);
            break;
        case NRF_MODEM_DECT_PHY_EVT_CONFIGURE:
            prv_on_phy_config_event(&event->configure);
            break;
        case NRF_MODEM_DECT_PHY_EVT_COMPLETED:
            prv_on_phy_op_completed(&event->op_complete);
            break;
        case NRF_MODEM_DECT_PHY_EVT_PDC:
            prv_on_phy_pdc_event(&event->pdc);
            break;
        case NRF_MODEM_DECT_PHY_EVT_ACTIVATE:
            prv_on_phy_activated(&event->activate);
            break;
        default:
            break;
    }
}

/**
 * @brief Receive data over PHY
 *
 * @return Return status
 */
static int prv_dect_receive(void)
{
    prv_inst.op_return_code = 0;
    prv_inst.op_cancelled = false;
    prv_inst.op_failed = false;
    prv_inst.valid_rx_data = false;

    struct nrf_modem_dect_phy_rx_params rx_op_params = {
        .start_time = 0,
        .handle = 1, // FIXME: Hard code to handle 1 for now
        .network_id = CONFIG_NETWORK_ID,
        .mode = NRF_MODEM_DECT_PHY_RX_MODE_SINGLE_SHOT,
        .rssi_interval = NRF_MODEM_DECT_PHY_RSSI_INTERVAL_OFF,
        .link_id = NRF_MODEM_DECT_PHY_LINK_UNSPECIFIED,
        .rssi_level = -60,
        .carrier = CONFIG_CARRIER,
        .duration = DECT_DATA_LAYER_RX_WINDOW_MS * NRF_MODEM_DECT_MODEM_TIME_TICK_RATE_KHZ,
        .filter.short_network_id = CONFIG_NETWORK_ID & 0xff,
        .filter.is_short_network_id_used = 1,
        /* listen for everything (broadcast mode used) */
        .filter.receiver_identity = 0,
    };

    int ret = nrf_modem_dect_phy_rx(&rx_op_params);
    if (ret != 0) {
        return ret;
    }

    k_sem_take(&prv_inst.op_sem, K_FOREVER);

    if (prv_inst.op_cancelled) {
        return -ECANCELED;
    }

    if (prv_inst.op_failed) {
        return prv_inst.op_return_code;
    }

    return 0;
}

/**
 * @brief Write queued TX frame out over PHY
 *
 * @param tx_obj Pointer to TX object
 * @return Return status
 */
static int prv_data_layer_write_frame(const dect_data_layer_tx_obj_t *tx_obj)
{
    struct phy_ctrl_field_common header = {.header_format = 0x0,
                                           .packet_length_type = 0x0,
                                           .packet_length = 0x01,
                                           .short_network_id = (CONFIG_NETWORK_ID & 0xFF),
                                           .transmitter_id_hi = (device_id() >> 8),
                                           .transmitter_id_lo = (device_id() & 0xFF),
                                           .transmit_power = CONFIG_TX_POWER,
                                           .reserved = 0,
                                           .df_mcs = CONFIG_MCS};

    struct nrf_modem_dect_phy_tx_params tx_op_params = {
        .start_time = 0,
        // Handle fixed to 0 at the moment
        .handle = 0,
        .network_id = CONFIG_NETWORK_ID,
        .phy_type = 0,
        .lbt_rssi_threshold_max = 0,
        .carrier = CONFIG_CARRIER,
        .lbt_period = NRF_MODEM_DECT_LBT_PERIOD_MAX,
        .phy_header = (union nrf_modem_dect_phy_hdr *)&header,
        .data = (uint8_t *)&tx_obj->frame,
        .data_size = sizeof(dect_data_layer_frame_t),
    };

    int ret = nrf_modem_dect_phy_tx(&tx_op_params);
    if (ret != 0) {
        return ret;
    }

    k_sem_take(&prv_inst.op_sem, K_FOREVER);

    if (prv_inst.op_cancelled) {
        return -ECANCELED;
    }

    if (prv_inst.op_failed) {
        return prv_inst.op_return_code;
    }

    return 0;
}

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
        dect_data_layer_tx_obj_t tx_obj = {0};
        int ret = k_msgq_get(&prv_inst.tx_queue, &tx_obj, K_NO_WAIT);

        if (ret == 0) {
            ret = prv_data_layer_write_frame(&tx_obj);
            if (ret != 0) {
                LOG_ERR("Failed to write frame: %d", ret);
            }
        }

        ret = prv_dect_receive();

        if (prv_inst.valid_rx_data == false) {
            continue;
        }

        if (prv_inst.rx_callback == NULL) {
            continue;
        }

        prv_inst.rx_callback(prv_inst.rx_frame.header.src_id, prv_inst.rx_frame.payload,
                             prv_inst.rx_frame.header.payload_size);
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

    k_msgq_init(&prv_inst.tx_queue, (char *)prv_inst.tx_queue_buf, DECT_DATA_LAYER_MAX_PAYLOAD_SIZE_BYTES,
                DECT_DATA_LAYER_TX_MSG_QUEUE_DEPTH);

    k_sem_init(&prv_inst.op_sem, 0, 1);

    int ret = nrf_modem_lib_init();
    if (ret != 0) {
        LOG_ERR("Failed to initialize nRF modem library: %d", ret);
        return ret;
    }

    ret = nrf_modem_dect_phy_event_handler_set(prv_dect_phy_event_handler);
    if (ret != 0) {
        LOG_ERR("Failed to register DECT PHY event handler: %d", ret);
        return ret;
    }

    ret = prv_init_phy();
    if (ret != 0) {
        LOG_ERR("Failed to initialize DECT PHY: %d", ret);
        return ret;
    }

    ret = prv_config_phy();
    if (ret != 0) {
        LOG_ERR("Failed to configure DECT PHY: %d", ret);
        return ret;
    }

    ret = prv_phy_activate();
    if (ret != 0) {
        LOG_ERR("Failed to activate DECT PHY: %d", ret);
        return ret;
    }

    prv_inst.thread_id = k_thread_create(&prv_inst.thread, prv_dect_data_layer_thread_stack,
                                         K_THREAD_STACK_SIZEOF(prv_dect_data_layer_thread_stack), prv_data_layer_thread,
                                         NULL, NULL, NULL, DECT_DATA_LAYER_THREAD_PRIORITY, 0, K_NO_WAIT);

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

    dect_data_layer_tx_obj_t tx_obj = {.frame.header = {.src_id = device_id(),
                                                        .dst_id = dst_id,
                                                        .magic = DECT_DATA_LAYER_HEADER_MAGIC,
                                                        .version = 1,
                                                        .payload_size = buf_size_bytes}};

    memcpy(&tx_obj.frame.payload, buf, buf_size_bytes);

    int ret = k_msgq_put(&prv_inst.tx_queue, &tx_obj, K_FOREVER);

    return ret;
}

int dect_data_layer_register_rx_callback(dect_data_layer_rx_cb_t callback)
{
    if (prv_inst.initialized == false) {
        return -ENODEV;
    }

    if (callback == NULL) {
        return -EINVAL;
    }

    prv_inst.rx_callback = callback;

    return 0;
}
