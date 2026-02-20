/*
 * Copyright (C) Ovyl
 */

/**
 * @file messaging_service.c
 * @author Evan Stoddard
 * @brief
 */

#include "messaging_service.h"

#include <stddef.h>
#include <stdint.h>

#include <zephyr/logging/log.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/conn.h>

/*****************************************************************************
 * Definitions
 *****************************************************************************/

LOG_MODULE_REGISTER(messaging_service);

#define MESSAGING_SERVICE_UUID_VAL           BT_UUID_128_ENCODE(0xc1534fa3, 0x5211, 0x4e32, 0xa176, 0xd1af04513305)
#define MESSAGING_SERVICE_DATA_CHAR_UUID_VAL BT_UUID_128_ENCODE(0xc1534fa4, 0x5211, 0x4e32, 0xa176, 0xd1af04513305)

static struct bt_uuid_128 messaging_service_uuid = BT_UUID_INIT_128(MESSAGING_SERVICE_UUID_VAL);
static struct bt_uuid_128 messaging_service_data_char_uuid = BT_UUID_INIT_128(MESSAGING_SERVICE_DATA_CHAR_UUID_VAL);

#define MESSAGING_SERVICE_UUID           ((const struct bt_uuid *)&messaging_service_uuid)
#define MESSAGING_SERVICE_DATA_CHAR_UUID ((const struct bt_uuid *)&messaging_service_data_char_uuid)

/*****************************************************************************
 * Variables
 *****************************************************************************/

static struct {
    struct bt_conn *conn;
} prv_inst;

/*****************************************************************************
 * BLE Bindings
 *****************************************************************************/

/**
 * @brief [TODO:description]
 *
 * @param conn [TODO:parameter]
 * @param attr [TODO:parameter]
 * @param buf [TODO:parameter]
 * @param len [TODO:parameter]
 * @param offset [TODO:parameter]
 * @param flags [TODO:parameter]
 * @return [TODO:return]
 */
static ssize_t prv_on_data_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len,
                                 uint16_t offset, uint8_t flags);

/**
 * @brief [TODO:description]
 *
 * @param attr [TODO:parameter]
 * @param value [TODO:parameter]
 */
static void prv_on_data_char_config_changed(const struct bt_gatt_attr *attr, uint16_t value);

/**
 * @brief [TODO:description]
 *
 * @param conn [TODO:parameter]
 * @param err [TODO:parameter]
 */
static void prv_device_connected(struct bt_conn *conn, uint8_t err);

/**
 * @brief Callback called when device disconnected
 *
 * @param conn Pointer to connection
 * @param reason Reason for disconnection
 */
static void prv_device_disconnected(struct bt_conn *conn, uint8_t reason);

BT_GATT_SERVICE_DEFINE(prv_message_service, BT_GATT_PRIMARY_SERVICE(MESSAGING_SERVICE_UUID),
                       BT_GATT_CHARACTERISTIC(MESSAGING_SERVICE_DATA_CHAR_UUID,
                                              BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_WRITE, NULL,
                                              prv_on_data_write, NULL),
                       BT_GATT_CCC(prv_on_data_char_config_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE));

BT_CONN_CB_DEFINE(prv_conn_callbacks) = {
    .connected = prv_device_connected,
    .disconnected = prv_device_disconnected,
};

/*****************************************************************************
 * Private Functions
 *****************************************************************************/

static ssize_t prv_on_data_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len,
                                 uint16_t offset, uint8_t flags)
{
    LOG_HEXDUMP_INF(buf, len, "Received data from phone:");
    return len;
}

static void prv_on_data_char_config_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    LOG_INF("Data characteristic changed.");
}

static void prv_device_connected(struct bt_conn *conn, uint8_t err)
{
    if (err != 0) {
        return;
    }

    prv_inst.conn = bt_conn_ref(conn);
}

static void prv_device_disconnected(struct bt_conn *conn, uint8_t reason)
{
    bt_conn_unref(conn);
    prv_inst.conn = NULL;
}

/*****************************************************************************
 * Functions
 *****************************************************************************/
