/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "setting_controller.h"
#include <string.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "setting_ctrl";

/**
 * @brief SettingController context
 */
struct sinricpro_setting_controller {
    sinricpro_setting_callback_t callback;
    void *user_data;
};

sinricpro_setting_controller_handle_t sinricpro_setting_controller_create(void)
{
    sinricpro_setting_controller_handle_t handle =
        malloc(sizeof(struct sinricpro_setting_controller));

    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate SettingController");
        return NULL;
    }

    handle->callback = NULL;
    handle->user_data = NULL;

    ESP_LOGD(TAG, "SettingController created");

    return handle;
}

esp_err_t sinricpro_setting_controller_set_callback(
    sinricpro_setting_controller_handle_t handle,
    sinricpro_setting_callback_t callback,
    void *user_data)
{
    if (handle == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->callback = callback;
    handle->user_data = user_data;

    ESP_LOGD(TAG, "Setting callback registered");

    return ESP_OK;
}

bool sinricpro_setting_controller_handle_request(
    sinricpro_setting_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value)
{
    if (handle == NULL || action == NULL) {
        return false;
    }

    if (strcmp(action, "setSetting") != 0) {
        return false;  /* Not our action */
    }

    if (handle->callback == NULL) {
        ESP_LOGW(TAG, "No Setting callback registered");
        return false;
    }

    /* Extract setting and value from request */
    cJSON *setting_item = cJSON_GetObjectItem(request_value, "setting");
    cJSON *value_item = cJSON_GetObjectItem(request_value, "value");

    if (setting_item == NULL || !cJSON_IsString(setting_item) ||
        value_item == NULL || !cJSON_IsString(value_item)) {
        ESP_LOGE(TAG, "Invalid setting or value in request");
        return false;
    }

    const char *setting_id = setting_item->valuestring;
    const char *value = value_item->valuestring;

    ESP_LOGI(TAG, "Setting request: device=%s, setting=%s, value=%s",
             device_id, setting_id, value);

    /* Call user callback */
    bool success = handle->callback(device_id, setting_id, value, handle->user_data);

    /* Echo back in response */
    cJSON_AddStringToObject(response_value, "setting", setting_id);
    cJSON_AddStringToObject(response_value, "value", value);

    return success;
}

void sinricpro_setting_controller_destroy(sinricpro_setting_controller_handle_t handle)
{
    if (handle != NULL) {
        free(handle);
        ESP_LOGD(TAG, "SettingController destroyed");
    }
}
