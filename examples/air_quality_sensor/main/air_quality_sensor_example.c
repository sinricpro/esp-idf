/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_random.h"
#include "nvs_flash.h"

#include "sinricpro.h"
#include "sinricpro_air_quality_sensor.h"

/* WiFi Configuration */
#define WIFI_SSID      "WIFI_SSID"
#define WIFI_PASS      "WIFI_PASS"

/* SinricPro Configuration */
#define DEVICE_ID      "DEVICE_ID"
#define APP_KEY        "APP_KEY"
#define APP_SECRET     "APP_SECRET"

/* Sensor Settings */
#define SENSOR_UPDATE_INTERVAL_MS  (60000)  /* Send air quality update every 60 seconds */

static const char *TAG = "air_quality_sensor_example";

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static sinricpro_device_handle_t my_sensor = NULL;

/* Current air quality readings */
static int current_pm1 = 0;
static int current_pm2_5 = 0;
static int current_pm10 = 0;

/* WiFi Event Handler */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi disconnected, retrying...");
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                          &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                          &wifi_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to WiFi...");
}

/* Simulated air quality sensor task */
static void air_quality_sensor_task(void *pvParameters)
{
    TickType_t last_update = 0;

    while (1) {
        /* Simulate PM sensor readings (in real application, read from actual sensor like PMS5003) */
        current_pm1 = 10 + (esp_random() % 30);      /* PM1.0: 10-40 ug/m3 */
        current_pm2_5 = 20 + (esp_random() % 50);    /* PM2.5: 20-70 ug/m3 */
        current_pm10 = 30 + (esp_random() % 70);     /* PM10: 30-100 ug/m3 */

        ESP_LOGI(TAG, "Air Quality - PM1.0: %d, PM2.5: %d, PM10: %d ug/m3",
                 current_pm1, current_pm2_5, current_pm10);

        /* Determine air quality level */
        const char *quality;
        if (current_pm2_5 <= 12) {
            quality = "Good";
        } else if (current_pm2_5 <= 35) {
            quality = "Moderate";
        } else if (current_pm2_5 <= 55) {
            quality = "Unhealthy for Sensitive Groups";
        } else if (current_pm2_5 <= 150) {
            quality = "Unhealthy";
        } else if (current_pm2_5 <= 250) {
            quality = "Very Unhealthy";
        } else {
            quality = "Hazardous";
        }

        ESP_LOGI(TAG, "Air Quality Level: %s", quality);

        /* Send air quality update every SENSOR_UPDATE_INTERVAL_MS */
        if ((xTaskGetTickCount() - last_update) >= pdMS_TO_TICKS(SENSOR_UPDATE_INTERVAL_MS)) {
            esp_err_t ret = sinricpro_air_quality_sensor_send_air_quality_event(
                my_sensor,
                current_pm1,
                current_pm2_5,
                current_pm10,
                SINRICPRO_CAUSE_PERIODIC_POLL
            );

            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Air quality event sent successfully");
            } else if (ret == SINRICPRO_ERR_RATE_LIMITED) {
                ESP_LOGW(TAG, "Air quality event rate limited");
            } else {
                ESP_LOGE(TAG, "Failed to send air quality event: %s", esp_err_to_name(ret));
            }

            last_update = xTaskGetTickCount();
        }

        vTaskDelay(pdMS_TO_TICKS(5000));  /* Update readings every 5 seconds */
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "SinricPro Air Quality Sensor Example");

    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Initialize WiFi */
    wifi_init_sta();

    /* Wait for WiFi connection */
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "WiFi connected");

    /* Initialize SinricPro */
    sinricpro_config_t config = {
        .app_key = APP_KEY,
        .app_secret = APP_SECRET,
        .auto_reconnect = true,
        .reconnect_interval_ms = 5000,
        .heartbeat_interval_ms = 0
    };
    ESP_ERROR_CHECK(sinricpro_init(&config));

    /* Create Air Quality Sensor device */
    my_sensor = sinricpro_air_quality_sensor_create(DEVICE_ID);
    if (my_sensor == NULL) {
        ESP_LOGE(TAG, "Failed to create Air Quality Sensor device");
        return;
    }

    ESP_LOGI(TAG, "Starting SinricPro...");
    ESP_ERROR_CHECK(sinricpro_start());

    /* Start air quality sensor task */
    xTaskCreate(air_quality_sensor_task, "air_quality_sensor", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Air Quality Sensor ready. Reporting PM values to SinricPro...");
    ESP_LOGI(TAG, "View real-time data in the SinricPro dashboard");
}
