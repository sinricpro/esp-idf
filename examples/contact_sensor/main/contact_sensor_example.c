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
#include "sinricpro_contact_sensor.h"

/* WiFi Configuration */
#define WIFI_SSID      "WIFI_SSID"
#define WIFI_PASS      "WIFI_PASS"

/* SinricPro Configuration */
#define DEVICE_ID      "DEVICE_ID"
#define APP_KEY        "APP_KEY"
#define APP_SECRET     "APP_SECRET"

/* GPIO Configuration */
#define REED_SWITCH_GPIO    (4)    /* Reed switch/magnetic sensor */
#define STATUS_LED_GPIO     (2)    /* Built-in LED */

static const char *TAG = "contact_sensor";

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
    /* Reed switch input with pull-up */
    gpio_config_t reed_conf = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << REED_SWITCH_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 1,  /* Internal pull-up */
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&reed_conf);

    /* Status LED */
    gpio_config_t led_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << STATUS_LED_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_conf);

    gpio_set_level(STATUS_LED_GPIO, 0);

    ESP_LOGI(TAG, "GPIO initialized (Reed Switch:%d, LED:%d)",
             REED_SWITCH_GPIO, STATUS_LED_GPIO);
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

/* Contact detection task */
static void contact_detection_task(void *arg)
{
    bool last_contact_state = false;

    /* Read initial state */
    bool current_state = gpio_get_level(REED_SWITCH_GPIO);
    last_contact_state = current_state;

    ESP_LOGI(TAG, "Contact sensor monitoring started");
    ESP_LOGI(TAG, "Reed switch logic: LOW (0) = Contact detected (magnet close)");
    ESP_LOGI(TAG, "Reed switch logic: HIGH (1) = Contact broken (magnet away)");

    while (1) {
        /* Read reed switch state
         * Reed switches are normally OPEN, close when magnet is near
         * With pull-up: LOW = contact (closed/magnet near), HIGH = no contact (open/magnet away)
         */
        bool contact_detected = !gpio_get_level(REED_SWITCH_GPIO);  /* Invert for logical sense */

        /* Detect state change */
        if (contact_detected != last_contact_state) {
            /* Update LED status */
            gpio_set_level(STATUS_LED_GPIO, contact_detected ? 1 : 0);

            /* Send event to SinricPro */
            esp_err_t ret = sinricpro_contact_sensor_send_contact_event(
                my_sensor,
                contact_detected,
                SINRICPRO_CAUSE_PHYSICAL_INTERACTION
            );

            if (ret == ESP_OK) {
                if (contact_detected) {
                    ESP_LOGI(TAG, "🚪 CLOSED - Contact detected (door/window closed)");
                } else {
                    ESP_LOGW(TAG, "🚪 OPEN - Contact broken (door/window opened)");
                }
            } else if (ret == SINRICPRO_ERR_RATE_LIMITED) {
                ESP_LOGW(TAG, "Event rate limited (max 1 per 60 seconds)");
            } else {
                ESP_LOGE(TAG, "Failed to send contact event: %s", esp_err_to_name(ret));
            }

            last_contact_state = contact_detected;
        }

        vTaskDelay(pdMS_TO_TICKS(100));  /* Check every 100ms */
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, " SinricPro Contact Sensor Example");
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

    ESP_LOGI(TAG, "Creating contact sensor device...");
    my_sensor = sinricpro_contact_sensor_create(DEVICE_ID);
    if (my_sensor == NULL) {
        ESP_LOGE(TAG, "Failed to create contact sensor");
        return;
    }

    ESP_LOGI(TAG, "Starting SinricPro...");
    ESP_ERROR_CHECK(sinricpro_start());

    ESP_LOGI(TAG, "Contact sensor ready!");
    ESP_LOGI(TAG, "Open/close your door or window to test");
    ESP_LOGI(TAG, "LED ON = Contact detected (closed)");
    ESP_LOGI(TAG, "LED OFF = Contact broken (open)");

    /* Start contact detection task */
    xTaskCreate(contact_detection_task, "contact_detect", 4096, NULL, 5, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
