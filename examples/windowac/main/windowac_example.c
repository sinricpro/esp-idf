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
#include "driver/gpio.h"

#include "sinricpro.h"
#include "sinricpro_windowac.h"

/* WiFi Configuration */
#define WIFI_SSID      "WIFI_SSID"
#define WIFI_PASS      "WIFI_PASS"

/* SinricPro Configuration */
#define DEVICE_ID      "DEVICE_ID"
#define APP_KEY        "APP_KEY"
#define APP_SECRET     "APP_SECRET"

/* GPIO Configuration */
#define RELAY_HEAT_GPIO     (18)  /* Relay for heating */
#define RELAY_COOL_GPIO     (19)  /* Relay for cooling */
#define RELAY_FAN_GPIO      (21)  /* Relay for fan */

/* Temperature Settings */
#define TEMP_UPDATE_INTERVAL_MS  (60000)  /* Send temp update every 60 seconds */
#define TEMP_HYSTERESIS          (0.5f)   /* Temperature hysteresis in Celsius */

static const char *TAG = "windowac_example";

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

/* Window AC state */
static bool ac_power_state = false;
static sinricpro_windowac_mode_t ac_mode = SINRICPRO_WINDOWAC_MODE_OFF;
static float target_temperature = 22.0f;  /* Target temperature in Celsius */
static int fan_speed = 50;                /* Fan speed 0-100 */
static float current_temperature = 25.0f; /* Current temperature (from sensor) */
static float current_humidity = 45.0f;    /* Current humidity (from sensor) */
static sinricpro_device_handle_t my_windowac = NULL;

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

static const char* mode_to_string(sinricpro_windowac_mode_t mode)
{
    switch (mode) {
        case SINRICPRO_WINDOWAC_MODE_AUTO: return "AUTO";
        case SINRICPRO_WINDOWAC_MODE_COOL: return "COOL";
        case SINRICPRO_WINDOWAC_MODE_HEAT: return "HEAT";
        case SINRICPRO_WINDOWAC_MODE_ECO: return "ECO";
        case SINRICPRO_WINDOWAC_MODE_OFF: return "OFF";
        default: return "UNKNOWN";
    }
}

/* Update AC hardware state based on current settings */
static void update_ac_state(void)
{
    if (!ac_power_state || ac_mode == SINRICPRO_WINDOWAC_MODE_OFF) {
        gpio_set_level(RELAY_HEAT_GPIO, 0);
        gpio_set_level(RELAY_COOL_GPIO, 0);
        gpio_set_level(RELAY_FAN_GPIO, 0);
        ESP_LOGI(TAG, "AC: OFF");
        return;
    }

    float temp_diff = target_temperature - current_temperature;

    switch (ac_mode) {
        case SINRICPRO_WINDOWAC_MODE_COOL:
            if (temp_diff < -TEMP_HYSTERESIS) {
                gpio_set_level(RELAY_HEAT_GPIO, 0);
                gpio_set_level(RELAY_COOL_GPIO, 1);
                gpio_set_level(RELAY_FAN_GPIO, 1);
                ESP_LOGI(TAG, "AC: COOLING (%.1f°C -> %.1f°C, Fan: %d%%)",
                         current_temperature, target_temperature, fan_speed);
            } else {
                gpio_set_level(RELAY_HEAT_GPIO, 0);
                gpio_set_level(RELAY_COOL_GPIO, 0);
                gpio_set_level(RELAY_FAN_GPIO, 0);
                ESP_LOGI(TAG, "AC: IDLE (at target %.1f°C)", current_temperature);
            }
            break;

        case SINRICPRO_WINDOWAC_MODE_HEAT:
            if (temp_diff > TEMP_HYSTERESIS) {
                gpio_set_level(RELAY_HEAT_GPIO, 1);
                gpio_set_level(RELAY_COOL_GPIO, 0);
                gpio_set_level(RELAY_FAN_GPIO, 1);
                ESP_LOGI(TAG, "AC: HEATING (%.1f°C -> %.1f°C, Fan: %d%%)",
                         current_temperature, target_temperature, fan_speed);
            } else {
                gpio_set_level(RELAY_HEAT_GPIO, 0);
                gpio_set_level(RELAY_COOL_GPIO, 0);
                gpio_set_level(RELAY_FAN_GPIO, 0);
                ESP_LOGI(TAG, "AC: IDLE (at target %.1f°C)", current_temperature);
            }
            break;

        case SINRICPRO_WINDOWAC_MODE_AUTO:
            if (temp_diff > TEMP_HYSTERESIS) {
                gpio_set_level(RELAY_HEAT_GPIO, 1);
                gpio_set_level(RELAY_COOL_GPIO, 0);
                gpio_set_level(RELAY_FAN_GPIO, 1);
                ESP_LOGI(TAG, "AC: AUTO HEATING (%.1f°C -> %.1f°C)",
                         current_temperature, target_temperature);
            } else if (temp_diff < -TEMP_HYSTERESIS) {
                gpio_set_level(RELAY_HEAT_GPIO, 0);
                gpio_set_level(RELAY_COOL_GPIO, 1);
                gpio_set_level(RELAY_FAN_GPIO, 1);
                ESP_LOGI(TAG, "AC: AUTO COOLING (%.1f°C -> %.1f°C)",
                         current_temperature, target_temperature);
            } else {
                gpio_set_level(RELAY_HEAT_GPIO, 0);
                gpio_set_level(RELAY_COOL_GPIO, 0);
                gpio_set_level(RELAY_FAN_GPIO, 0);
                ESP_LOGI(TAG, "AC: IDLE (at target %.1f°C)", current_temperature);
            }
            break;

        case SINRICPRO_WINDOWAC_MODE_ECO:
            if (temp_diff > TEMP_HYSTERESIS * 2) {
                gpio_set_level(RELAY_HEAT_GPIO, 1);
                gpio_set_level(RELAY_COOL_GPIO, 0);
                gpio_set_level(RELAY_FAN_GPIO, 1);
                ESP_LOGI(TAG, "AC: ECO HEATING");
            } else if (temp_diff < -TEMP_HYSTERESIS * 2) {
                gpio_set_level(RELAY_HEAT_GPIO, 0);
                gpio_set_level(RELAY_COOL_GPIO, 1);
                gpio_set_level(RELAY_FAN_GPIO, 1);
                ESP_LOGI(TAG, "AC: ECO COOLING");
            } else {
                gpio_set_level(RELAY_HEAT_GPIO, 0);
                gpio_set_level(RELAY_COOL_GPIO, 0);
                gpio_set_level(RELAY_FAN_GPIO, 0);
            }
            break;

        default:
            break;
    }
}

/* SinricPro Callbacks */
static bool on_power_state(const char *device_id, bool *state, void *user_data)
{
    ESP_LOGI(TAG, "Power: %s", *state ? "ON" : "OFF");
    ac_power_state = *state;
    update_ac_state();
    return true;
}

static bool on_thermostat_mode(const char *device_id, sinricpro_windowac_mode_t *mode, void *user_data)
{
    ESP_LOGI(TAG, "Mode: %s", mode_to_string(*mode));
    ac_mode = *mode;
    update_ac_state();
    return true;
}

static bool on_target_temperature(const char *device_id, float *temperature, void *user_data)
{
    ESP_LOGI(TAG, "Target Temperature: %.1f°C", *temperature);
    target_temperature = *temperature;
    update_ac_state();
    return true;
}

static bool on_adjust_target_temperature(const char *device_id, float *delta, void *user_data)
{
    target_temperature += *delta;
    if (target_temperature < 16.0f) target_temperature = 16.0f;
    if (target_temperature > 30.0f) target_temperature = 30.0f;
    *delta = target_temperature;
    ESP_LOGI(TAG, "Adjust target temperature, new: %.1f°C", target_temperature);
    update_ac_state();
    return true;
}

static bool on_range_value(const char *device_id, int *range_value, void *user_data)
{
    ESP_LOGI(TAG, "Fan Speed: %d%%", *range_value);
    fan_speed = *range_value;
    update_ac_state();
    return true;
}

static bool on_adjust_range_value(const char *device_id, int *delta, void *user_data)
{
    fan_speed += *delta;
    if (fan_speed < 0) fan_speed = 0;
    if (fan_speed > 100) fan_speed = 100;
    *delta = fan_speed;
    ESP_LOGI(TAG, "Adjust fan speed, new: %d%%", fan_speed);
    update_ac_state();
    return true;
}

/* Simulated temperature sensor task */
static void temperature_sensor_task(void *pvParameters)
{
    TickType_t last_update = 0;
    float simulated_temp = current_temperature;

    while (1) {
        /* Simulate temperature changes */
        simulated_temp += ((float)(esp_random() % 20) - 10.0f) / 100.0f;

        /* Temperature drift based on AC operation */
        if (ac_power_state && ac_mode != SINRICPRO_WINDOWAC_MODE_OFF) {
            float temp_diff = target_temperature - simulated_temp;
            if (temp_diff > 0 && gpio_get_level(RELAY_HEAT_GPIO)) {
                simulated_temp += 0.1f;  /* Heating effect */
            } else if (temp_diff < 0 && gpio_get_level(RELAY_COOL_GPIO)) {
                simulated_temp -= 0.1f;  /* Cooling effect */
            }
        }

        /* Keep temperature in realistic range */
        if (simulated_temp < 15.0f) simulated_temp = 15.0f;
        if (simulated_temp > 35.0f) simulated_temp = 35.0f;

        current_temperature = simulated_temp;

        /* Simulate humidity */
        current_humidity = 40.0f + ((float)(esp_random() % 20));

        /* Send temperature update every TEMP_UPDATE_INTERVAL_MS */
        if ((xTaskGetTickCount() - last_update) >= pdMS_TO_TICKS(TEMP_UPDATE_INTERVAL_MS)) {
            ESP_LOGI(TAG, "Current: %.1f°C, %.1f%% humidity",
                     current_temperature, current_humidity);

            esp_err_t ret = sinricpro_windowac_send_temperature_event(
                my_windowac,
                current_temperature,
                current_humidity,
                SINRICPRO_CAUSE_PERIODIC_POLL
            );

            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Temperature event sent successfully");
            } else if (ret == SINRICPRO_ERR_RATE_LIMITED) {
                ESP_LOGW(TAG, "Temperature event rate limited");
            } else {
                ESP_LOGE(TAG, "Failed to send temperature event: %s", esp_err_to_name(ret));
            }

            last_update = xTaskGetTickCount();
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "SinricPro Window AC Example");

    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Configure GPIOs */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RELAY_HEAT_GPIO) | (1ULL << RELAY_COOL_GPIO) | (1ULL << RELAY_FAN_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(RELAY_HEAT_GPIO, 0);
    gpio_set_level(RELAY_COOL_GPIO, 0);
    gpio_set_level(RELAY_FAN_GPIO, 0);

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
        .enable_trace = false
    };
    ESP_ERROR_CHECK(sinricpro_init(&config));

    /* Create Window AC device */
    my_windowac = sinricpro_windowac_create(DEVICE_ID);
    if (my_windowac == NULL) {
        ESP_LOGE(TAG, "Failed to create Window AC device");
        return;
    }

    /* Register callbacks */
    sinricpro_windowac_on_power_state(my_windowac, on_power_state, NULL);
    sinricpro_windowac_on_thermostat_mode(my_windowac, on_thermostat_mode, NULL);
    sinricpro_windowac_on_target_temperature(my_windowac, on_target_temperature, NULL);
    sinricpro_windowac_on_adjust_target_temperature(my_windowac, on_adjust_target_temperature, NULL);
    sinricpro_windowac_on_range_value(my_windowac, on_range_value, NULL);
    sinricpro_windowac_on_adjust_range_value(my_windowac, on_adjust_range_value, NULL);

    ESP_LOGI(TAG, "Starting SinricPro...");
    ESP_ERROR_CHECK(sinricpro_start());

    /* Start temperature sensor task */
    xTaskCreate(temperature_sensor_task, "temp_sensor", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Window AC ready. Waiting for commands from Alexa/Google Home...");
}
