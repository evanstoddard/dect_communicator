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
#include <string.h>

#include <zephyr/logging/log.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>

#include "utils/device_id.h"

/*****************************************************************************
 * Definitions
 *****************************************************************************/

LOG_MODULE_REGISTER(ble_core);

#define BLE_CORE_NRF5340_RESET_PULSE_MS (10U)
#define BLE_CORE_NRF5340_BOOT_DELAY_MS (3000U)

#define BLE_CORE_MAX_ADV_UUID128 (1U)

static const struct gpio_dt_spec prv_nrf5340_reset =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), nrf5340_reset_gpios);

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

static uint8_t prv_adv_uuid128_buf[BLE_CORE_MAX_ADV_UUID128 * BT_UUID_SIZE_128];
static uint8_t prv_adv_uuid128_count;

static struct bt_data prv_sd_data[BLE_CORE_MAX_ADV_UUID128];

static struct {
    struct k_work adv_work;
} prv_inst;

/*****************************************************************************
 * Private Functions
 *****************************************************************************/

/**
 * @brief Reset the nRF5340 by pulsing its reset line.
 *
 * Ensures the HCI controller starts fresh before bt_enable().
 *
 * @return 0 on success, negative errno on failure
 */
static int prv_reset_nrf5340(void)
{
    if (!gpio_is_ready_dt(&prv_nrf5340_reset)) {
        LOG_ERR("nRF5340 reset GPIO not ready");
        return -ENODEV;
    }

    int ret = gpio_pin_configure_dt(&prv_nrf5340_reset, GPIO_OUTPUT_ACTIVE);
    if (ret) {
        LOG_ERR("Failed to configure nRF5340 reset GPIO: %d", ret);
        return ret;
    }

    /* Hold reset low */
    k_sleep(K_MSEC(BLE_CORE_NRF5340_RESET_PULSE_MS));

    /* Release reset */
    gpio_pin_set_dt(&prv_nrf5340_reset, 0);

    /* Wait for nRF5340 to boot and send NOP */
    LOG_INF("nRF5340 reset released, waiting for boot...");
    k_sleep(K_MSEC(BLE_CORE_NRF5340_BOOT_DELAY_MS));

    return 0;
}

/**
 * @brief Work handler that starts BLE advertising
 *
 * @param work Pointer to the work item
 */
static void prv_advertising_work_handler(struct k_work *work)
{
    const struct bt_data *sd = NULL;
    size_t sd_len = 0;

    if (prv_adv_uuid128_count > 0) {
        prv_sd_data[0].type = BT_DATA_UUID128_ALL;
        prv_sd_data[0].data_len = prv_adv_uuid128_count * BT_UUID_SIZE_128;
        prv_sd_data[0].data = prv_adv_uuid128_buf;
        sd = prv_sd_data;
        sd_len = 1;
    }

    int ret = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, prv_ad_data, ARRAY_SIZE(prv_ad_data), sd, sd_len);
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

    /* Reset the nRF5340 to guarantee a clean HCI controller state.
     * This handles both cold boot and nRF9151 reboot scenarios.
     * The boot delay must be long enough for the nRF5340 to fully
     * initialize cpuapp + cpunet + HCI before bt_enable() sends
     * HCI_Reset, since the HCI timeout assert is fatal. */
    int ret = prv_reset_nrf5340();
    if (ret) {
        LOG_WRN("nRF5340 reset failed: %d, attempting bt_enable anyway", ret);
    }

    ret = bt_enable(NULL);
    if (ret) {
        LOG_ERR("Bluetooth init failed: %d", ret);
        return ret;
    }
    LOG_INF("Bluetooth initialized.");

    k_work_init(&prv_inst.adv_work, prv_advertising_work_handler);

    return 0;
}

int ble_core_register_adv_uuid128(const struct bt_uuid_128 *uuid)
{
    if (prv_adv_uuid128_count >= BLE_CORE_MAX_ADV_UUID128) {
        return -ENOMEM;
    }

    memcpy(&prv_adv_uuid128_buf[prv_adv_uuid128_count * BT_UUID_SIZE_128],
           uuid->val, BT_UUID_SIZE_128);
    prv_adv_uuid128_count++;

    return 0;
}

int ble_core_start_advertising(void)
{
    k_work_submit(&prv_inst.adv_work);
    return 0;
}
