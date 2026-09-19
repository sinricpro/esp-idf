/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef CAMERA_CONTROLLER_H
#define CAMERA_CONTROLLER_H

#include "sinricpro_types.h"
#include "sinricpro_camera.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CameraController handle (opaque)
 */
typedef struct sinricpro_camera_controller* sinricpro_camera_controller_handle_t;

/**
 * @brief Create CameraController
 *
 * @return Controller handle, or NULL on failure
 */
sinricpro_camera_controller_handle_t sinricpro_camera_controller_create(void);

/**
 * @brief Set WebRTC offer callback
 *
 * @return ESP_OK on success
 */
esp_err_t sinricpro_camera_controller_set_webrtc_offer_callback(
    sinricpro_camera_controller_handle_t handle,
    sinricpro_camera_webrtc_offer_callback_t callback,
    void *user_data);

/**
 * @brief Set the snapshot callback
 *
 * @return ESP_OK on success
 */
esp_err_t sinricpro_camera_controller_set_snapshot_callback(
    sinricpro_camera_controller_handle_t handle,
    sinricpro_camera_snapshot_callback_t callback,
    void *user_data);

/**
 * @brief Set whether WebRTC sessions carry a microphone audio track
 */
void sinricpro_camera_controller_set_webrtc_audio(sinricpro_camera_controller_handle_t handle,
                                                  bool enabled);

/**
 * @brief Set whether WebRTC sessions can carry an H.264 video track
 */
void sinricpro_camera_controller_set_webrtc_video(sinricpro_camera_controller_handle_t handle,
                                                  bool enabled);

/**
 * @brief Whether @p action belongs to this controller
 *
 * Lets a device report a failed camera request as a failure rather than as an
 * action nobody handled.
 */
bool sinricpro_camera_controller_owns_action(const char *action);

/**
 * @brief Handle a camera request
 *
 * @param[in]     handle         Controller handle
 * @param[in]     device_id      Device ID
 * @param[in]     action         Action name
 * @param[in]     request_value  Request value JSON
 * @param[in,out] response_value Response value JSON
 *
 * @return true if handled successfully, false otherwise
 */
bool sinricpro_camera_controller_handle_request(
    sinricpro_camera_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value);

/**
 * @brief Destroy CameraController
 */
void sinricpro_camera_controller_destroy(sinricpro_camera_controller_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* CAMERA_CONTROLLER_H */
