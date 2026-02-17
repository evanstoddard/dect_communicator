/*
 * Copyright (C) Ovyl
 */

/**
 * @file main.c
 * @brief
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "dect/dect_data_layer.h"

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

    return 0;
}
