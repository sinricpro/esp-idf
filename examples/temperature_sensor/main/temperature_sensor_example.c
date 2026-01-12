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
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "sinricpro.h"
#include "sinricpro_temperature_sensor.h"

/* WiFi Configuration */
#define WIFI_SSID      "WIFI_SSID"
#define WIFI_PASS      "WIFI_PASS"

/* SinricPro Configuration */
#define DEVICE_ID      "DEVICE_ID"
#define APP_KEY        "APP_KEY"
#define APP_SECRET     "APP_SECRET"

/* GPIO Configuration */
#define DHT_GPIO                (4)    /* DHT22 data pin */

/* Sensor Configuration */
#define SENSOR_UPDATE_INTERVAL_MS  (60000)  /* Send update every 60 seconds */

/* Simulated sensor - replace with real DHT22 library */
#define USE_SIMULATED_SENSOR   1

static const char *TAG = "temp_sensor";

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static sinricpro_device_handle_t my_sensor = NULL;

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

    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    ESP_LOGI(TAG, "Connected to WiFi");
}

/* GPIO Initialization */
static void gpio_init(void)
{
#if !USE_SIMULATED_SENSOR
    /* DHT22 uses 1-wire protocol, initialize as needed */
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << DHT_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 1,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    ESP_LOGI(TAG, "GPIO initialized (DHT:%d)", DHT_GPIO);
#else
    ESP_LOGI(TAG, "Using simulated sensor (replace with real DHT22)");
#endif
}

static void sinricpro_event_handler(void *arg, esp_event_base_t event_base,
                                     int32_t event_id, void *event_data)
{
    if (event_base == SINRICPRO_EVENT) {
        switch (event_id) {
            case SINRICPRO_EVENT_CONNECTED:
                ESP_LOGI(TAG, "✓ Connected to SinricPro");
                break;
            case SINRICPRO_EVENT_DISCONNECTED:
                ESP_LOGW(TAG, "✗ Disconnected from SinricPro");
                break;
            case SINRICPRO_EVENT_ERROR:
                ESP_LOGE(TAG, "SinricPro error");
                break;
        }
    }
}

#if USE_SIMULATED_SENSOR
/* Simulated DHT22 sensor - generates realistic values */
static void read_dht22_simulated(float *temperature, float *humidity)
{
    static float sim_temp = 22.0f;
    static float sim_hum = 50.0f;

    /* Add random variation */
    sim_temp += ((float)(esp_random() % 20) - 10.0f) / 10.0f;  /* ±1°C */
    sim_hum += ((float)(esp_random() % 10) - 5.0f) / 5.0f;     /* ±1% */

    /* Clamp to realistic ranges */
    if (sim_temp < 15.0f) sim_temp = 15.0f;
    if (sim_temp > 30.0f) sim_temp = 30.0f;
    if (sim_hum < 30.0f) sim_hum = 30.0f;
    if (sim_hum > 70.0f) sim_hum = 70.0f;

    *temperature = sim_temp;
    *humidity = sim_hum;
}
#else
/* Real DHT22 sensor reading */
/*
 * To use real DHT22:
 * 1. Uncomment esp-dht dependency in idf_component.yml
 * 2. Include: #include "dht.h"
 * 3. Replace this function with actual DHT read
 */
static void read_dht22_real(float *temperature, float *humidity)
{
    /* Example with esp-dht library:
     *
     * esp_err_t ret = dht_read_float_data(DHT_TYPE_DHT22, DHT_GPIO,
     *                                      humidity, temperature);
     * if (ret != ESP_OK) {
     *     ESP_LOGE(TAG, "Failed to read DHT22: %s", esp_err_to_name(ret));
     *     *temperature = 0.0f;
     *     *humidity = 0.0f;
     * }
     */

    ESP_LOGE(TAG, "Real DHT22 not implemented - enable simulation or add DHT library");
    *temperature = 0.0f;
    *humidity = 0.0f;
}
#endif

/* Temperature monitoring task */
static void temperature_monitoring_task(void *arg)
{
    float temperature, humidity;

    ESP_LOGI(TAG, "Temperature monitoring started");
    ESP_LOGI(TAG, "Reporting interval: %d seconds", SENSOR_UPDATE_INTERVAL_MS / 1000);

    while (1) {
        /* Read sensor */
#if USE_SIMULATED_SENSOR
        read_dht22_simulated(&temperature, &humidity);
#else
        read_dht22_real(&temperature, &humidity);
#endif

        /* Send event to SinricPro */
        esp_err_t ret = sinricpro_temperature_sensor_send_temperature_event(
            my_sensor,
            temperature,
            humidity,
            SINRICPRO_CAUSE_PERIODIC_POLL
        );

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "📊 Temperature: %.1f°C, Humidity: %.1f%%",
                     temperature, humidity);
        } else if (ret == SINRICPRO_ERR_RATE_LIMITED) {
            ESP_LOGW(TAG, "Event rate limited (max 1 per 60 seconds)");
        } else {
            ESP_LOGE(TAG, "Failed to send temperature event: %s", esp_err_to_name(ret));
        }

        /* Comfort level indicator */
        if (temperature < 18.0f) {
            ESP_LOGI(TAG, "🥶 Comfort: TOO COLD");
        } else if (temperature > 26.0f) {
            ESP_LOGI(TAG, "🥵 Comfort: TOO HOT");
        } else if (humidity < 30.0f) {
            ESP_LOGI(TAG, "😐 Comfort: TOO DRY");
        } else if (humidity > 60.0f) {
            ESP_LOGI(TAG, "😐 Comfort: TOO HUMID");
        } else {
            ESP_LOGI(TAG, "😊 Comfort: OPTIMAL");
        }

        vTaskDelay(pdMS_TO_TICKS(SENSOR_UPDATE_INTERVAL_MS));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, " SinricPro Temperature Sensor Example");
    ESP_LOGI(TAG, " Version: %s", sinricpro_get_version());
    ESP_LOGI(TAG, "==============================================");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    gpio_init();
    wifi_init_sta();

    ESP_ERROR_CHECK(esp_event_handler_register(SINRICPRO_EVENT, ESP_EVENT_ANY_ID,
                                                 &sinricpro_event_handler, NULL));

    sinricpro_config_t config = {
        .app_key = APP_KEY,
        .app_secret = APP_SECRET,
        .auto_reconnect = true,
    };

    ESP_LOGI(TAG, "Initializing SinricPro...");
    ESP_ERROR_CHECK(sinricpro_init(&config));

    ESP_LOGI(TAG, "Creating temperature sensor device...");
    my_sensor = sinricpro_temperature_sensor_create(DEVICE_ID);
    if (my_sensor == NULL) {
        ESP_LOGE(TAG, "Failed to create temperature sensor");
        return;
    }

    ESP_LOGI(TAG, "Starting SinricPro...");
    ESP_ERROR_CHECK(sinricpro_start());

    ESP_LOGI(TAG, "Temperature sensor ready!");
    ESP_LOGI(TAG, "Monitor temperature and humidity in SinricPro app");

    /* Start temperature monitoring task */
    xTaskCreate(temperature_monitoring_task, "temp_monitor", 4096, NULL, 5, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
