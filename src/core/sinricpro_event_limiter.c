/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "sinricpro_event_limiter.h"
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sinricpro_limiter";

/**
 * @brief Event limiter structure
 */
struct sinricpro_event_limiter {
    uint32_t min_interval_ms;    /**< Minimum interval between events (ms) */
    TickType_t last_event_time;  /**< Tick count of last event */
    bool initialized;            /**< Initialization flag */
};

sinricpro_event_limiter_handle_t sinricpro_event_limiter_create(uint32_t min_interval_ms)
{
    sinricpro_event_limiter_handle_t handle = malloc(sizeof(struct sinricpro_event_limiter));
    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate event limiter");
        return NULL;
    }

    handle->min_interval_ms = min_interval_ms;
    handle->last_event_time = 0;
    handle->initialized = false;

    ESP_LOGD(TAG, "Event limiter created (min_interval=%lu ms)", min_interval_ms);

    return handle;
}

bool sinricpro_event_limiter_check(sinricpro_event_limiter_handle_t handle)
{
    if (handle == NULL) {
        return false;
    }

    TickType_t current_time = xTaskGetTickCount();

    /* First event always allowed */
    if (!handle->initialized) {
        handle->initialized = true;
        handle->last_event_time = current_time;
        ESP_LOGD(TAG, "First event allowed");
        return true;
    }

    /* Calculate time since last event */
    TickType_t ticks_elapsed = current_time - handle->last_event_time;
    uint32_t ms_elapsed = (ticks_elapsed * 1000) / configTICK_RATE_HZ;

    if (ms_elapsed >= handle->min_interval_ms) {
        /* Enough time has passed, allow event */
        handle->last_event_time = current_time;
        ESP_LOGD(TAG, "Event allowed (elapsed=%lu ms)", ms_elapsed);
        return true;
    } else {
        /* Too soon, rate limit */
        ESP_LOGD(TAG, "Event rate limited (elapsed=%lu ms, required=%lu ms)",
                 ms_elapsed, handle->min_interval_ms);
        return false;
    }
}

uint32_t sinricpro_event_limiter_time_until_next(sinricpro_event_limiter_handle_t handle)
{
    if (handle == NULL) {
        return 0;
    }

    if (!handle->initialized) {
        return 0;  /* First event can be sent immediately */
    }

    TickType_t current_time = xTaskGetTickCount();
    TickType_t ticks_elapsed = current_time - handle->last_event_time;
    uint32_t ms_elapsed = (ticks_elapsed * 1000) / configTICK_RATE_HZ;

    if (ms_elapsed >= handle->min_interval_ms) {
        return 0;  /* Can send now */
    } else {
        return handle->min_interval_ms - ms_elapsed;
    }
}

void sinricpro_event_limiter_reset(sinricpro_event_limiter_handle_t handle)
{
    if (handle == NULL) {
        return;
    }

    handle->initialized = false;
    handle->last_event_time = 0;

    ESP_LOGD(TAG, "Event limiter reset");
}

void sinricpro_event_limiter_destroy(sinricpro_event_limiter_handle_t handle)
{
    if (handle != NULL) {
        free(handle);
        ESP_LOGD(TAG, "Event limiter destroyed");
    }
}
