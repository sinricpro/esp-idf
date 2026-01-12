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
#include "sinricpro_tv.h"

/* WiFi Configuration - MODIFY THESE */
#define WIFI_SSID      "WIFI_SSID"
#define WIFI_PASS      "WIFI_PASS"

/* SinricPro Configuration - MODIFY THESE */
#define DEVICE_ID      "DEVICE_ID"      /* 24-character hex string */
#define APP_KEY        "APP_KEY"        /* From SinricPro portal */
#define APP_SECRET     "APP_SECRET"     /* From SinricPro portal */

/* GPIO Configuration */
#define LED_GPIO       GPIO_NUM_2  /* Built-in LED */
#define BUTTON_GPIO    GPIO_NUM_0  /* Built-in BOOT button */

static const char *TAG = "tv_example";

/* WiFi event group */
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

/* TV state */
static bool tv_power_state = false;
static int tv_volume = 50;
static bool tv_muted = false;
static char tv_input[32] = "HDMI1";
static sinricpro_tv_channel_t tv_channel = {1, "Channel 1"};
static sinricpro_device_handle_t my_tv = NULL;

/* ===========================================================================
 * WiFi Event Handler
 * =========================================================================== */

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
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                          ESP_EVENT_ANY_ID,
                                                          &wifi_event_handler,
                                                          NULL,
                                                          &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                          IP_EVENT_STA_GOT_IP,
                                                          &wifi_event_handler,
                                                          NULL,
                                                          &instance_got_ip));

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

    ESP_LOGI(TAG, "WiFi initialization finished.");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                             WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                             pdFALSE,
                                             pdFALSE,
                                             portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi SSID:%s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s", WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

/* ===========================================================================
 * GPIO Functions
 * =========================================================================== */

static void gpio_init(void)
{
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_conf);

    gpio_config_t button_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&button_conf);

    ESP_LOGI(TAG, "GPIO initialized (LED: GPIO%d, BUTTON: GPIO%d)", LED_GPIO, BUTTON_GPIO);
}

static void set_led_state(bool state)
{
    gpio_set_level(LED_GPIO, state ? 1 : 0);
    ESP_LOGI(TAG, "TV LED %s", state ? "ON" : "OFF");
}

/* ===========================================================================
 * SinricPro Callbacks
 * =========================================================================== */

static bool on_power_state(const char *device_id, bool *state, void *user_data)
{
    ESP_LOGI(TAG, "PowerState: %s", *state ? "ON" : "OFF");
    tv_power_state = *state;
    set_led_state(tv_power_state);
    return true;
}

static bool on_volume(const char *device_id, int *volume, void *user_data)
{
    ESP_LOGI(TAG, "Volume: %d", *volume);
    tv_volume = *volume;
    return true;
}

static bool on_adjust_volume(const char *device_id, int *delta, void *user_data)
{
    tv_volume += *delta;
    if (tv_volume < 0) tv_volume = 0;
    if (tv_volume > 100) tv_volume = 100;
    *delta = tv_volume; /* Return new volume level */
    ESP_LOGI(TAG, "Adjust volume by %d, new volume: %d", *delta, tv_volume);
    return true;
}

static bool on_mute(const char *device_id, bool *mute, void *user_data)
{
    ESP_LOGI(TAG, "Mute: %s", *mute ? "ON" : "OFF");
    tv_muted = *mute;
    return true;
}

static bool on_media_control(const char *device_id, const char *control, void *user_data)
{
    ESP_LOGI(TAG, "Media control: %s", control);
    /* Handle media control: Play, Pause, Stop, Next, Previous, etc. */
    return true;
}

static bool on_input(const char *device_id, const char **input, void *user_data)
{
    ESP_LOGI(TAG, "Input changed to: %s", *input);
    strncpy(tv_input, *input, sizeof(tv_input) - 1);
    return true;
}

static bool on_channel(const char *device_id, sinricpro_tv_channel_t *channel, void *user_data)
{
    ESP_LOGI(TAG, "Channel: %d - %s", channel->number,
             channel->name ? channel->name : "Unknown");
    tv_channel.number = channel->number;
    if (channel->name) {
        tv_channel.name = channel->name;
    }
    return true;
}

static bool on_skip_channels(const char *device_id, int count, void *user_data)
{
    tv_channel.number += count;
    if (tv_channel.number < 1) tv_channel.number = 1;
    if (tv_channel.number > 999) tv_channel.number = 999;

    ESP_LOGI(TAG, "Skip %d channels, new channel: %d", count, tv_channel.number);
    return true;
}

static bool on_setting(const char *device_id, const char *setting_id,
                        const char *value, void *user_data)
{
    ESP_LOGI(TAG, "Setting: %s = %s", setting_id, value);
    return true;
}

static void sinricpro_event_handler(void *arg, esp_event_base_t event_base,
                                     int32_t event_id, void *event_data)
{
    if (event_base == SINRICPRO_EVENT) {
        switch (event_id) {
            case SINRICPRO_EVENT_CONNECTED:
                ESP_LOGI(TAG, "✓ Connected to SinricPro server");
                break;
            case SINRICPRO_EVENT_DISCONNECTED:
                ESP_LOGW(TAG, "✗ Disconnected from SinricPro server");
                break;
            case SINRICPRO_EVENT_ERROR:
                ESP_LOGE(TAG, "SinricPro error occurred");
                break;
        }
    }
}

/* ===========================================================================
 * Button Task
 * =========================================================================== */

static void button_task(void *arg)
{
    bool last_button_state = true;
    ESP_LOGI(TAG, "Button task started. Press BOOT button to toggle power.");

    while (1) {
        bool button_state = gpio_get_level(BUTTON_GPIO);

        if (last_button_state && !button_state) {
            ESP_LOGI(TAG, "Button pressed!");

            tv_power_state = !tv_power_state;
            set_led_state(tv_power_state);

            esp_err_t ret = sinricpro_tv_send_power_state_event(
                my_tv,
                tv_power_state,
                SINRICPRO_CAUSE_PHYSICAL_INTERACTION
            );

            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "PowerState event sent successfully");
            } else if (ret == SINRICPRO_ERR_RATE_LIMITED) {
                ESP_LOGW(TAG, "Event rate limited (try again in 1 second)");
            } else {
                ESP_LOGE(TAG, "Failed to send event: %s", esp_err_to_name(ret));
            }

            vTaskDelay(pdMS_TO_TICKS(500));
        }

        last_button_state = button_state;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* ===========================================================================
 * Main Application
 * =========================================================================== */

void app_main(void)
{
    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, " SinricPro ESP-IDF TV Example");
    ESP_LOGI(TAG, " Version: %s", sinricpro_get_version());
    ESP_LOGI(TAG, "==============================================");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    gpio_init();

    ESP_LOGI(TAG, "Connecting to WiFi...");
    wifi_init_sta();

    ESP_ERROR_CHECK(esp_event_handler_register(SINRICPRO_EVENT,
                                                 ESP_EVENT_ANY_ID,
                                                 &sinricpro_event_handler,
                                                 NULL));

    sinricpro_config_t sinric_config = {
        .app_key = APP_KEY,
        .app_secret = APP_SECRET,
        .auto_reconnect = true,
        .reconnect_interval_ms = 5000,
        .heartbeat_interval_ms = 0,
    };

    ESP_LOGI(TAG, "Initializing SinricPro...");
    ret = sinricpro_init(&sinric_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SinricPro: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "Creating TV device...");
    my_tv = sinricpro_tv_create(DEVICE_ID);
    if (my_tv == NULL) {
        ESP_LOGE(TAG, "Failed to create TV device");
        return;
    }

    /* Register callbacks */
    sinricpro_tv_on_power_state(my_tv, on_power_state, NULL);
    sinricpro_tv_on_volume(my_tv, on_volume, NULL);
    sinricpro_tv_on_adjust_volume(my_tv, on_adjust_volume, NULL);
    sinricpro_tv_on_mute(my_tv, on_mute, NULL);
    sinricpro_tv_on_media_control(my_tv, on_media_control, NULL);
    sinricpro_tv_on_input(my_tv, on_input, NULL);
    sinricpro_tv_on_channel(my_tv, on_channel, NULL);
    sinricpro_tv_on_skip_channels(my_tv, on_skip_channels, NULL);

    ESP_LOGI(TAG, "Starting SinricPro...");
    ret = sinricpro_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start SinricPro: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "SinricPro started successfully!");
    ESP_LOGI(TAG, "You can now control the TV via Alexa or Google Home");
    ESP_LOGI(TAG, "Say: 'Alexa, turn on the TV'");
    ESP_LOGI(TAG, "Say: 'Alexa, set TV volume to 50'");
    ESP_LOGI(TAG, "Say: 'Alexa, mute the TV'");
    ESP_LOGI(TAG, "Say: 'Alexa, change TV to HDMI 2'");

    xTaskCreate(button_task, "button_task", 4096, NULL, 5, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
