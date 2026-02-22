/*
 * Copyright (C) Evan Stoddard
 */

/**
 * @file main.c
 * @brief
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "dect/dect_data_layer.h"
#include "dect/dect_protocol_layer.h"

#include "ble/ble_core.h"

#include "ble/services/messaging/messaging_service.h"

#include "endpoints/messaging/messaging_endpoint.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/*****************************************************************************
 * Definitions
 *****************************************************************************/

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

    ret = ble_core_init();
    if (ret != 0) {
        LOG_ERR("Failed to initialize BLE core: %d", ret);
        return ret;
    }

    LOG_INF("BLE Core Initialized!");

    ret = messaging_service_init();
    if (ret != 0) {
        LOG_ERR("Failed to initialize messaging BLE service: %d", ret);
        return ret;
    }

    LOG_INF("BLE Messaging Service initialized!");

    ret = messaging_endpoint_init();
    if (ret != 0) {
        LOG_INF("Failed to initialize messaging endpoint: %d", ret);
        return ret;
    }
    LOG_INF("Messaging endpoint initialized!");

    return 0;
}
