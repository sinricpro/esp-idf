/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef SINRICPRO_TYPES_H
#define SINRICPRO_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SinricPro error codes
 */
#define SINRICPRO_ERR_BASE              0x50000  /* SinricPro error base */
#define SINRICPRO_ERR_INVALID_CONFIG    (SINRICPRO_ERR_BASE + 1)  /* Invalid configuration */
#define SINRICPRO_ERR_NOT_INITIALIZED   (SINRICPRO_ERR_BASE + 2)  /* Not initialized */
#define SINRICPRO_ERR_ALREADY_STARTED   (SINRICPRO_ERR_BASE + 3)  /* Already started */
#define SINRICPRO_ERR_NOT_STARTED       (SINRICPRO_ERR_BASE + 4)  /* Not started */
#define SINRICPRO_ERR_WEBSOCKET_FAILED  (SINRICPRO_ERR_BASE + 5)  /* WebSocket operation failed */
#define SINRICPRO_ERR_INVALID_DEVICE    (SINRICPRO_ERR_BASE + 6)  /* Invalid device handle */
#define SINRICPRO_ERR_DEVICE_NOT_FOUND  (SINRICPRO_ERR_BASE + 7)  /* Device not found */
#define SINRICPRO_ERR_QUEUE_FULL        (SINRICPRO_ERR_BASE + 8)  /* Queue is full */
#define SINRICPRO_ERR_NO_MEMORY         (SINRICPRO_ERR_BASE + 9)  /* Out of memory */
#define SINRICPRO_ERR_INVALID_ARG       (SINRICPRO_ERR_BASE + 10) /* Invalid argument */
#define SINRICPRO_ERR_TIMEOUT           (SINRICPRO_ERR_BASE + 11) /* Operation timeout */
#define SINRICPRO_ERR_SIGNATURE         (SINRICPRO_ERR_BASE + 12) /* Signature verification failed */
#define SINRICPRO_ERR_NOT_CONNECTED     (SINRICPRO_ERR_BASE + 13) /* Not connected to server */
#define SINRICPRO_ERR_RATE_LIMITED      (SINRICPRO_ERR_BASE + 14) /* Rate limited */

/**
 * @brief Device handle type (opaque pointer)
 */
typedef void* sinricpro_device_handle_t;

/**
 * @brief Device type enumeration
 */
typedef enum {
    SINRICPRO_DEVICE_TYPE_SWITCH = 0,
    SINRICPRO_DEVICE_TYPE_LIGHT,
    SINRICPRO_DEVICE_TYPE_DIMSWITCH,
    SINRICPRO_DEVICE_TYPE_FAN,
    SINRICPRO_DEVICE_TYPE_THERMOSTAT,
    SINRICPRO_DEVICE_TYPE_LOCK,
    SINRICPRO_DEVICE_TYPE_BLINDS,
    SINRICPRO_DEVICE_TYPE_GARAGE_DOOR,
    SINRICPRO_DEVICE_TYPE_DOORBELL,
    SINRICPRO_DEVICE_TYPE_MOTION_SENSOR,
    SINRICPRO_DEVICE_TYPE_CONTACT_SENSOR,
    SINRICPRO_DEVICE_TYPE_TEMPERATURE_SENSOR,
    SINRICPRO_DEVICE_TYPE_AIR_QUALITY_SENSOR,
    SINRICPRO_DEVICE_TYPE_POWER_SENSOR,
    SINRICPRO_DEVICE_TYPE_CAMERA,
    SINRICPRO_DEVICE_TYPE_TV,
    SINRICPRO_DEVICE_TYPE_SPEAKER,
    SINRICPRO_DEVICE_TYPE_WINDOW_AC,
    SINRICPRO_DEVICE_TYPE_MAX
} sinricpro_device_type_t;

/**
 * @brief Cause types for events
 */
#define SINRICPRO_CAUSE_PHYSICAL_INTERACTION "PHYSICAL_INTERACTION"
#define SINRICPRO_CAUSE_PERIODIC_POLL        "PERIODIC_POLL"
#define SINRICPRO_CAUSE_VOICE_INTERACTION    "VOICE_INTERACTION"
#define SINRICPRO_CAUSE_APP_INTERACTION      "APP_INTERACTION"

/**
 * @brief Action types
 */
#define SINRICPRO_ACTION_SET_POWER_STATE     "setPowerState"
#define SINRICPRO_ACTION_SET_BRIGHTNESS      "setBrightness"
#define SINRICPRO_ACTION_SET_COLOR           "setColor"
#define SINRICPRO_ACTION_SET_COLOR_TEMP      "setColorTemperature"
#define SINRICPRO_ACTION_SET_PERCENTAGE      "setPercentage"
#define SINRICPRO_ACTION_SET_RANGE_VALUE     "setRangeValue"
#define SINRICPRO_ACTION_PUSH_NOTIFICATION   "pushNotification"

/**
 * @brief Message payload structure (internal)
 */
typedef struct {
    char *action;
    char *device_id;
    char *instance_id;
    void *value;      /* JSON object */
    void *response;   /* JSON object for response */
} sinricpro_message_t;

#ifdef __cplusplus
}
#endif

#endif /* SINRICPRO_TYPES_H */
