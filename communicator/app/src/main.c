/*
 * Copyright (C) Evan Stoddard
 */

/**
 * @file main.c
 * @brief
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "dect/dect_link_layer.h"
#include "dect/dect_transport_layer.h"

#include "ble/ble_core.h"
#include "ble/services/alfie_service/alfie_ble_service.h"

#include "alfie/alfie_router.h"
#include "alfie/endpoints/messaging/alfie_messaging_endpoint.h"

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

    int ret = dect_link_layer_init();
    if (ret != 0) {
        LOG_ERR("Failed to initialize DECT link layer: %d", ret);
        return ret;
    }

    ret = dect_transport_layer_init();
    if (ret != 0) {
        LOG_ERR("Failed to initialize DECT transport layer: %d", ret);
        return ret;
    }

    ret = ble_core_init();
    if (ret != 0) {
        LOG_ERR("Failed to initialize BLE core: %d", ret);
        return ret;
    }

    ret = alfie_ble_service_init();
    if (ret != 0) {
        LOG_ERR("Failed to initialize Alfie BLE service: %d", ret);
        return ret;
    }

    ret = alfie_router_init();
    if (ret != 0) {
        LOG_ERR("Failed to initialize Alfie router: %d", ret);
        return ret;
    }

    ret = alfie_router_register_transport(dect_transport_layer_get_transport());
    if (ret != 0) {
        LOG_ERR("Failed to register DECT transport with router: %d", ret);
        return ret;
    }

    ret = alfie_router_register_transport(alfie_ble_service_get_transport());
    if (ret != 0) {
        LOG_ERR("Failed to register BLE transport with router: %d", ret);
        return ret;
    }

    ret = alfie_messaging_endpoint_init();
    if (ret != 0) {
        LOG_ERR("Failed to initialize messaging endpoint: %d", ret);
        return ret;
    }

    return 0;
}
