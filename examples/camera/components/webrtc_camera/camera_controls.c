/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "camera_controls.h"
#include <string.h>
#include "cJSON.h"
#include "driver/gpio.h"

typedef struct {
    framesize_t size;
    const char *name;
} resolution_name_t;

static const resolution_name_t RESOLUTIONS[] = {
    {FRAMESIZE_QVGA, "QVGA"}, {FRAMESIZE_CIF, "CIF"}, {FRAMESIZE_VGA, "VGA"},   {FRAMESIZE_SVGA, "SVGA"},
    {FRAMESIZE_XGA, "XGA"},   {FRAMESIZE_HD, "HD"},   {FRAMESIZE_SXGA, "SXGA"}, {FRAMESIZE_UXGA, "UXGA"},
};
#define RESOLUTION_COUNT (sizeof(RESOLUTIONS) / sizeof(RESOLUTIONS[0]))

typedef struct {
    uint16_t interval_percent;
    uint8_t quality_offset;
} quality_level_t;

/* Frame rate drops first because it keeps detail; JPEG compression follows when rate alone is not enough. */
static const quality_level_t LEVELS[] = {{100, 0}, {150, 0}, {200, 6}, {300, 12}, {400, 18}, {600, 24}};
#define LEVEL_COUNT          (sizeof(LEVELS) / sizeof(LEVELS[0]))
#define DEGRADE_AFTER_FRAMES 2
#define RECOVER_AFTER_FRAMES 20
#define MAX_JPEG_QUALITY     63 /* esp32-camera: a higher number compresses harder */

static int clamp_fps(int fps)
{
    if (fps < CAMERA_CONTROLS_MIN_FPS) {
        return CAMERA_CONTROLS_MIN_FPS;
    }
    return fps > CAMERA_CONTROLS_MAX_FPS ? CAMERA_CONTROLS_MAX_FPS : fps;
}

static const char *resolution_name(framesize_t size)
{
    for (size_t i = 0; i < RESOLUTION_COUNT; i++) {
        if (RESOLUTIONS[i].size == size) {
            return RESOLUTIONS[i].name;
        }
    }
    return "";
}

static void set_level(camera_controls_t *controls, uint8_t level)
{
    controls->congested_frames = 0;
    controls->good_frames = 0;
    if (level == controls->level) {
        return;
    }
    controls->level = level;

    sensor_t *sensor = esp_camera_sensor_get();
    if (sensor != NULL) {
        int quality = controls->base_quality + LEVELS[level].quality_offset;
        sensor->set_quality(sensor, quality > MAX_JPEG_QUALITY ? MAX_JPEG_QUALITY : quality);
    }
    controls->state_changed = true;
}

static bool set_resolution(camera_controls_t *controls, const char *name)
{
    if (controls->h264_active) {
        /* Recorded rather than applied: the encoder is built for one size, so the session has to
         * stop it, re-initialise the camera and start it again. */
        for (size_t i = 0; i < controls->h264_resolution_count; i++) {
            if (strcmp(name, controls->h264_resolutions[i]) != 0) {
                continue;
            }
            if (controls->h264_resolution != NULL && strcmp(name, controls->h264_resolution) == 0) {
                return false;
            }
            controls->h264_resolution = controls->h264_resolutions[i];
            controls->h264_resolution_changed = true;
            return true;
        }
        return false;
    }
    sensor_t *sensor = esp_camera_sensor_get();
    for (size_t i = 0; i < RESOLUTION_COUNT; i++) {
        if (strcmp(name, RESOLUTIONS[i].name) != 0 || RESOLUTIONS[i].size > controls->max_frame_size) {
            continue;
        }
        if (RESOLUTIONS[i].size == controls->frame_size || sensor == NULL ||
            sensor->set_framesize(sensor, RESOLUTIONS[i].size) != 0) {
            return false;
        }
        controls->frame_size = RESOLUTIONS[i].size;
        set_level(controls, 0);
        return true;
    }
    return false;
}

static bool set_fps(camera_controls_t *controls, int fps)
{
    fps = clamp_fps(fps);
    if (fps == controls->fps) {
        return false;
    }
    controls->fps = fps;
    set_level(controls, 0);
    return true;
}

static bool set_flash(camera_controls_t *controls, bool on)
{
    if (controls->flash_gpio < 0 || on == controls->flash) {
        return false;
    }
    gpio_set_level((gpio_num_t)controls->flash_gpio, on ? 1 : 0);
    controls->flash = on;
    controls->state_changed = true;
    return true;
}

static bool set_flip(camera_controls_t *controls, bool on)
{
    sensor_t *sensor = esp_camera_sensor_get();
    if (on == controls->flip || sensor == NULL || sensor->set_vflip(sensor, on) != 0) {
        return false;
    }
    controls->flip = on;
    return true;
}

static bool set_mirror(camera_controls_t *controls, bool on)
{
    sensor_t *sensor = esp_camera_sensor_get();
    if (on == controls->mirror || sensor == NULL || sensor->set_hmirror(sensor, on) != 0) {
        return false;
    }
    controls->mirror = on;
    return true;
}

static bool set_auto_quality(camera_controls_t *controls, bool on)
{
    if (on == controls->auto_quality) {
        return false;
    }
    controls->auto_quality = on;
    if (!on) {
        set_level(controls, 0);
    }
    return true;
}

void camera_controls_init(camera_controls_t *controls, framesize_t max_frame_size, int flash_gpio,
                          uint32_t frame_interval_ms, bool auto_quality)
{
    memset(controls, 0, sizeof(*controls));
    controls->frame_size = FRAMESIZE_VGA;
    controls->base_quality = 12;
    controls->auto_quality = auto_quality;
    controls->flash_gpio = flash_gpio;

    sensor_t *sensor = esp_camera_sensor_get();
    if (sensor != NULL) {
        controls->frame_size = sensor->status.framesize;
        controls->base_quality = sensor->status.quality;
        controls->flip = sensor->status.vflip;
        controls->mirror = sensor->status.hmirror;
    }
    controls->max_frame_size = max_frame_size == FRAMESIZE_INVALID ? controls->frame_size : max_frame_size;

    if (flash_gpio >= 0) {
        gpio_reset_pin((gpio_num_t)flash_gpio);
        gpio_set_direction((gpio_num_t)flash_gpio, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)flash_gpio, 0);
    }

    controls->fps = clamp_fps((int)(1000 / (frame_interval_ms > 0 ? frame_interval_ms : 1)));
}

void camera_controls_set_h264(camera_controls_t *controls, bool active,
                              const char *const *resolutions, size_t resolution_count,
                              const char *resolution, int fps)
{
    controls->h264_active = active;
    controls->h264_resolutions = resolutions;
    controls->h264_resolution_count = active ? resolution_count : 0;
    controls->h264_resolution = resolution;
    controls->h264_resolution_changed = false;
    if (active) {
        /* The JPEG quality ladder has nothing to act on while the encoder owns the bitrate. */
        set_level(controls, 0);
        controls->fps = clamp_fps(fps);
    }
    controls->state_changed = true;
}

void camera_controls_reapply(camera_controls_t *controls, bool include_frame_size)
{
    sensor_t *sensor = esp_camera_sensor_get();
    if (sensor == NULL) {
        return;
    }
    if (include_frame_size) {
        sensor->set_framesize(sensor, controls->frame_size);
    }
    int quality = controls->base_quality + LEVELS[controls->level].quality_offset;
    sensor->set_quality(sensor, quality > MAX_JPEG_QUALITY ? MAX_JPEG_QUALITY : quality);
    sensor->set_vflip(sensor, controls->flip);
    sensor->set_hmirror(sensor, controls->mirror);
}

bool camera_controls_apply(camera_controls_t *controls, const char *message, size_t length)
{
    cJSON *json = cJSON_ParseWithLength(message, length);
    if (json == NULL) {
        return false;
    }

    bool changed = false;
    cJSON *type = cJSON_GetObjectItem(json, "type");
    if (cJSON_IsString(type) && strcmp(type->valuestring, "set") == 0) {
        cJSON *item = cJSON_GetObjectItem(json, "resolution");
        if (cJSON_IsString(item)) {
            changed |= set_resolution(controls, item->valuestring);
        }
        item = cJSON_GetObjectItem(json, "fps");
        if (cJSON_IsNumber(item)) {
            changed |= set_fps(controls, item->valueint);
        }
        item = cJSON_GetObjectItem(json, "flash");
        if (cJSON_IsBool(item)) {
            changed |= set_flash(controls, cJSON_IsTrue(item));
        }
        item = cJSON_GetObjectItem(json, "flip");
        if (cJSON_IsBool(item)) {
            changed |= set_flip(controls, cJSON_IsTrue(item));
        }
        item = cJSON_GetObjectItem(json, "mirror");
        if (cJSON_IsBool(item)) {
            changed |= set_mirror(controls, cJSON_IsTrue(item));
        }
        item = cJSON_GetObjectItem(json, "autoQuality");
        if (cJSON_IsBool(item)) {
            changed |= set_auto_quality(controls, cJSON_IsTrue(item));
        }

        /* Report state even when a value was rejected, so the viewer resyncs its controls. */
        controls->state_changed = true;
    }

    cJSON_Delete(json);
    return changed;
}

void camera_controls_on_frame_result(camera_controls_t *controls, bool completed, uint32_t duration_ms)
{
    if (!controls->auto_quality) {
        return;
    }

    uint32_t interval = camera_controls_frame_interval_ms(controls);
    /* A frame that takes longer to send than the frame interval means the link cannot keep up. */
    if (!completed || duration_ms > interval) {
        controls->good_frames = 0;
        if (++controls->congested_frames >= DEGRADE_AFTER_FRAMES && controls->level + 1 < LEVEL_COUNT) {
            set_level(controls, controls->level + 1);
        }
    } else if (duration_ms * 2 < interval) {
        controls->congested_frames = 0;
        if (++controls->good_frames >= RECOVER_AFTER_FRAMES && controls->level > 0) {
            set_level(controls, controls->level - 1);
        }
    } else {
        controls->congested_frames = 0;
        controls->good_frames = 0;
    }
}

void camera_controls_viewer_left(camera_controls_t *controls)
{
    set_flash(controls, false);
    if (controls->level > 0) {
        set_level(controls, 0);
    }
    controls->state_changed = false;
}

uint32_t camera_controls_frame_interval_ms(const camera_controls_t *controls)
{
    return (1000 / (uint32_t)controls->fps) * LEVELS[controls->level].interval_percent / 100;
}

static char *print_and_delete(cJSON *json)
{
    char *text = json != NULL ? cJSON_PrintUnformatted(json) : NULL;
    cJSON_Delete(json);
    return text;
}

char *camera_controls_capabilities_json(const camera_controls_t *controls)
{
    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    cJSON_AddStringToObject(json, "type", "capabilities");
    cJSON *resolutions = cJSON_AddArrayToObject(json, "resolutions");
    if (controls->h264_active) {
        for (size_t i = 0; i < controls->h264_resolution_count && resolutions != NULL; i++) {
            cJSON_AddItemToArray(resolutions, cJSON_CreateString(controls->h264_resolutions[i]));
        }
    } else {
        for (size_t i = 0; i < RESOLUTION_COUNT && resolutions != NULL; i++) {
            if (RESOLUTIONS[i].size <= controls->max_frame_size) {
                cJSON_AddItemToArray(resolutions, cJSON_CreateString(RESOLUTIONS[i].name));
            }
        }
    }
    cJSON_AddStringToObject(json, "videoCodec", controls->h264_active ? "h264" : "jpeg");
    cJSON_AddNumberToObject(json, "minFps", CAMERA_CONTROLS_MIN_FPS);
    cJSON_AddNumberToObject(json, "maxFps", CAMERA_CONTROLS_MAX_FPS);
    cJSON_AddBoolToObject(json, "flash", controls->flash_gpio >= 0);
    cJSON_AddBoolToObject(json, "flip", true);
    cJSON_AddBoolToObject(json, "mirror", true);
    return print_and_delete(json);
}

char *camera_controls_state_json(const camera_controls_t *controls)
{
    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    uint32_t interval = camera_controls_frame_interval_ms(controls);
    /* One decimal, matching what the viewers display. */
    double effective_fps = (double)(uint32_t)(10000.0 / (interval > 0 ? interval : 1) + 0.5) / 10.0;

    cJSON_AddStringToObject(json, "type", "state");
    cJSON_AddStringToObject(json, "videoCodec", controls->h264_active ? "h264" : "jpeg");
    cJSON_AddStringToObject(json, "resolution",
                            controls->h264_active && controls->h264_resolution != NULL
                                ? controls->h264_resolution
                                : resolution_name(controls->frame_size));
    cJSON_AddNumberToObject(json, "fps", controls->fps);
    cJSON_AddBoolToObject(json, "flash", controls->flash);
    cJSON_AddBoolToObject(json, "flip", controls->flip);
    cJSON_AddBoolToObject(json, "mirror", controls->mirror);
    cJSON_AddBoolToObject(json, "autoQuality", controls->auto_quality);
    cJSON_AddNumberToObject(json, "qualityLevel", controls->level);
    cJSON_AddNumberToObject(json, "effectiveFps", effective_fps);
    return print_and_delete(json);
}

const char *camera_controls_take_h264_resolution(camera_controls_t *controls)
{
    if (!controls->h264_resolution_changed) {
        return NULL;
    }
    controls->h264_resolution_changed = false;
    return controls->h264_resolution;
}

bool camera_controls_take_state_changed(camera_controls_t *controls)
{
    bool changed = controls->state_changed;
    controls->state_changed = false;
    return changed;
}
