/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf-sinricpro)
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
#include "sinricpro_switch.h"

/* WiFi Configuration - MODIFY THESE */
#define WIFI_SSID      "WIFI_SSID"
#define WIFI_PASS      "WIFI_PASS"

/* SinricPro Configuration - MODIFY THESE */
#define DEVICE_ID      "DEVICE_ID"      /* 24-character hex string */
#define APP_KEY        "APP_KEY"        /* From SinricPro portal */
#define APP_SECRET     "APP_SECRET"     /* From SinricPro portal */


/* GPIO Configuration */
#define LED_GPIO       GPIO_NUM_2  /* Built-in LED on most ESP32 boards */
#define BUTTON_GPIO    GPIO_NUM_0  /* Built-in BOOT button */

static const char *TAG = "switch_example";

/* WiFi event group */
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

/* Device state */
static bool device_power_state = false;
static sinricpro_device_handle_t my_switch = NULL;

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

    /* Wait for connection */
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
    /* Configure LED GPIO */
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_conf);

    /* Configure BUTTON GPIO */
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
    ESP_LOGI(TAG, "LED %s", state ? "ON" : "OFF");
}

/* ===========================================================================
 * SinricPro Callbacks
 * =========================================================================== */

/* PowerState callback - called when Alexa/Google Home changes the power state */
static bool on_power_state(const char *device_id, bool *state, void *user_data)
{
    ESP_LOGI(TAG, "PowerState callback: device=%s, state=%s",
             device_id, *state ? "ON" : "OFF");

    /* Update device state */
    device_power_state = *state;

    /* Update hardware (LED) */
    set_led_state(device_power_state);

    /* Return true to indicate success */
    return true;
}

/* Setting callback - called when device settings are changed */
static bool on_setting(const char *device_id, const char *setting_id,
                        const char *value, void *user_data)
{
    ESP_LOGI(TAG, "Setting callback: device=%s, setting=%s, value=%s",
             device_id, setting_id, value);

    /* Handle setting changes here */
    return true;
}

/* SinricPro event handler */
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
 * Button Task (Simulates physical interaction)
 * =========================================================================== */

static void button_task(void *arg)
{
    bool last_button_state = true;  /* Pull-up, so high when not pressed */

    ESP_LOGI(TAG, "Button task started. Press BOOT button to toggle switch.");

    while (1) {
        bool button_state = gpio_get_level(BUTTON_GPIO);

        /* Detect button press (high->low transition) */
        if (last_button_state && !button_state) {
            ESP_LOGI(TAG, "Button pressed!");

            /* Toggle state */
            device_power_state = !device_power_state;
            set_led_state(device_power_state);

            /* Send event to SinricPro server */
            esp_err_t ret = sinricpro_switch_send_power_state_event(
                my_switch,
                device_power_state,
                SINRICPRO_CAUSE_PHYSICAL_INTERACTION
            );

            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "PowerState event sent successfully");
            } else if (ret == SINRICPRO_ERR_RATE_LIMITED) {
                ESP_LOGW(TAG, "Event rate limited (try again in 1 second)");
            } else {
                ESP_LOGE(TAG, "Failed to send event: %s", esp_err_to_name(ret));
            }

            /* Optional: Send notification */
            // sinricpro_switch_send_notification(my_switch, "Button pressed!");

            /* Debounce */
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        last_button_state = button_state;
        vTaskDelay(pdMS_TO_TICKS(50));  /* Poll every 50ms */
    }
}

/* ===========================================================================
 * Main Application
 * =========================================================================== */

void app_main(void)
{
    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, " SinricPro ESP-IDF Switch Example");
    ESP_LOGI(TAG, " Version: %s", sinricpro_get_version());
    ESP_LOGI(TAG, "==============================================");

    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Initialize GPIO */
    gpio_init();

    /* Initialize WiFi */
    ESP_LOGI(TAG, "Connecting to WiFi...");
    wifi_init_sta();

    /* Register SinricPro event handler */
    ESP_ERROR_CHECK(esp_event_handler_register(SINRICPRO_EVENT,
                                                 ESP_EVENT_ANY_ID,
                                                 &sinricpro_event_handler,
                                                 NULL));

    /* Configure SinricPro */
    sinricpro_config_t sinric_config = {
        .app_key = APP_KEY,
        .app_secret = APP_SECRET,
        .auto_reconnect = true,
        .reconnect_interval_ms = 5000,
        .heartbeat_interval_ms = 0,  /* Use default */
    };

    /* Initialize SinricPro */
    ESP_LOGI(TAG, "Initializing SinricPro...");
    ret = sinricpro_init(&sinric_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SinricPro: %s", esp_err_to_name(ret));
        return;
    }

    /* Create switch device */
    ESP_LOGI(TAG, "Creating switch device...");
    my_switch = sinricpro_switch_create(DEVICE_ID);
    if (my_switch == NULL) {
        ESP_LOGE(TAG, "Failed to create switch device");
        return;
    }

    /* Register callbacks */
    sinricpro_switch_on_power_state(my_switch, on_power_state, NULL);
    sinricpro_switch_on_setting(my_switch, on_setting, NULL);

    /* Start SinricPro */
    ESP_LOGI(TAG, "Starting SinricPro...");
    ret = sinricpro_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start SinricPro: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "SinricPro started successfully!");
    ESP_LOGI(TAG, "You can now control the device via Alexa or Google Home");
    ESP_LOGI(TAG, "Say: 'Alexa, turn on the switch'");

    /* Create button task */
    xTaskCreate(button_task, "button_task", 4096, NULL, 5, NULL);

    /* Main loop */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
