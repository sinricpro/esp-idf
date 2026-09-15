/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef SINRICPRO_CAMERA_H
#define SINRICPRO_CAMERA_H

#include <stdbool.h>
#include <stddef.h>
#include "sinricpro.h"
#include "sinricpro_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief PowerState callback function signature
 *
 * @param[in]     device_id   Device ID string
 * @param[in,out] state       Requested power state in; actual state out
 * @param[in]     user_data   User data pointer passed during registration
 *
 * @return true if request handled successfully, false otherwise
 */
typedef bool (*sinricpro_camera_power_state_callback_t)(
    const char *device_id,
    bool *state,
    void *user_data
);

/**
 * @brief One STUN/TURN server URL with its credentials, as sent by the SinricPro server
 */
typedef struct {
    const char *url;         /**< "stun:…", "turn:…?transport=udp" or "turns:…:443?transport=tcp" */
    const char *username;    /**< Empty string when the server sent none */
    const char *credential;  /**< Empty string when the server sent none */
} sinricpro_ice_server_t;

/**
 * @brief WebRTC offer callback function signature
 *
 * Invoked when a viewer in the SinricPro portal or app requests live view.
 * Signaling is one offer/answer exchange with no trickle ICE, so the answer
 * must already contain every local candidate.
 *
 * @param[in]  device_id         Device ID string
 * @param[in]  offer_sdp         Viewer's SDP offer (plain text, candidates included)
 * @param[in]  ice_servers       STUN/TURN servers for this session, one entry per URL
 * @param[in]  ice_server_count  Number of entries in @p ice_servers
 * @param[out] answer_sdp        Set to a malloc()ed SDP answer; the SDK frees it
 * @param[in]  user_data         User data pointer passed during registration
 *
 * @return true if an answer was produced
 *
 * @note Unlike other callbacks this one may block while ICE gathers, for up to
 *       about 5 s. The strings in @p ice_servers are valid only during the call.
 * @note Call sinricpro_set_response_message() before returning false to show
 *       the viewer why live view failed.
 */
typedef bool (*sinricpro_camera_webrtc_offer_callback_t)(
    const char *device_id,
    const char *offer_sdp,
    const sinricpro_ice_server_t *ice_servers,
    size_t ice_server_count,
    char **answer_sdp,
    void *user_data
);

/**
 * @brief Create a camera device
 *
 * @param[in] device_id Device ID string (exactly 24 hexadecimal characters)
 *
 * @return Device handle, or NULL on failure
 *
 * @note Call after sinricpro_init() and before sinricpro_start()
 */
sinricpro_device_handle_t sinricpro_camera_create(const char *device_id);

/**
 * @brief Register PowerState callback
 *
 * @return ESP_OK, or ESP_ERR_INVALID_ARG for a NULL device or callback
 */
esp_err_t sinricpro_camera_on_power_state(
    sinricpro_device_handle_t device,
    sinricpro_camera_power_state_callback_t callback,
    void *user_data
);

/**
 * @brief Register the WebRTC offer callback
 *
 * Registering it is what makes the camera report WebRTC support through
 * getCameraCapabilities, so viewers only attempt live view once it is set.
 *
 * @return ESP_OK, or ESP_ERR_INVALID_ARG for a NULL device or callback
 */
esp_err_t sinricpro_camera_on_webrtc_offer(
    sinricpro_device_handle_t device,
    sinricpro_camera_webrtc_offer_callback_t callback,
    void *user_data
);

/**
 * @brief Declare that WebRTC sessions carry a microphone audio track
 *
 * Reported through getCameraCapabilities; viewers request an audio track in
 * their offer only when it is set.
 *
 * @return ESP_OK, or ESP_ERR_INVALID_ARG for a NULL device
 */
esp_err_t sinricpro_camera_enable_webrtc_audio(sinricpro_device_handle_t device, bool enabled);

/**
 * @brief Send PowerState event to server
 *
 * @return ESP_OK when queued, or an error (see sinricpro_switch_send_power_state_event())
 */
esp_err_t sinricpro_camera_send_power_state_event(
    sinricpro_device_handle_t device,
    bool state,
    const char *cause
);

/**
 * @brief Delete camera device
 *
 * @return ESP_OK, or ESP_ERR_INVALID_ARG for a NULL device
 */
esp_err_t sinricpro_camera_delete(sinricpro_device_handle_t device);

#ifdef __cplusplus
}
#endif

#endif /* SINRICPRO_CAMERA_H */
