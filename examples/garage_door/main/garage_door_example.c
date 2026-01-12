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
#include "sinricpro_garage_door.h"

/* WiFi Configuration */
#define WIFI_SSID      "WIFI_SSID"
#define WIFI_PASS      "WIFI_PASS"

/* SinricPro Configuration */
#define DEVICE_ID      "DEVICE_ID"
#define APP_KEY        "APP_KEY"
#define APP_SECRET     "APP_SECRET"

/* GPIO Configuration */
#define RELAY_GPIO              (18)   /* Relay to trigger garage door opener */
#define REED_SWITCH_OPEN_GPIO   (4)    /* Reed switch for door fully open */
#define REED_SWITCH_CLOSED_GPIO (5)    /* Reed switch for door fully closed */
#define BUTTON_GPIO             (0)    /* Manual trigger button */

/* Timing */
#define RELAY_PULSE_MS          (500)  /* Relay pulse duration (most openers use momentary) */

static const char *TAG = "garage_door";

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

/* Door state */
static bool door_is_closed = false;  /* true = closed, false = open */
static sinricpro_device_handle_t my_garage_door = NULL;

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
    /* Relay output */
    gpio_config_t relay_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << RELAY_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&relay_conf);
    gpio_set_level(RELAY_GPIO, 0);  /* Relay off */

    /* Reed switches with pull-up */
    gpio_config_t reed_conf = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << REED_SWITCH_OPEN_GPIO) | (1ULL << REED_SWITCH_CLOSED_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 1,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&reed_conf);

    /* Button with pull-up */
    gpio_config_t button_conf = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 1,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&button_conf);

    ESP_LOGI(TAG, "GPIO initialized (Relay:%d, Switches:%d,%d, Button:%d)",
             RELAY_GPIO, REED_SWITCH_OPEN_GPIO, REED_SWITCH_CLOSED_GPIO, BUTTON_GPIO);
}

/* Trigger garage door opener (momentary relay pulse) */
static void trigger_garage_door(void)
{
    ESP_LOGI(TAG, "Triggering garage door opener...");
    gpio_set_level(RELAY_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(RELAY_PULSE_MS));
    gpio_set_level(RELAY_GPIO, 0);
    ESP_LOGI(TAG, "Garage door triggered");
}

/* Get current door state from reed switches */
static bool get_door_state(void)
{
    bool open_switch = !gpio_get_level(REED_SWITCH_OPEN_GPIO);    /* LOW = triggered */
    bool closed_switch = !gpio_get_level(REED_SWITCH_CLOSED_GPIO); /* LOW = triggered */

    if (closed_switch) {
        return true;  /* Door is closed */
    } else if (open_switch) {
        return false;  /* Door is open */
    } else {
        /* Door is in motion or partially open */
        return door_is_closed;  /* Return last known state */
    }
}

/* SinricPro Callbacks */
static bool on_door_state(const char *device_id, bool *state, void *user_data)
{
    ESP_LOGI(TAG, "Door command: %s", *state ? "CLOSE" : "OPEN");

    bool current_state = get_door_state();

    /* Only trigger if state needs to change */
    if (current_state != *state) {
        trigger_garage_door();
        /* Note: Actual state will be updated by monitoring task when door reaches position */
    } else {
        ESP_LOGI(TAG, "Door already in requested state");
    }

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

/* Door state monitoring task */
static void door_monitoring_task(void *arg)
{
    bool last_state = get_door_state();
    door_is_closed = last_state;

    ESP_LOGI(TAG, "Door monitoring started, initial state: %s",
             door_is_closed ? "CLOSED" : "OPEN");

    while (1) {
        bool current_state = get_door_state();

        /* Detect state change */
        if (current_state != last_state) {
            door_is_closed = current_state;

            /* Send event to SinricPro */
            esp_err_t ret = sinricpro_garage_door_send_door_state_event(
                my_garage_door,
                door_is_closed,
                SINRICPRO_CAUSE_PHYSICAL_INTERACTION
            );

            if (ret == ESP_OK) {
                if (door_is_closed) {
                    ESP_LOGI(TAG, "🚪 Door CLOSED");
                } else {
                    ESP_LOGI(TAG, "🚪 Door OPENED");
                }
            } else if (ret == SINRICPRO_ERR_RATE_LIMITED) {
                ESP_LOGW(TAG, "Event rate limited");
            }

            last_state = current_state;
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* Manual button task */
static void button_task(void *arg)
{
    bool last_button_state = true;  /* Active low */

    while (1) {
        bool button_state = gpio_get_level(BUTTON_GPIO);

        if (button_state == false && last_button_state == true) {
            ESP_LOGI(TAG, "Manual button pressed");
            trigger_garage_door();
            vTaskDelay(pdMS_TO_TICKS(300));  /* Debounce */
        }

        last_button_state = button_state;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, " SinricPro Garage Door Example");
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

    ESP_LOGI(TAG, "Creating garage door device...");
    my_garage_door = sinricpro_garage_door_create(DEVICE_ID);
    if (my_garage_door == NULL) {
        ESP_LOGE(TAG, "Failed to create garage door");
        return;
    }

    /* Register callback */
    sinricpro_garage_door_on_door_state(my_garage_door, on_door_state, NULL);

    ESP_LOGI(TAG, "Starting SinricPro...");
    ESP_ERROR_CHECK(sinricpro_start());

    ESP_LOGI(TAG, "Garage door controller ready!");
    ESP_LOGI(TAG, "Try: 'Alexa, open the garage door'");
    ESP_LOGI(TAG, "Try: 'Alexa, close the garage door'");
    ESP_LOGI(TAG, "Press BOOT button for manual control");

    /* Start monitoring and control tasks */
    xTaskCreate(door_monitoring_task, "door_monitor", 4096, NULL, 5, NULL);
    xTaskCreate(button_task, "button_task", 2048, NULL, 5, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
