/*
 * Copyright (C) Evan Stoddard
 */

/**
 * @file ble_core.h
 * @author Evan Stoddard
 * @brief
 */

#ifndef ble_core_h
#define ble_core_h

#include <zephyr/bluetooth/uuid.h>

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * Definitions
 *****************************************************************************/

/*****************************************************************************
 * Structs, Unions, Enums, & Typedefs
 *****************************************************************************/

/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/

/**
 * @brief Initialize BLE core and enable Bluetooth stack
 *
 * @note Does not start advertising. Call ble_core_start_advertising() after
 *       registering any service UUIDs.
 *
 * @return 0 on success, negative errno on failure
 */
int ble_core_init(void);

/**
 * @brief Register a 128-bit service UUID to include in the scan response
 *
 * @note Must be called before ble_core_start_advertising().
 *
 * @param uuid Pointer to the 128-bit UUID to advertise
 * @return 0 on success, -ENOMEM if the UUID buffer is full
 */
int ble_core_register_adv_uuid128(const struct bt_uuid_128 *uuid);

/**
 * @brief Start BLE advertising
 *
 * Begins advertising with any UUIDs previously registered via
 * ble_core_register_adv_uuid128() included in the scan response.
 *
 * @return 0 on success, negative errno on failure
 */
int ble_core_start_advertising(void);

#ifdef __cplusplus
}
#endif
#endif /* ble_core_h */
