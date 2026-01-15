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
#include "sinricpro_thermostat.h"

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
#define DHT_GPIO            (4)   /* DHT22 sensor (simulated for this example) */

/* Temperature Settings */
#define TEMP_UPDATE_INTERVAL_MS  (60000)  /* Send temp update every 60 seconds */
#define TEMP_HYSTERESIS          (0.5f)   /* Temperature hysteresis in Celsius */

static const char *TAG = "thermostat_example";

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

/* Thermostat state */
static bool thermostat_power_state = false;
static sinricpro_thermostat_mode_t thermostat_mode = SINRICPRO_THERMOSTAT_MODE_OFF;
static float target_temperature = 22.0f;  /* Target temperature in Celsius */
static float current_temperature = 20.0f; /* Current temperature (from sensor) */
static float current_humidity = 45.0f;    /* Current humidity (from sensor) */
static sinricpro_device_handle_t my_thermostat = NULL;

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
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << RELAY_HEAT_GPIO) | (1ULL << RELAY_COOL_GPIO) | (1ULL << RELAY_FAN_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    /* Initialize all relays to OFF */
    gpio_set_level(RELAY_HEAT_GPIO, 0);
    gpio_set_level(RELAY_COOL_GPIO, 0);
    gpio_set_level(RELAY_FAN_GPIO, 0);

    ESP_LOGI(TAG, "GPIO initialized (Heat:%d, Cool:%d, Fan:%d)",
             RELAY_HEAT_GPIO, RELAY_COOL_GPIO, RELAY_FAN_GPIO);
}

/* Convert thermostat mode to string */
static const char* mode_to_string(sinricpro_thermostat_mode_t mode)
{
    switch (mode) {
        case SINRICPRO_THERMOSTAT_MODE_AUTO: return "AUTO";
        case SINRICPRO_THERMOSTAT_MODE_COOL: return "COOL";
        case SINRICPRO_THERMOSTAT_MODE_HEAT: return "HEAT";
        case SINRICPRO_THERMOSTAT_MODE_ECO:  return "ECO";
        case SINRICPRO_THERMOSTAT_MODE_OFF:  return "OFF";
        default: return "UNKNOWN";
    }
}

/* HVAC Control Logic */
static void update_hvac_state(void)
{
    if (!thermostat_power_state || thermostat_mode == SINRICPRO_THERMOSTAT_MODE_OFF) {
        /* Turn off all relays */
        gpio_set_level(RELAY_HEAT_GPIO, 0);
        gpio_set_level(RELAY_COOL_GPIO, 0);
        gpio_set_level(RELAY_FAN_GPIO, 0);
        ESP_LOGI(TAG, "HVAC: OFF");
        return;
    }

    float temp_diff = target_temperature - current_temperature;

    switch (thermostat_mode) {
        case SINRICPRO_THERMOSTAT_MODE_HEAT:
        case SINRICPRO_THERMOSTAT_MODE_AUTO:
            if (temp_diff > TEMP_HYSTERESIS) {
                /* Need heating */
                gpio_set_level(RELAY_HEAT_GPIO, 1);
                gpio_set_level(RELAY_COOL_GPIO, 0);
                gpio_set_level(RELAY_FAN_GPIO, 1);
                ESP_LOGI(TAG, "HVAC: HEATING (%.1f°C -> %.1f°C)",
                         current_temperature, target_temperature);
            } else if (temp_diff < -TEMP_HYSTERESIS &&
                       thermostat_mode == SINRICPRO_THERMOSTAT_MODE_AUTO) {
                /* Need cooling (AUTO mode only) */
                gpio_set_level(RELAY_HEAT_GPIO, 0);
                gpio_set_level(RELAY_COOL_GPIO, 1);
                gpio_set_level(RELAY_FAN_GPIO, 1);
                ESP_LOGI(TAG, "HVAC: COOLING (%.1f°C -> %.1f°C)",
                         current_temperature, target_temperature);
            } else {
                /* Temperature in range */
                gpio_set_level(RELAY_HEAT_GPIO, 0);
                gpio_set_level(RELAY_COOL_GPIO, 0);
                gpio_set_level(RELAY_FAN_GPIO, 0);
                ESP_LOGI(TAG, "HVAC: IDLE (at target %.1f°C)", current_temperature);
            }
            break;

        case SINRICPRO_THERMOSTAT_MODE_COOL:
            if (temp_diff < -TEMP_HYSTERESIS) {
                /* Need cooling */
                gpio_set_level(RELAY_HEAT_GPIO, 0);
                gpio_set_level(RELAY_COOL_GPIO, 1);
                gpio_set_level(RELAY_FAN_GPIO, 1);
                ESP_LOGI(TAG, "HVAC: COOLING (%.1f°C -> %.1f°C)",
                         current_temperature, target_temperature);
            } else {
                gpio_set_level(RELAY_HEAT_GPIO, 0);
                gpio_set_level(RELAY_COOL_GPIO, 0);
                gpio_set_level(RELAY_FAN_GPIO, 0);
                ESP_LOGI(TAG, "HVAC: IDLE (at target %.1f°C)", current_temperature);
            }
            break;

        case SINRICPRO_THERMOSTAT_MODE_ECO:
            /* ECO mode: wider temperature range, less aggressive */
            if (temp_diff > TEMP_HYSTERESIS * 2) {
                gpio_set_level(RELAY_HEAT_GPIO, 1);
                gpio_set_level(RELAY_COOL_GPIO, 0);
                gpio_set_level(RELAY_FAN_GPIO, 1);
                ESP_LOGI(TAG, "HVAC: ECO HEATING");
            } else if (temp_diff < -TEMP_HYSTERESIS * 2) {
                gpio_set_level(RELAY_HEAT_GPIO, 0);
                gpio_set_level(RELAY_COOL_GPIO, 1);
                gpio_set_level(RELAY_FAN_GPIO, 1);
                ESP_LOGI(TAG, "HVAC: ECO COOLING");
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
    thermostat_power_state = *state;
    update_hvac_state();
    return true;
}

static bool on_thermostat_mode(const char *device_id, sinricpro_thermostat_mode_t *mode, void *user_data)
{
    ESP_LOGI(TAG, "Mode: %s", mode_to_string(*mode));
    thermostat_mode = *mode;
    update_hvac_state();
    return true;
}

static bool on_target_temperature(const char *device_id, float *temperature, void *user_data)
{
    ESP_LOGI(TAG, "Target Temperature: %.1f°C", *temperature);
    target_temperature = *temperature;
    update_hvac_state();
    return true;
}

static bool on_adjust_target_temperature(const char *device_id, float *delta, void *user_data)
{
    target_temperature += *delta;

    /* Clamp to reasonable range */
    if (target_temperature < 10.0f) target_temperature = 10.0f;
    if (target_temperature > 35.0f) target_temperature = 35.0f;

    *delta = target_temperature;
    ESP_LOGI(TAG, "Adjust temperature, new target: %.1f°C", target_temperature);
    update_hvac_state();
    return true;
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

/* Simulated DHT22 sensor reading task */
static void temperature_sensor_task(void *arg)
{
    /* In a real application, you would read from actual DHT22 sensor here
     * For this example, we simulate temperature fluctuations */

    float simulated_temp = 20.0f;
    float simulated_humidity = 45.0f;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(TEMP_UPDATE_INTERVAL_MS));

        /* Simulate temperature changes based on HVAC state */
        bool heating = gpio_get_level(RELAY_HEAT_GPIO);
        bool cooling = gpio_get_level(RELAY_COOL_GPIO);

        if (heating) {
            simulated_temp += 0.5f;  /* Temperature rising */
        } else if (cooling) {
            simulated_temp -= 0.5f;  /* Temperature falling */
        } else {
            /* Drift toward room temperature (23°C) */
            if (simulated_temp < 23.0f) {
                simulated_temp += 0.2f;
            } else if (simulated_temp > 23.0f) {
                simulated_temp -= 0.2f;
            }
        }

        /* Add some random variation */
        simulated_temp += ((float)(esp_random() % 20) - 10.0f) / 100.0f;
        simulated_humidity += ((float)(esp_random() % 10) - 5.0f) / 10.0f;

        /* Clamp values */
        if (simulated_humidity < 30.0f) simulated_humidity = 30.0f;
        if (simulated_humidity > 70.0f) simulated_humidity = 70.0f;

        current_temperature = simulated_temp;
        current_humidity = simulated_humidity;

        /* Send temperature event to SinricPro */
        esp_err_t ret = sinricpro_thermostat_send_temperature_event(
            my_thermostat,
            current_temperature,
            current_humidity,
            SINRICPRO_CAUSE_PERIODIC_POLL
        );

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Temperature: %.1f°C, Humidity: %.1f%%",
                     current_temperature, current_humidity);
        } else if (ret == SINRICPRO_ERR_RATE_LIMITED) {
            ESP_LOGW(TAG, "Temperature event rate limited");
        }

        /* Update HVAC state based on new temperature */
        update_hvac_state();
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, " SinricPro Thermostat Example");
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

    ESP_LOGI(TAG, "Creating thermostat device...");
    my_thermostat = sinricpro_thermostat_create(DEVICE_ID);
    if (my_thermostat == NULL) {
        ESP_LOGE(TAG, "Failed to create thermostat");
        return;
    }

    /* Register callbacks */
    sinricpro_thermostat_on_power_state(my_thermostat, on_power_state, NULL);
    sinricpro_thermostat_on_thermostat_mode(my_thermostat, on_thermostat_mode, NULL);
    sinricpro_thermostat_on_target_temperature(my_thermostat, on_target_temperature, NULL);
    sinricpro_thermostat_on_adjust_target_temperature(my_thermostat, on_adjust_target_temperature, NULL);

    ESP_LOGI(TAG, "Starting SinricPro...");
    ESP_ERROR_CHECK(sinricpro_start());

    ESP_LOGI(TAG, "Thermostat ready!");
    ESP_LOGI(TAG, "Try: 'Alexa, turn on the thermostat'");
    ESP_LOGI(TAG, "Try: 'Alexa, set thermostat to 72 degrees'");
    ESP_LOGI(TAG, "Try: 'Alexa, make it warmer'");

    /* Start temperature sensor task */
    xTaskCreate(temperature_sensor_task, "temp_sensor", 4096, NULL, 5, NULL);

    /* Initial state */
    thermostat_power_state = true;
    thermostat_mode = SINRICPRO_THERMOSTAT_MODE_AUTO;
    target_temperature = 22.0f;
    update_hvac_state();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
