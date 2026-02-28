/*
 * Copyright (C) Evan Stoddard
 */

/**
 * @file alfie_router.h
 * @author Evan Stoddard
 * @brief
 */

#ifndef alfie_router_h
#define alfie_router_h

#include "endpoints/alfie_endpoint.h"

#include "alfie_transport.h"

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
 * @brief Initialize Alfie router
 *
 * @return Returns 0 on success or negative errno on failure
 */
int alfie_router_init(void);

/**
 * @brief Register transport with router
 *
 * @param transport Pointer to transport
 * @return Returns 0 on success or negative errno on failure
 */
int alfie_router_register_transport(alfie_transport_t *transport);

/**
 * @brief Register endpoint with router
 *
 * @param endpoint Endpoint to register
 * @return Returns 0 on success or negative errno on failure
 */
int alfie_router_register_endpoint(alfie_endpoint_t *endpoint);

#ifdef __cplusplus
}
#endif
#endif /* alfie_router_h */
