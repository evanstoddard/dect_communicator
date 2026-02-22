/*
 * Copyright (C) Evan Stoddard
 */

/**
 * @file ble_core.c
 * @author Evan Stoddard
 * @brief
 */

#include "ble_core.h"

#include <stdio.h>

#include <zephyr/logging/log.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>

#include "utils/device_id.h"

/*****************************************************************************
 * Definitions
 *****************************************************************************/

LOG_MODULE_REGISTER(ble_core);

/*****************************************************************************
 * Variables
 *****************************************************************************/

/**
 * @brief BLE Device Name
 */
static char prv_device_name[16];

/**
 * @brief BLE Advertising Data
 */
static struct bt_data prv_ad_data[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    {0},
};

static struct {
    struct k_work adv_work;
} prv_inst;

/*****************************************************************************
 * Private Functions
 *****************************************************************************/

/**
 * @brief Work handler that starts BLE advertising
 *
 * @param work Pointer to the work item
 */
static void prv_advertising_work_handler(struct k_work *work)
{
    int ret = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, prv_ad_data, ARRAY_SIZE(prv_ad_data), NULL, 0);
    if (ret) {
        LOG_ERR("Advertising failed to start: %d", ret);
        return;
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

    k_work_submit(&prv_inst.adv_work);
}

BT_CONN_CB_DEFINE(prv_conn_callbacks) = {
    .connected = prv_device_connected,
    .disconnected = prv_device_disconnected,
};

/*****************************************************************************
 * Functions
 *****************************************************************************/

/**
 * @brief Initialize BLE core, enable Bluetooth, and start advertising
 *
 * @return 0 on success, negative errno on failure
 */
int ble_core_init(void)
{
    int len = snprintf(prv_device_name, sizeof(prv_device_name), "DECT-%u", device_id());
    prv_ad_data[1].type = BT_DATA_NAME_COMPLETE;
    prv_ad_data[1].data_len = len;
    prv_ad_data[1].data = (const uint8_t *)prv_device_name;

    int ret = bt_enable(NULL);
    if (ret) {
        LOG_ERR("Bluetooth init failed: %d", ret);
        return ret;
    }
    LOG_INF("Bluetooth initialized.");

    k_work_init(&prv_inst.adv_work, prv_advertising_work_handler);

    k_work_submit(&prv_inst.adv_work);

    return 0;
}
