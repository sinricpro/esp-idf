/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef SINRICPRO_DEVICE_INTERNAL_H
#define SINRICPRO_DEVICE_INTERNAL_H

#include "sinricpro_types.h"
#include "esp_err.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Device request handler callback
 *
 * Called when a request is received for a specific device.
 *
 * @param[in]     device_id        Device ID
 * @param[in]     action           Action name
 * @param[in]     instance_id      Instance ID (optional, can be NULL)
 * @param[in]     request_value    Request value JSON object
 * @param[in,out] response_value   Response value JSON object (to be filled)
 * @param[in]     user_data        User data
 *
 * @return true if request handled successfully, false otherwise
 */
typedef bool (*sinricpro_device_request_handler_t)(
    const char *device_id,
    const char *action,
    const char *instance_id,
    cJSON *request_value,
    cJSON *response_value,
    void *user_data
);

/**
 * @brief Internal device structure
 */
typedef struct sinricpro_device {
    char device_id[CONFIG_SINRICPRO_MAX_DEVICE_ID_LEN];
    sinricpro_device_type_t device_type;
    sinricpro_device_request_handler_t request_handler;
    void *user_data;
    struct sinricpro_device *next;  /* Linked list */
} sinricpro_device_t;

/**
 * @brief Register a device (internal API)
 *
 * @param[in] device Device structure
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sinricpro_core_register_device(sinricpro_device_t *device);

/**
 * @brief Unregister a device (internal API)
 *
 * @param[in] device_id Device ID
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sinricpro_core_unregister_device(const char *device_id);

/**
 * @brief Send an event message (internal API)
 *
 * @param[in] device_id Device ID
 * @param[in] action    Action name
 * @param[in] cause     Cause string
 * @param[in] value     Value JSON object
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sinricpro_core_send_event(const char *device_id,
                                     const char *action,
                                     const char *cause,
                                     cJSON *value);

#ifdef __cplusplus
}
#endif

#endif /* SINRICPRO_DEVICE_INTERNAL_H */
