/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef CAMERA_CONTROLS_H
#define CAMERA_CONTROLS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_camera.h"

/* Camera settings a viewer can change over the DataChannel, plus automatic quality adaptation.
 * Control messages are UTF-8 JSON text; binary messages carry JPEG fragments.
 *   device -> viewer  {"type":"capabilities","resolutions":["QVGA","VGA","SVGA"],"minFps":1,"maxFps":15,
 *                      "flash":true,"flip":true,"mirror":true,"videoCodec":"jpeg"}
 *   device -> viewer  {"type":"state","resolution":"VGA","fps":5,"flash":false,"flip":false,"mirror":false,
 *                      "autoQuality":true,"qualityLevel":0,"effectiveFps":5.0,"videoCodec":"jpeg"}
 *   viewer -> device  {"type":"set", ...any of resolution, fps, flash, flip, mirror, autoQuality}
 * Not thread-safe: use it from the task that owns the peer. */

#define CAMERA_CONTROLS_MIN_FPS 1
#define CAMERA_CONTROLS_MAX_FPS 15

typedef struct {
    framesize_t frame_size;
    framesize_t max_frame_size;
    /* An H.264 session runs at whichever of h264_resolutions the encoder was created for. A
     * change is accepted but cannot be applied here: the session has to rebuild the encoder,
     * which it learns about through camera_controls_take_h264_resolution(). */
    bool h264_active;
    const char *const *h264_resolutions;
    size_t h264_resolution_count;
    const char *h264_resolution;
    bool h264_resolution_changed;
    int flash_gpio;
    int fps;
    int base_quality;
    bool flash;
    bool flip;
    bool mirror;
    bool auto_quality;
    uint8_t level;
    uint8_t congested_frames;
    uint8_t good_frames;
    bool state_changed;
} camera_controls_t;

/** Call after esp_camera_init(): the current sensor settings become the starting state. */
void camera_controls_init(camera_controls_t *controls, framesize_t max_frame_size, int flash_gpio,
                          uint32_t frame_interval_ms, bool auto_quality);

/** Switches the reported video path between the H.264 track and DataChannel JPEG.
 *  @p resolutions are the sizes the encoder can be built for, @p resolution the current one. */
void camera_controls_set_h264(camera_controls_t *controls, bool active,
                              const char *const *resolutions, size_t resolution_count,
                              const char *resolution, int fps);

/** The resolution a viewer asked for while H.264 is streaming, or NULL if unchanged. Returned
 *  once: the session rebuilds the encoder for it. */
const char *camera_controls_take_h264_resolution(camera_controls_t *controls);

/** Re-applies the remembered sensor settings after the camera has been re-initialised. */
void camera_controls_reapply(camera_controls_t *controls, bool include_frame_size);

/** Applies a "set" message; returns true if anything changed. */
bool camera_controls_apply(camera_controls_t *controls, const char *message, size_t length);

/** Feeds automatic quality with the outcome of each streamed frame. */
void camera_controls_on_frame_result(camera_controls_t *controls, bool completed, uint32_t duration_ms);

/** Restores full quality and turns the flash off when the viewer disconnects. */
void camera_controls_viewer_left(camera_controls_t *controls);

uint32_t camera_controls_frame_interval_ms(const camera_controls_t *controls);

/** Heap-allocated JSON, or NULL when out of memory. The caller frees it. */
char *camera_controls_capabilities_json(const camera_controls_t *controls);
char *camera_controls_state_json(const camera_controls_t *controls);

/** True once after any state change the viewer has not been told about. */
bool camera_controls_take_state_changed(camera_controls_t *controls);

#endif /* CAMERA_CONTROLS_H */
