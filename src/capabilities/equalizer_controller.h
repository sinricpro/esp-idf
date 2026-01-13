/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef EQUALIZER_CONTROLLER_H
#define EQUALIZER_CONTROLLER_H

#include "sinricpro_types.h"
#include "cJSON.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Equalizer bands structure
 */
typedef struct {
    int bass;        /**< Bass level (-10 to +10) */
    int midrange;    /**< Midrange level (-10 to +10) */
    int treble;      /**< Treble level (-10 to +10) */
} sinricpro_equalizer_bands_t;

/**
 * @brief Equalizer callback signature
 *
 * @param[in] device_id Device ID
 * @param[in,out] bands Pointer to equalizer bands
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_equalizer_callback_t)(
    const char *device_id,
    sinricpro_equalizer_bands_t *bands,
    void *user_data
);

typedef struct sinricpro_equalizer_controller* sinricpro_equalizer_controller_handle_t;

sinricpro_equalizer_controller_handle_t sinricpro_equalizer_controller_create(void);

esp_err_t sinricpro_equalizer_controller_set_callback(
    sinricpro_equalizer_controller_handle_t handle,
    sinricpro_equalizer_callback_t callback,
    void *user_data);

bool sinricpro_equalizer_controller_handle_request(
    sinricpro_equalizer_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value);

esp_err_t sinricpro_equalizer_controller_send_event(
    sinricpro_equalizer_controller_handle_t handle,
    const char *device_id,
    const sinricpro_equalizer_bands_t *bands,
    const char *cause);

void sinricpro_equalizer_controller_destroy(sinricpro_equalizer_controller_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* EQUALIZER_CONTROLLER_H */
