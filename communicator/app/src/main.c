/*
 * Copyright (C) Evan Stoddard
 */

/**
 * @file main.c
 * @brief
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>

#include "dect/dect_data_layer.h"
#include "dect/dect_protocol_layer.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/*****************************************************************************
 * Definitions
 *****************************************************************************/

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

/*****************************************************************************
 * Variables
 *****************************************************************************/

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

/*****************************************************************************
 * Functions
 *****************************************************************************/

int main(void)
{
    LOG_INF("Application started.");

    int ret = dect_data_layer_init();

    if (ret != 0) {
        LOG_ERR("Failed to initialize DECT data layer: %d", ret);
        return ret;
    }
    LOG_INF("DECT Data Layer Initialized!");

    ret = dect_protocol_layer_init();
    if (ret != 0) {
        LOG_ERR("Failed to initialize DECT protocol layer: %d", ret);
        return ret;
    }

    LOG_INF("DECT Protocol Layer Initialized!");

    ret = bt_enable(NULL);
    if (ret) {
        LOG_ERR("Bluetooth init failed: %d", ret);
        return ret;
    }
    LOG_INF("Bluetooth initialized.");

    ret = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), NULL, 0);
    if (ret) {
        LOG_ERR("Advertising failed to start: %d", ret);
        return ret;
    }
    LOG_INF("Advertising started.");

    return 0;
}
