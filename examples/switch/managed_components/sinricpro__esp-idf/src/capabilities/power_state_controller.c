/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "power_state_controller.h"
#include "../core/sinricpro_device_internal.h"
#include "../core/sinricpro_event_limiter.h"
#include <string.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "power_state_ctrl";

/**
 * @brief PowerStateController context
 */
struct sinricpro_power_state_controller {
    sinricpro_power_state_callback_t callback;
    void *user_data;
    sinricpro_event_limiter_handle_t limiter;
};

sinricpro_power_state_controller_handle_t sinricpro_power_state_controller_create(void)
{
    sinricpro_power_state_controller_handle_t handle =
        malloc(sizeof(struct sinricpro_power_state_controller));

    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate PowerStateController");
        return NULL;
    }

    handle->callback = NULL;
    handle->user_data = NULL;
    handle->limiter = sinricpro_event_limiter_create(SINRICPRO_EVENT_LIMIT_STATE);

    if (handle->limiter == NULL) {
        ESP_LOGE(TAG, "Failed to create event limiter");
        free(handle);
        return NULL;
    }

    ESP_LOGD(TAG, "PowerStateController created");

    return handle;
}

esp_err_t sinricpro_power_state_controller_set_callback(
    sinricpro_power_state_controller_handle_t handle,
    sinricpro_power_state_callback_t callback,
    void *user_data)
{
    if (handle == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->callback = callback;
    handle->user_data = user_data;

    ESP_LOGD(TAG, "PowerState callback registered");

    return ESP_OK;
}

bool sinricpro_power_state_controller_handle_request(
    sinricpro_power_state_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value)
{
    if (handle == NULL || action == NULL) {
        return false;
    }

    if (strcmp(action, "setPowerState") != 0) {
        return false;  /* Not our action */
    }

    if (handle->callback == NULL) {
        ESP_LOGW(TAG, "No PowerState callback registered");
        return false;
    }

    /* Extract state from request */
    cJSON *state_item = cJSON_GetObjectItem(request_value, "state");
    if (state_item == NULL || !cJSON_IsString(state_item)) {
        ESP_LOGE(TAG, "Invalid state in request");
        return false;
    }

    bool state = (strcmp(state_item->valuestring, "On") == 0);

    ESP_LOGI(TAG, "PowerState request: device=%s, state=%s", device_id,
             state ? "ON" : "OFF");

    /* Call user callback */
    bool success = handle->callback(device_id, &state, handle->user_data);

    /* Add state to response */
    cJSON_AddStringToObject(response_value, "state", state ? "On" : "Off");

    return success;
}

esp_err_t sinricpro_power_state_controller_send_event(
    sinricpro_power_state_controller_handle_t handle,
    const char *device_id,
    bool state,
    const char *cause)
{
    if (handle == NULL || device_id == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Check rate limit */
    if (!sinricpro_event_limiter_check(handle->limiter)) {
        uint32_t wait_ms = sinricpro_event_limiter_time_until_next(handle->limiter);
        ESP_LOGW(TAG, "PowerState event rate limited (wait %lu ms)", wait_ms);
        return SINRICPRO_ERR_RATE_LIMITED;
    }

    ESP_LOGI(TAG, "Sending PowerState event: device=%s, state=%s, cause=%s",
             device_id, state ? "ON" : "OFF", cause);

    /* Create value object */
    cJSON *value = cJSON_CreateObject();
    cJSON_AddStringToObject(value, "state", state ? "On" : "Off");

    /* Send event (value is consumed by send_event) */
    esp_err_t ret = sinricpro_core_send_event(device_id, "setPowerState", cause, value);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send PowerState event: %s", esp_err_to_name(ret));
    }

    return ret;
}

void sinricpro_power_state_controller_destroy(sinricpro_power_state_controller_handle_t handle)
{
    if (handle == NULL) {
        return;
    }

    if (handle->limiter) {
        sinricpro_event_limiter_destroy(handle->limiter);
    }

    free(handle);

    ESP_LOGD(TAG, "PowerStateController destroyed");
}
