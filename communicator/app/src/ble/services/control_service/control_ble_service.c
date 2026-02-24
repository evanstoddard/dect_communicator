/*
 * Copyright (C) Ovyl
 */

/**
 * @file control_ble_service.c
 * @author Evan Stoddard
 * @brief
 */

#include "control_ble_service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/conn.h>

#include "utils/device_id.h"

/*****************************************************************************
 * Definitions
 *****************************************************************************/

LOG_MODULE_REGISTER(control_ble_service);

#define CONTROL_BLE_SERVICE_UUID_VAL BT_UUID_128_ENCODE(0x7928884e, 0x01e6, 0x4137, 0x86d3, 0xadefd8afe21d)
#define CONTROL_BLE_SERVICE_DEVICE_ID_CHAR_UUID_VAL                                                                    \
    BT_UUID_128_ENCODE(0x7928884f, 0x01e6, 0x4137, 0x86d3, 0xadefd8afe21d)

static struct bt_uuid_128 prv_control_ble_service_uuid = BT_UUID_INIT_128(CONTROL_BLE_SERVICE_UUID_VAL);
static struct bt_uuid_128 prv_control_ble_service_device_id_char_uuid =
    BT_UUID_INIT_128(CONTROL_BLE_SERVICE_DEVICE_ID_CHAR_UUID_VAL);

#define CONTROL_BLE_SERVICE_UUID                ((const struct bt_uuid *)&prv_control_ble_service_uuid)
#define CONTROL_BLE_SERVICE_DEVICE_ID_CHAR_UUID ((const struct bt_uuid *)&prv_control_ble_service_device_id_char_uuid)

/*****************************************************************************
 * Variables
 *****************************************************************************/

/*****************************************************************************
 * BLE Bindings
 *****************************************************************************/

/**
 * @brief Called when data written to data characteristic by connected device
 *
 * @param conn Pointer to connection
 * @param attr Pointer to attribute
 * @param buf Pointer to outbound buffer
 * @param len Length of TX buffer in bytes
 * @param offset Offset of total transmission (currently not used)
 * @return Returns len on success or negative BT_ATT_ERR
 */
static ssize_t prv_on_device_id_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len,
                                     uint16_t offset);

BT_GATT_SERVICE_DEFINE(prv_control_ble_service, BT_GATT_PRIMARY_SERVICE(CONTROL_BLE_SERVICE_UUID),
                       BT_GATT_CHARACTERISTIC(CONTROL_BLE_SERVICE_DEVICE_ID_CHAR_UUID, BT_GATT_CHRC_READ,
                                              BT_GATT_PERM_READ, prv_on_device_id_read, NULL, NULL), );

/*****************************************************************************
 * Private Functions
 *****************************************************************************/

static ssize_t prv_on_device_id_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len,
                                     uint16_t offset)
{
    static bool fetched = false;
    static uint32_t dev_id = 0;

    if (fetched == false) {
        dev_id = device_id();
        fetched = true;
    }

    return bt_gatt_attr_read(conn, attr, buf, len, offset, &dev_id, sizeof(dev_id));
}

/*****************************************************************************
 * Functions
 *****************************************************************************/
