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
#include "sinricpro_blinds.h"

/* WiFi Configuration */
#define WIFI_SSID      "WIFI_SSID"
#define WIFI_PASS      "WIFI_PASS"

/* SinricPro Configuration */
#define DEVICE_ID      "DEVICE_ID"
#define APP_KEY        "APP_KEY"
#define APP_SECRET     "APP_SECRET"

/* Motor Control GPIO (L298N or similar) */
#define MOTOR_IN1_GPIO      (25)  /* Motor direction pin 1 */
#define MOTOR_IN2_GPIO      (26)  /* Motor direction pin 2 */
#define MOTOR_ENABLE_GPIO   (27)  /* Motor enable (can use PWM for speed) */

/* Physical buttons */
#define BUTTON_UP_GPIO      (32)  /* Manual up button */
#define BUTTON_DOWN_GPIO    (33)  /* Manual down button */
#define BUTTON_STOP_GPIO    (0)   /* Stop button (BOOT) */

/* Timing and calibration */
#define FULL_TRAVEL_TIME_MS (10000)  /* Time to go from fully closed to fully open (10s) */
#define POSITION_UPDATE_MS  (100)    /* Update position every 100ms */

static const char *TAG = "blinds";

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

/* Blinds state */
static bool blinds_power_state = false;
static int blinds_position = 0;           /* 0 = closed, 100 = open */
static int target_position = 0;
static bool motor_running = false;
static sinricpro_device_handle_t my_blinds = NULL;

typedef enum {
    MOTOR_STOP,
    MOTOR_UP,
    MOTOR_DOWN
} motor_direction_t;

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
    /* Motor control pins */
    gpio_config_t motor_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << MOTOR_IN1_GPIO) | (1ULL << MOTOR_IN2_GPIO) | (1ULL << MOTOR_ENABLE_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&motor_conf);

    /* Buttons with pull-up */
    gpio_config_t button_conf = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_UP_GPIO) | (1ULL << BUTTON_DOWN_GPIO) | (1ULL << BUTTON_STOP_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 1,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&button_conf);

    /* Initialize motor to stopped state */
    gpio_set_level(MOTOR_IN1_GPIO, 0);
    gpio_set_level(MOTOR_IN2_GPIO, 0);
    gpio_set_level(MOTOR_ENABLE_GPIO, 0);

    ESP_LOGI(TAG, "GPIO initialized");
}

/* Control motor direction */
static void set_motor_direction(motor_direction_t direction)
{
    switch (direction) {
        case MOTOR_UP:
            gpio_set_level(MOTOR_IN1_GPIO, 1);
            gpio_set_level(MOTOR_IN2_GPIO, 0);
            gpio_set_level(MOTOR_ENABLE_GPIO, 1);
            motor_running = true;
            ESP_LOGI(TAG, "Motor: UP");
            break;
        case MOTOR_DOWN:
            gpio_set_level(MOTOR_IN1_GPIO, 0);
            gpio_set_level(MOTOR_IN2_GPIO, 1);
            gpio_set_level(MOTOR_ENABLE_GPIO, 1);
            motor_running = true;
            ESP_LOGI(TAG, "Motor: DOWN");
            break;
        case MOTOR_STOP:
        default:
            gpio_set_level(MOTOR_IN1_GPIO, 0);
            gpio_set_level(MOTOR_IN2_GPIO, 0);
            gpio_set_level(MOTOR_ENABLE_GPIO, 0);
            motor_running = false;
            ESP_LOGI(TAG, "Motor: STOP");
            break;
    }
}

/* SinricPro Callbacks */
static bool on_power_state(const char *device_id, bool *state, void *user_data)
{
    ESP_LOGI(TAG, "Power: %s", *state ? "ON" : "OFF");
    blinds_power_state = *state;

    if (!blinds_power_state) {
        /* Power off - stop motor and reset to closed */
        set_motor_direction(MOTOR_STOP);
        target_position = 0;
    }

    return true;
}

static bool on_range_value(const char *device_id, int *range_value, void *user_data)
{
    ESP_LOGI(TAG, "Position: %d%% (0=closed, 100=open)", *range_value);

    /* Validate range */
    if (*range_value < 0) *range_value = 0;
    if (*range_value > 100) *range_value = 100;

    target_position = *range_value;
    blinds_power_state = true;

    return true;
}

static bool on_adjust_range_value(const char *device_id, int *delta, void *user_data)
{
    target_position += *delta;

    /* Clamp to valid range */
    if (target_position < 0) target_position = 0;
    if (target_position > 100) target_position = 100;

    *delta = target_position;
    ESP_LOGI(TAG, "Adjust position, new target: %d%%", target_position);
    blinds_power_state = true;

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

/* Motor control task - manages position */
static void motor_control_task(void *arg)
{
    int last_reported_position = blinds_position;

    while (1) {
        if (blinds_power_state && blinds_position != target_position) {
            /* Calculate time per percent (ms) */
            int time_per_percent = FULL_TRAVEL_TIME_MS / 100;

            if (blinds_position < target_position) {
                /* Open blinds (move up) */
                set_motor_direction(MOTOR_UP);

                /* Update position based on time */
                vTaskDelay(pdMS_TO_TICKS(time_per_percent));
                blinds_position++;
                if (blinds_position > 100) blinds_position = 100;

            } else if (blinds_position > target_position) {
                /* Close blinds (move down) */
                set_motor_direction(MOTOR_DOWN);

                vTaskDelay(pdMS_TO_TICKS(time_per_percent));
                blinds_position--;
                if (blinds_position < 0) blinds_position = 0;
            }

            /* Report position every 10% change */
            if (abs(blinds_position - last_reported_position) >= 10) {
                sinricpro_blinds_send_range_value_event(
                    my_blinds,
                    blinds_position,
                    SINRICPRO_CAUSE_PHYSICAL_INTERACTION
                );
                last_reported_position = blinds_position;
            }

        } else {
            /* Target reached or powered off */
            set_motor_direction(MOTOR_STOP);

            /* Send final position if different from last report */
            if (blinds_position != last_reported_position) {
                sinricpro_blinds_send_range_value_event(
                    my_blinds,
                    blinds_position,
                    SINRICPRO_CAUSE_PHYSICAL_INTERACTION
                );
                last_reported_position = blinds_position;
            }

            vTaskDelay(pdMS_TO_TICKS(POSITION_UPDATE_MS));
        }
    }
}

/* Manual button task */
static void button_task(void *arg)
{
    while (1) {
        if (gpio_get_level(BUTTON_STOP_GPIO) == 0) {
            /* Stop button pressed */
            target_position = blinds_position;
            set_motor_direction(MOTOR_STOP);
            ESP_LOGI(TAG, "Manual STOP at %d%%", blinds_position);
            vTaskDelay(pdMS_TO_TICKS(300));  /* Debounce */
        } else if (gpio_get_level(BUTTON_UP_GPIO) == 0) {
            /* Up button pressed - fully open */
            target_position = 100;
            ESP_LOGI(TAG, "Manual UP to 100%%");
            vTaskDelay(pdMS_TO_TICKS(300));
        } else if (gpio_get_level(BUTTON_DOWN_GPIO) == 0) {
            /* Down button pressed - fully close */
            target_position = 0;
            ESP_LOGI(TAG, "Manual DOWN to 0%%");
            vTaskDelay(pdMS_TO_TICKS(300));
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, " SinricPro Blinds Example");
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

    ESP_LOGI(TAG, "Creating blinds device...");
    my_blinds = sinricpro_blinds_create(DEVICE_ID);
    if (my_blinds == NULL) {
        ESP_LOGE(TAG, "Failed to create blinds");
        return;
    }

    /* Register callbacks */
    sinricpro_blinds_on_power_state(my_blinds, on_power_state, NULL);
    sinricpro_blinds_on_range_value(my_blinds, on_range_value, NULL);
    sinricpro_blinds_on_adjust_range_value(my_blinds, on_adjust_range_value, NULL);

    ESP_LOGI(TAG, "Starting SinricPro...");
    ESP_ERROR_CHECK(sinricpro_start());

    ESP_LOGI(TAG, "Motorized blinds ready!");
    ESP_LOGI(TAG, "Try: 'Alexa, open the blinds'");
    ESP_LOGI(TAG, "Try: 'Alexa, close the blinds'");
    ESP_LOGI(TAG, "Try: 'Alexa, set blinds to 50 percent'");

    /* Start control tasks */
    xTaskCreate(motor_control_task, "motor_ctrl", 4096, NULL, 5, NULL);
    xTaskCreate(button_task, "button_task", 2048, NULL, 5, NULL);

    /* Initial state: closed */
    blinds_position = 0;
    target_position = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
