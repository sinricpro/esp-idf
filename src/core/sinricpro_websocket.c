/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "sinricpro_websocket.h"
#include "sinricpro.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "sinricpro_websocket";

/**
 * @brief WebSocket client state
 */
static struct {
    esp_websocket_client_handle_t client;
    sinricpro_ws_callbacks_t callbacks;
    char *uri;
    bool initialized;
    bool connected;
    SemaphoreHandle_t mutex;
} ws_state = {0};

/**
 * @brief WebSocket event handler
 */
static void websocket_event_handler(void *arg, esp_event_base_t event_base,
                                     int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WebSocket connected");
        xSemaphoreTake(ws_state.mutex, portMAX_DELAY);
        ws_state.connected = true;
        xSemaphoreGive(ws_state.mutex);

        if (ws_state.callbacks.on_connected) {
            ws_state.callbacks.on_connected(ws_state.callbacks.context);
        }
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "WebSocket disconnected");
        xSemaphoreTake(ws_state.mutex, portMAX_DELAY);
        ws_state.connected = false;
        xSemaphoreGive(ws_state.mutex);

        if (ws_state.callbacks.on_disconnected) {
            ws_state.callbacks.on_disconnected(ws_state.callbacks.context);
        }
        break;

    case WEBSOCKET_EVENT_DATA:
        ESP_LOGD(TAG, "WebSocket data received (len=%d)", data->data_len);

        if (data->data_len > 0 && data->data_ptr != NULL) {
            /* Null-terminate the data */
            char *message = malloc(data->data_len + 1);
            if (message) {
                memcpy(message, data->data_ptr, data->data_len);
                message[data->data_len] = '\0';

                if (ws_state.callbacks.on_receive) {
                    ws_state.callbacks.on_receive(message, data->data_len,
                                                   ws_state.callbacks.context);
                }

                free(message);
            } else {
                ESP_LOGE(TAG, "Failed to allocate memory for received message");
            }
        }
        break;

    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "WebSocket error");
        break;

    default:
        ESP_LOGD(TAG, "WebSocket event: %ld", event_id);
        break;
    }
}

esp_err_t sinricpro_ws_init(const char *server_url,
                             uint16_t server_port,
                             const char *app_key,
                             const char *device_ids,
                             const sinricpro_ws_callbacks_t *callbacks)
{
    if (server_url == NULL || app_key == NULL || device_ids == NULL || callbacks == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    if (ws_state.initialized) {
        ESP_LOGW(TAG, "WebSocket already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /* Create mutex */
    ws_state.mutex = xSemaphoreCreateMutex();
    if (ws_state.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    /* Build WebSocket URI (without query parameters) */
    size_t uri_len = snprintf(NULL, 0, "wss://%s:%d/", server_url, server_port) + 1;

    ws_state.uri = malloc(uri_len);
    if (ws_state.uri == NULL) {
        ESP_LOGE(TAG, "Failed to allocate URI buffer");
        vSemaphoreDelete(ws_state.mutex);
        return ESP_ERR_NO_MEM;
    }

    snprintf(ws_state.uri, uri_len, "wss://%s:%d/", server_url, server_port);

    /* Get IP address */
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip_info;
    char ip_str[16] = "0.0.0.0";
    if (netif != NULL && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
    }

    /* Get MAC address */
    uint8_t mac[6];
    char mac_str[18] = "00:00:00:00:00:00";
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK) {
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    /* Build custom headers (HTTP header format with space after colon) */
    char *headers = NULL;
    int headers_len = asprintf(&headers,
        "appkey: %s\r\n"
        "deviceids: %s\r\n"
        "restoredevicestates: false\r\n"
        "ip: %s\r\n"
        "mac: %s\r\n"
        "platform: esp-idf\r\n"
        "SDKVersion: %s\r\n",
        app_key, device_ids, ip_str, mac_str, SINRICPRO_VERSION);

    if (headers_len < 0 || headers == NULL) {
        ESP_LOGE(TAG, "Failed to build headers");
        free(ws_state.uri);
        vSemaphoreDelete(ws_state.mutex);
        return ESP_ERR_NO_MEM;
    }

    /* Save callbacks */
    memcpy(&ws_state.callbacks, callbacks, sizeof(sinricpro_ws_callbacks_t));

    /* Configure WebSocket client */
    esp_websocket_client_config_t ws_config = {
        .uri = ws_state.uri,
        .headers = headers,
        .buffer_size = 2048,
        .task_stack = CONFIG_SINRICPRO_WEBSOCKET_TASK_STACK_SIZE,
        .task_prio = CONFIG_SINRICPRO_WEBSOCKET_TASK_PRIORITY,
        .reconnect_timeout_ms = 10000,
        .network_timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,  /* Use ESP-IDF cert bundle for TLS */
    };

    /* Initialize WebSocket client */
    ws_state.client = esp_websocket_client_init(&ws_config);

    /* Free headers after client init (client makes internal copy) */
    free(headers);

    if (ws_state.client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize WebSocket client");
        free(ws_state.uri);
        vSemaphoreDelete(ws_state.mutex);
        return ESP_FAIL;
    }

    /* Register event handler */
    esp_err_t ret = esp_websocket_register_events(ws_state.client,
                                                    WEBSOCKET_EVENT_ANY,
                                                    websocket_event_handler,
                                                    NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WebSocket event handler: %s",
                 esp_err_to_name(ret));
        esp_websocket_client_destroy(ws_state.client);
        free(ws_state.uri);
        vSemaphoreDelete(ws_state.mutex);
        return ret;
    }

    ws_state.initialized = true;
    ws_state.connected = false;

    ESP_LOGI(TAG, "WebSocket initialized");

    return ESP_OK;
}

esp_err_t sinricpro_ws_start(void)
{
    if (!ws_state.initialized) {
        ESP_LOGE(TAG, "WebSocket not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Starting WebSocket connection...");

    esp_err_t ret = esp_websocket_client_start(ws_state.client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WebSocket client: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

esp_err_t sinricpro_ws_stop(void)
{
    if (!ws_state.initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping WebSocket connection...");

    esp_err_t ret = esp_websocket_client_stop(ws_state.client);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "WebSocket stop returned: %s", esp_err_to_name(ret));
    }

    xSemaphoreTake(ws_state.mutex, portMAX_DELAY);
    ws_state.connected = false;
    xSemaphoreGive(ws_state.mutex);

    return ESP_OK;
}

esp_err_t sinricpro_ws_deinit(void)
{
    if (!ws_state.initialized) {
        return ESP_OK;
    }

    /* Stop first */
    sinricpro_ws_stop();

    /* Destroy client */
    if (ws_state.client) {
        esp_websocket_client_destroy(ws_state.client);
        ws_state.client = NULL;
    }

    /* Free URI */
    if (ws_state.uri) {
        free(ws_state.uri);
        ws_state.uri = NULL;
    }

    /* Delete mutex */
    if (ws_state.mutex) {
        vSemaphoreDelete(ws_state.mutex);
        ws_state.mutex = NULL;
    }

    ws_state.initialized = false;
    ws_state.connected = false;

    ESP_LOGI(TAG, "WebSocket deinitialized");

    return ESP_OK;
}

bool sinricpro_ws_is_connected(void)
{
    if (!ws_state.initialized) {
        return false;
    }

    bool connected;
    xSemaphoreTake(ws_state.mutex, portMAX_DELAY);
    connected = ws_state.connected;
    xSemaphoreGive(ws_state.mutex);

    return connected;
}

esp_err_t sinricpro_ws_send(const char *message, size_t length)
{
    if (!ws_state.initialized) {
        ESP_LOGE(TAG, "WebSocket not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (message == NULL) {
        ESP_LOGE(TAG, "Invalid message");
        return ESP_ERR_INVALID_ARG;
    }

    if (!sinricpro_ws_is_connected()) {
        ESP_LOGW(TAG, "WebSocket not connected, cannot send message");
        return ESP_ERR_INVALID_STATE;
    }

    if (length == 0) {
        length = strlen(message);
    }

    ESP_LOGD(TAG, "Sending WebSocket message (len=%zu)", length);

    int ret = esp_websocket_client_send_text(ws_state.client, message, length,
                                               portMAX_DELAY);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to send WebSocket message: %d", ret);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "WebSocket message sent successfully");

    return ESP_OK;
}
