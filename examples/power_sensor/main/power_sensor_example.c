/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
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
#include "sinricpro_power_sensor.h"

/* WiFi Configuration */
#define WIFI_SSID      "WIFI_SSID"
#define WIFI_PASS      "WIFI_PASS"

/* SinricPro Configuration */
#define DEVICE_ID      "DEVICE_ID"
#define APP_KEY        "APP_KEY"
#define APP_SECRET     "APP_SECRET"

/* Sensor Settings */
#define POWER_UPDATE_INTERVAL_MS  (60000)  /* Send power update every 60 seconds */
#define NOMINAL_VOLTAGE           (230.0f) /* Nominal voltage in Volts (230V for EU, 120V for US) */

static const char *TAG = "power_sensor_example";

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static sinricpro_device_handle_t my_sensor = NULL;

/* Current power readings */
static float current_voltage = NOMINAL_VOLTAGE;
static float current_current = 0.0f;
static float current_power = 0.0f;
static float current_apparent_power = 0.0f;
static float current_reactive_power = 0.0f;
static float current_power_factor = 1.0f;

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

/* Simulated power sensor task */
static void power_sensor_task(void *pvParameters)
{
    TickType_t last_update = 0;

    /* Simulate a variable load (e.g., appliance with varying power consumption) */
    float base_load = 500.0f;  /* Base load in Watts */

    while (1) {
        /* Simulate voltage fluctuation (typically ±10V) */
        current_voltage = NOMINAL_VOLTAGE + ((float)(esp_random() % 20) - 10.0f);

        /* Simulate varying load (300W to 1500W) */
        float load_variation = ((float)(esp_random() % 100)) / 100.0f;  /* 0.0 to 1.0 */
        current_power = base_load + (load_variation * 1000.0f);

        /* Calculate current from power and voltage: P = V * I */
        current_current = current_power / current_voltage;

        /* Simulate power factor (typically 0.7 to 1.0 for household appliances) */
        current_power_factor = 0.7f + ((float)(esp_random() % 30)) / 100.0f;

        /* Calculate apparent power: S = P / PF */
        current_apparent_power = current_power / current_power_factor;

        /* Calculate reactive power: Q = sqrt(S^2 - P^2) */
        float s_squared = current_apparent_power * current_apparent_power;
        float p_squared = current_power * current_power;
        current_reactive_power = sqrtf(s_squared - p_squared);

        ESP_LOGI(TAG, "Power Readings:");
        ESP_LOGI(TAG, "  Voltage: %.1fV", current_voltage);
        ESP_LOGI(TAG, "  Current: %.2fA", current_current);
        ESP_LOGI(TAG, "  Power: %.1fW", current_power);
        ESP_LOGI(TAG, "  Apparent Power: %.1fVA", current_apparent_power);
        ESP_LOGI(TAG, "  Reactive Power: %.1fVAR", current_reactive_power);
        ESP_LOGI(TAG, "  Power Factor: %.2f", current_power_factor);

        /* Send power update every POWER_UPDATE_INTERVAL_MS */
        if ((xTaskGetTickCount() - last_update) >= pdMS_TO_TICKS(POWER_UPDATE_INTERVAL_MS)) {
            esp_err_t ret = sinricpro_power_sensor_send_power_sensor_event(
                my_sensor,
                current_voltage,
                current_current,
                current_power,
                current_apparent_power,
                current_reactive_power,
                current_power_factor,
                SINRICPRO_CAUSE_PERIODIC_POLL
            );

            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Power sensor event sent successfully");
            } else if (ret == SINRICPRO_ERR_RATE_LIMITED) {
                ESP_LOGW(TAG, "Power sensor event rate limited");
            } else {
                ESP_LOGE(TAG, "Failed to send power sensor event: %s", esp_err_to_name(ret));
            }

            last_update = xTaskGetTickCount();
        }

        vTaskDelay(pdMS_TO_TICKS(5000));  /* Update readings every 5 seconds */
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "SinricPro Power Sensor Example");

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

    /* Create Power Sensor device */
    my_sensor = sinricpro_power_sensor_create(DEVICE_ID);
    if (my_sensor == NULL) {
        ESP_LOGE(TAG, "Failed to create Power Sensor device");
        return;
    }

    ESP_LOGI(TAG, "Starting SinricPro...");
    ESP_ERROR_CHECK(sinricpro_start());

    /* Start power sensor task */
    xTaskCreate(power_sensor_task, "power_sensor", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Power Sensor ready. Monitoring energy consumption...");
    ESP_LOGI(TAG, "View real-time data in the SinricPro dashboard");
}
