/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef SINRICPRO_LIGHT_H
#define SINRICPRO_LIGHT_H

#include "sinricpro_types.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief RGB color structure
 */
typedef struct {
    uint8_t r;  /**< Red component (0-255) */
    uint8_t g;  /**< Green component (0-255) */
    uint8_t b;  /**< Blue component (0-255) */
} sinricpro_light_color_t;

/**
 * @brief Power state callback for light
 *
 * @param[in] device_id Device ID
 * @param[in,out] state Pointer to power state (true=on, false=off)
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_light_power_state_callback_t)(
    const char *device_id,
    bool *state,
    void *user_data
);

/**
 * @brief Brightness callback for light
 *
 * @param[in] device_id Device ID
 * @param[in,out] brightness Pointer to brightness value (0-100)
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_light_brightness_callback_t)(
    const char *device_id,
    int *brightness,
    void *user_data
);

/**
 * @brief Adjust brightness callback for light
 *
 * @param[in] device_id Device ID
 * @param[in,out] delta Pointer to delta value (-100 to +100)
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_light_adjust_brightness_callback_t)(
    const char *device_id,
    int *delta,
    void *user_data
);

/**
 * @brief Color callback for light
 *
 * @param[in] device_id Device ID
 * @param[in,out] color Pointer to RGB color structure
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_light_color_callback_t)(
    const char *device_id,
    sinricpro_light_color_t *color,
    void *user_data
);

/**
 * @brief Color temperature callback for light
 *
 * @param[in] device_id Device ID
 * @param[in,out] color_temperature Pointer to color temperature in Kelvin (2200-7000)
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_light_color_temperature_callback_t)(
    const char *device_id,
    int *color_temperature,
    void *user_data
);

/**
 * @brief Adjust color temperature callback for light
 *
 * @param[in] device_id Device ID
 * @param[in,out] delta Pointer to delta value in Kelvin
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_light_adjust_color_temperature_callback_t)(
    const char *device_id,
    int *delta,
    void *user_data
);

/**
 * @brief Create a new light device
 *
 * @param[in] device_id Device ID from SinricPro portal
 * @return Device handle or NULL on error
 */
sinricpro_device_handle_t sinricpro_light_create(const char *device_id);

/**
 * @brief Register power state callback
 *
 * @param[in] device Device handle
 * @param[in] callback Callback function
 * @param[in] user_data User data passed to callback
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sinricpro_light_on_power_state(
    sinricpro_device_handle_t device,
    sinricpro_light_power_state_callback_t callback,
    void *user_data
);

/**
 * @brief Register brightness callback
 *
 * @param[in] device Device handle
 * @param[in] callback Callback function
 * @param[in] user_data User data passed to callback
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sinricpro_light_on_brightness(
    sinricpro_device_handle_t device,
    sinricpro_light_brightness_callback_t callback,
    void *user_data
);

/**
 * @brief Register adjust brightness callback
 *
 * @param[in] device Device handle
 * @param[in] callback Callback function
 * @param[in] user_data User data passed to callback
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sinricpro_light_on_adjust_brightness(
    sinricpro_device_handle_t device,
    sinricpro_light_adjust_brightness_callback_t callback,
    void *user_data
);

/**
 * @brief Register color callback
 *
 * @param[in] device Device handle
 * @param[in] callback Callback function
 * @param[in] user_data User data passed to callback
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sinricpro_light_on_color(
    sinricpro_device_handle_t device,
    sinricpro_light_color_callback_t callback,
    void *user_data
);

/**
 * @brief Register color temperature callback
 *
 * @param[in] device Device handle
 * @param[in] callback Callback function
 * @param[in] user_data User data passed to callback
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sinricpro_light_on_color_temperature(
    sinricpro_device_handle_t device,
    sinricpro_light_color_temperature_callback_t callback,
    void *user_data
);

/**
 * @brief Register adjust color temperature callback
 *
 * @param[in] device Device handle
 * @param[in] callback Callback function
 * @param[in] user_data User data passed to callback
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sinricpro_light_on_adjust_color_temperature(
    sinricpro_device_handle_t device,
    sinricpro_light_adjust_color_temperature_callback_t callback,
    void *user_data
);

/**
 * @brief Send power state event
 *
 * @param[in] device Device handle
 * @param[in] state Power state (true=on, false=off)
 * @param[in] cause Event cause (PHYSICAL_INTERACTION, etc.)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sinricpro_light_send_power_state_event(
    sinricpro_device_handle_t device,
    bool state,
    const char *cause
);

/**
 * @brief Send brightness event
 *
 * @param[in] device Device handle
 * @param[in] brightness Brightness value (0-100)
 * @param[in] cause Event cause (PHYSICAL_INTERACTION, etc.)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sinricpro_light_send_brightness_event(
    sinricpro_device_handle_t device,
    int brightness,
    const char *cause
);

/**
 * @brief Send color event
 *
 * @param[in] device Device handle
 * @param[in] color RGB color
 * @param[in] cause Event cause (PHYSICAL_INTERACTION, etc.)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sinricpro_light_send_color_event(
    sinricpro_device_handle_t device,
    const sinricpro_light_color_t *color,
    const char *cause
);

/**
 * @brief Send color temperature event
 *
 * @param[in] device Device handle
 * @param[in] color_temperature Color temperature in Kelvin
 * @param[in] cause Event cause (PHYSICAL_INTERACTION, etc.)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sinricpro_light_send_color_temperature_event(
    sinricpro_device_handle_t device,
    int color_temperature,
    const char *cause
);

#ifdef __cplusplus
}
#endif

#endif /* SINRICPRO_LIGHT_H */
