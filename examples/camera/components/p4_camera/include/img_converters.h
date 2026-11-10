/*
 * esp32-camera compatible frame conversion for the ESP32-P4, done by the hardware JPEG encoder.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Compresses a frame from esp_camera_fb_get() to JPEG.
 * @param quality 1-100, higher is better (this argument follows esp32-camera, unlike the 0-63
 *                camera setting)
 * @param out     On success, a heap buffer the caller releases with free()
 */
bool frame2jpg(camera_fb_t *fb, uint8_t quality, uint8_t **out, size_t *out_len);

#ifdef __cplusplus
}
#endif
