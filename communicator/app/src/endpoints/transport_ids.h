/*
 * Copyright (C) Ovyl
 */

/**
 * @file transport_ids.h
 * @author Evan Stoddard
 * @brief
 */

#ifndef transport_ids_h
#define transport_ids_h

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * Definitions
 *****************************************************************************/

/*****************************************************************************
 * Structs, Unions, Enums, & Typedefs
 *****************************************************************************/

typedef enum {
    ENDPOINT_TRANSPORT_ID_DECT,
    ENDPOINT_TRANSPORT_ID_BLE,
} endpoint_transport_id_t;

/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/

#ifdef __cplusplus
}
#endif
#endif /* transport_ids_h */
