/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef WEBRTC_CAMERA_H
#define WEBRTC_CAMERA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_camera.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief One STUN/TURN server URL with its optional credentials
 */
typedef struct {
    const char *url;         /**< "stun:host:port", "turn:host:port?transport=udp", "turns:host:443?transport=tcp" */
    const char *username;    /**< NULL or empty when the server needs none */
    const char *credential;  /**< NULL or empty when the server needs none */
} webrtc_camera_ice_server_t;

/**
 * @brief Microphone source for the optional PCMU audio track
 *
 * Fills @p size bytes of 8 kHz PCMU (160 = 20 ms) and returns true when a frame
 * is ready. Called on the session task every loop, even without a viewer, so
 * the source can drain its input. Must not block.
 */
typedef bool (*webrtc_camera_audio_source_t)(uint8_t *pcmu, size_t size, void *ctx);

typedef struct {
    uint32_t task_stack_size;
    UBaseType_t task_priority;
    uint32_t answer_timeout_ms;        /**< Leaves headroom inside Alexa's 6 s answer budget */
    uint32_t channel_open_timeout_ms;  /**< Drops a viewer whose DataChannel never opens */
    uint32_t frame_interval_ms;        /**< Initial frame rate; viewers can change it */
    size_t max_frame_bytes;
    uint32_t data_channel_send_cache;
    uint32_t data_channel_recv_cache;
    bool auto_quality;                 /**< Lower frame rate, then JPEG quality, when the link backs up */
    /** Largest viewer-selectable resolution. Must not exceed the size esp_camera_init()
     *  used, since the camera's JPEG buffers are sized for it. FRAMESIZE_INVALID = current size. */
    framesize_t max_frame_size;
    int flash_gpio;                    /**< Flash LED GPIO, -1 for none (AI-Thinker ESP32-CAM: 4) */
    webrtc_camera_audio_source_t audio_source;  /**< NULL = no microphone track */
    void *audio_ctx;

    /** Send H.264 on a WebRTC video track when the viewer offers one. ESP32-S3 only, where
     *  esp_h264 encodes in software. A viewer that offers no video track still gets JPEG over
     *  the DataChannel, so older app and portal versions keep working. */
    bool h264_enabled;
    uint16_t h264_width;
    uint16_t h264_height;
    uint8_t h264_fps;
    uint32_t h264_bitrate;
    /** The board's camera wiring, as passed to esp_camera_init(). Required when h264_enabled:
     *  the session re-initialises the camera in YUV422 for a video track and back to JPEG after. */
    camera_config_t camera_config;
} webrtc_camera_config_t;

#define WEBRTC_CAMERA_CONFIG_DEFAULT() {        \
    .task_stack_size = 24 * 1024,               \
    .task_priority = 4,                         \
    .answer_timeout_ms = 4000,                  \
    .channel_open_timeout_ms = 15000,           \
    .frame_interval_ms = 200,                   \
    .max_frame_bytes = 128 * 1024,              \
    .data_channel_send_cache = 48 * 1024,       \
    .data_channel_recv_cache = 16 * 1024,       \
    .auto_quality = true,                       \
    .max_frame_size = FRAMESIZE_INVALID,        \
    .flash_gpio = -1,                           \
    .audio_source = NULL,                       \
    .audio_ctx = NULL,                          \
    .h264_enabled = false,                      \
    .h264_width = 320,                          \
    .h264_height = 240,                         \
    .h264_fps = 10,                             \
    .h264_bitrate = 400000,                     \
    .camera_config = {0},                       \
}

/**
 * @brief Session handle (opaque)
 *
 * One viewer at a time: a new offer replaces the current viewer. Every esp_peer
 * call runs on the session task; other tasks reach it only through a queue.
 */
typedef struct webrtc_camera *webrtc_camera_handle_t;

/**
 * @brief Start the session task
 *
 * Call after esp_camera_init() and once Wi-Fi is connected. The session lives
 * for the lifetime of the application.
 */
esp_err_t webrtc_camera_start(const webrtc_camera_config_t *config, webrtc_camera_handle_t *out_handle);

/**
 * @brief Answer a viewer's SDP offer
 *
 * Blocks until the local answer, carrying every gathered candidate, is ready or
 * the answer timeout expires. Signaling is a single exchange with no trickle ICE,
 * so the answer must be complete when it is returned.
 *
 * @param[in]  handle            Session
 * @param[in]  offer_sdp         Viewer's SDP offer
 * @param[in]  ice_servers       STUN/TURN servers for this session
 * @param[in]  ice_server_count  Number of servers
 * @param[out] answer_sdp        On success, the heap-allocated answer; the caller frees it
 * @param[out] error             Optional; on failure, a reason to show the viewer
 * @param[in]  error_size        Size of @p error
 *
 * @return ESP_OK, or an error with @p error describing it
 */
esp_err_t webrtc_camera_handle_offer(webrtc_camera_handle_t handle, const char *offer_sdp,
                                     const webrtc_camera_ice_server_t *ice_servers, size_t ice_server_count,
                                     char **answer_sdp, char *error, size_t error_size);

/**
 * @brief Close the current viewer, if any
 */
void webrtc_camera_stop_viewer(webrtc_camera_handle_t handle);

/**
 * @brief Whether a viewer's DataChannel is open
 */
bool webrtc_camera_is_streaming(webrtc_camera_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* WEBRTC_CAMERA_H */
