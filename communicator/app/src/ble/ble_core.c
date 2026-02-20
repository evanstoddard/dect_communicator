/*
 * Copyright (C) Evan Stoddard
 */

/**
 * @file ble_core.c
 * @author Evan Stoddard
 * @brief
 */

#include "ble_core.h"

#include <zephyr/logging/log.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>

/*****************************************************************************
 * Definitions
 *****************************************************************************/

LOG_MODULE_REGISTER(ble_core);

/*****************************************************************************
 * Variables
 *****************************************************************************/

/**
 * @brief BLE Advertising Data
 */
static const struct bt_data prv_ad_data[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

/*****************************************************************************
 * Private Functions
 *****************************************************************************/

/**
 * @brief [TODO:description]
 *
 * @param work [TODO:parameter]
 */
static void prv_advertising_work_handler(struct k_work *work)
{
    int ret = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, prv_ad_data, ARRAY_SIZE(prv_ad_data), NULL, 0);
    if (ret) {
        LOG_ERR("Advertising failed to start: %d", ret);
        return ret;
    }

    LOG_INF("Advertising started.");
}

/**
 * @brief Callback called when device connected
 *
 * @param conn Pointer to connection
 * @param err Error connecting to device
 */
static void prv_device_connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("Failed to connect to BLE device: %u", err);
        return;
    }
    else {
        LOG_INF("Connected to BLE device.");
    }
}

/**
 * @brief Callback called when device disconnected
 *
 * @param conn Pointer to connection
 * @param reason Reason for disconnection
 */
static void prv_device_disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Disconnected from device: %u", reason);

    // Explicitly unreference the connection
    bt_conn_unref(conn);
}

BT_CONN_CB_DEFINE(prv_conn_callbacks) = {
    .connected = prv_device_connected,
    .disconnected = prv_device_disconnected,
};

/*****************************************************************************
 * Functions
 *****************************************************************************/

/**
 * @brief [TODO:description]
 *
 * @return [TODO:return]
 */
int ble_core_init(void)
{
    int ret = bt_enable(NULL);
    if (ret) {
        LOG_ERR("Bluetooth init failed: %d", ret);
        return ret;
    }
    LOG_INF("Bluetooth initialized.");

    return 0;
}
