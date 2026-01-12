/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef SINRICPRO_SPEAKER_H
#define SINRICPRO_SPEAKER_H

#include "sinricpro_types.h"
#include <stdbool.h>
#include <stdint.h>

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
} sinricpro_speaker_equalizer_bands_t;

/* Callback types */
typedef bool (*sinricpro_speaker_power_state_callback_t)(const char *device_id, bool *state, void *user_data);
typedef bool (*sinricpro_speaker_volume_callback_t)(const char *device_id, int *volume, void *user_data);
typedef bool (*sinricpro_speaker_adjust_volume_callback_t)(const char *device_id, int *delta, void *user_data);
typedef bool (*sinricpro_speaker_mute_callback_t)(const char *device_id, bool *mute, void *user_data);
typedef bool (*sinricpro_speaker_media_control_callback_t)(const char *device_id, const char *control, void *user_data);
typedef bool (*sinricpro_speaker_input_callback_t)(const char *device_id, const char **input, void *user_data);
typedef bool (*sinricpro_speaker_equalizer_callback_t)(const char *device_id, sinricpro_speaker_equalizer_bands_t *bands, void *user_data);
typedef bool (*sinricpro_speaker_mode_callback_t)(const char *device_id, const char **mode, void *user_data);

sinricpro_device_handle_t sinricpro_speaker_create(const char *device_id);

esp_err_t sinricpro_speaker_on_power_state(sinricpro_device_handle_t device, sinricpro_speaker_power_state_callback_t callback, void *user_data);
esp_err_t sinricpro_speaker_on_volume(sinricpro_device_handle_t device, sinricpro_speaker_volume_callback_t callback, void *user_data);
esp_err_t sinricpro_speaker_on_adjust_volume(sinricpro_device_handle_t device, sinricpro_speaker_adjust_volume_callback_t callback, void *user_data);
esp_err_t sinricpro_speaker_on_mute(sinricpro_device_handle_t device, sinricpro_speaker_mute_callback_t callback, void *user_data);
esp_err_t sinricpro_speaker_on_media_control(sinricpro_device_handle_t device, sinricpro_speaker_media_control_callback_t callback, void *user_data);
esp_err_t sinricpro_speaker_on_input(sinricpro_device_handle_t device, sinricpro_speaker_input_callback_t callback, void *user_data);
esp_err_t sinricpro_speaker_on_equalizer(sinricpro_device_handle_t device, sinricpro_speaker_equalizer_callback_t callback, void *user_data);
esp_err_t sinricpro_speaker_on_mode(sinricpro_device_handle_t device, sinricpro_speaker_mode_callback_t callback, void *user_data);

esp_err_t sinricpro_speaker_send_power_state_event(sinricpro_device_handle_t device, bool state, const char *cause);
esp_err_t sinricpro_speaker_send_volume_event(sinricpro_device_handle_t device, int volume, const char *cause);
esp_err_t sinricpro_speaker_send_mute_event(sinricpro_device_handle_t device, bool mute, const char *cause);
esp_err_t sinricpro_speaker_send_media_control_event(sinricpro_device_handle_t device, const char *control, const char *cause);
esp_err_t sinricpro_speaker_send_input_event(sinricpro_device_handle_t device, const char *input, const char *cause);
esp_err_t sinricpro_speaker_send_equalizer_event(sinricpro_device_handle_t device, const sinricpro_speaker_equalizer_bands_t *bands, const char *cause);
esp_err_t sinricpro_speaker_send_mode_event(sinricpro_device_handle_t device, const char *mode, const char *cause);

#ifdef __cplusplus
}
#endif

#endif /* SINRICPRO_SPEAKER_H */
