/*
 * Copyright (C) Evan Stoddard
 */

/**
 * @file alfie_messaging_endpoint.h
 * @author Evan Stoddard
 * @brief
 */

#ifndef alfie_messaging_endpoint_h
#define alfie_messaging_endpoint_h

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
 * @brief Initialize Alfie messaging endpoint
 *
 * @return Returns 0 on success or negative errno on error
 */
int alfie_messaging_endpoint_init(void);

#ifdef __cplusplus
}
#endif
#endif /* alfie_messaging_endpoint_h */
