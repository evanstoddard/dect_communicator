/*
 * Copyright (C) Ovyl
 */

/**
 * @file messaging_service.h
 * @author Evan Stoddard
 * @brief
 */

#ifndef messaging_service_h
#define messaging_service_h

#include "messaging_service_protocol.h"

#include <zephyr/net_buf.h>

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * Definitions
 *****************************************************************************/

/*****************************************************************************
 * Structs, Unions, Enums, & Typedefs
 *****************************************************************************/

/**
 * @typedef messaging_service_on_payload_frame_callback_t
 * @brief [TODO:description]
 *
 */
typedef struct messaging_service_on_payload_frame_callback_t {
    void (*callback)(struct net_buf *net_buf, void *user_ctx);
    void *user_ctx;
} messaging_service_on_payload_frame_callback_t;

/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/

/**
 * @brief [TODO:description]
 *
 * @return [TODO:return]
 */
int messaging_service_init(void);

/**
 * @brief [TODO:description]
 *
 * @param callback [TODO:parameter]
 * @return [TODO:return]
 */
int messaging_service_register_on_payload_frame(messaging_service_on_payload_frame_callback_t *callback);

#ifdef __cplusplus
}
#endif
#endif /* messaging_service_h */
