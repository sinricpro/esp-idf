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
#include "driver/ledc.h"

#include "sinricpro.h"
#include "sinricpro_lock.h"

/* WiFi Configuration */
#define WIFI_SSID      "WIFI_SSID"
#define WIFI_PASS      "WIFI_PASS"

/* SinricPro Configuration */
#define DEVICE_ID      "DEVICE_ID"
#define APP_KEY        "APP_KEY"
#define APP_SECRET     "APP_SECRET"

/* Servo Configuration */
#define SERVO_GPIO              (18)
#define SERVO_TIMER             LEDC_TIMER_0
#define SERVO_CHANNEL           LEDC_CHANNEL_0
#define SERVO_MODE              LEDC_LOW_SPEED_MODE
#define SERVO_DUTY_RES          LEDC_TIMER_14_BIT  /* 14-bit resolution */
#define SERVO_FREQUENCY         (50)               /* 50 Hz for servo */

/* Servo angles (adjust these for your servo) */
#define SERVO_LOCKED_ANGLE      (0)    /* 0° = locked */
#define SERVO_UNLOCKED_ANGLE    (90)   /* 90° = unlocked */

/* Status LED */
#define STATUS_LED_GPIO         (2)    /* Built-in LED */

/* Physical button for manual lock/unlock */
#define BUTTON_GPIO             (0)    /* BOOT button */

static const char *TAG = "lock_example";

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

/* Lock state */
static bool lock_state = true;  /* true = locked, false = unlocked */
static sinricpro_device_handle_t my_lock = NULL;

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

/* Servo Initialization */
static void servo_init(void)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = SERVO_MODE,
        .timer_num        = SERVO_TIMER,
        .duty_resolution  = SERVO_DUTY_RES,
        .freq_hz          = SERVO_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .channel    = SERVO_CHANNEL,
        .duty       = 0,
        .gpio_num   = SERVO_GPIO,
        .speed_mode = SERVO_MODE,
        .hpoint     = 0,
        .timer_sel  = SERVO_TIMER
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ESP_LOGI(TAG, "Servo initialized on GPIO %d", SERVO_GPIO);
}

/* Set servo angle (0-180 degrees) */
static void servo_set_angle(uint8_t angle)
{
    if (angle > 180) angle = 180;

    /* Calculate duty cycle
     * For 50Hz (20ms period):
     * 0° = 0.5ms pulse (2.5% duty)
     * 90° = 1.5ms pulse (7.5% duty)
     * 180° = 2.5ms pulse (12.5% duty)
     */
    uint32_t pulse_width_us = 500 + (angle * 2000 / 180);  /* 500-2500 μs */
    uint32_t duty = (pulse_width_us * ((1 << SERVO_DUTY_RES) - 1)) / 20000;

    ledc_set_duty(SERVO_MODE, SERVO_CHANNEL, duty);
    ledc_update_duty(SERVO_MODE, SERVO_CHANNEL);

    ESP_LOGI(TAG, "Servo angle: %d° (duty: %lu)", angle, duty);
}

/* GPIO Initialization */
static void gpio_init(void)
{
    /* Status LED */
    gpio_config_t led_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << STATUS_LED_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_conf);

    /* Button with internal pull-up */
    gpio_config_t button_conf = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 1,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&button_conf);

    ESP_LOGI(TAG, "GPIO initialized (LED:%d, Button:%d)", STATUS_LED_GPIO, BUTTON_GPIO);
}

/* Update physical lock mechanism */
static void update_lock_mechanism(bool locked)
{
    if (locked) {
        servo_set_angle(SERVO_LOCKED_ANGLE);
        gpio_set_level(STATUS_LED_GPIO, 0);  /* LED off = locked */
        ESP_LOGI(TAG, "🔒 LOCKED");
    } else {
        servo_set_angle(SERVO_UNLOCKED_ANGLE);
        gpio_set_level(STATUS_LED_GPIO, 1);  /* LED on = unlocked */
        ESP_LOGI(TAG, "🔓 UNLOCKED");
    }

    /* Give servo time to move */
    vTaskDelay(pdMS_TO_TICKS(500));
}

/* SinricPro Callbacks */
static bool on_lock_state(const char *device_id, bool *state, void *user_data)
{
    ESP_LOGI(TAG, "Lock command: %s", *state ? "LOCK" : "UNLOCK");

    lock_state = *state;
    update_lock_mechanism(lock_state);

    /* Return true if lock succeeded, false if jammed or error */
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

/* Physical button handler task */
static void button_task(void *arg)
{
    bool last_button_state = true;  /* Button is active low */

    while (1) {
        bool button_state = gpio_get_level(BUTTON_GPIO);

        /* Detect button press (transition from high to low) */
        if (button_state == false && last_button_state == true) {
            /* Toggle lock state */
            lock_state = !lock_state;
            update_lock_mechanism(lock_state);

            /* Send event to SinricPro */
            esp_err_t ret = sinricpro_lock_send_lock_state_event(
                my_lock,
                lock_state,
                SINRICPRO_CAUSE_PHYSICAL_INTERACTION
            );

            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Lock state event sent: %s", lock_state ? "LOCKED" : "UNLOCKED");
            } else if (ret == SINRICPRO_ERR_RATE_LIMITED) {
                ESP_LOGW(TAG, "Event rate limited");
            }

            /* Debounce delay */
            vTaskDelay(pdMS_TO_TICKS(300));
        }

        last_button_state = button_state;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, " SinricPro Smart Lock Example");
    ESP_LOGI(TAG, " Version: %s", sinricpro_get_version());
    ESP_LOGI(TAG, "==============================================");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    gpio_init();
    servo_init();
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

    ESP_LOGI(TAG, "Creating lock device...");
    my_lock = sinricpro_lock_create(DEVICE_ID);
    if (my_lock == NULL) {
        ESP_LOGE(TAG, "Failed to create lock");
        return;
    }

    /* Register callback */
    sinricpro_lock_on_lock_state(my_lock, on_lock_state, NULL);

    ESP_LOGI(TAG, "Starting SinricPro...");
    ESP_ERROR_CHECK(sinricpro_start());

    ESP_LOGI(TAG, "Smart lock ready!");
    ESP_LOGI(TAG, "Try: 'Alexa, lock the door'");
    ESP_LOGI(TAG, "Try: 'Alexa, unlock the door'");
    ESP_LOGI(TAG, "Press BOOT button to toggle lock manually");

    /* Start button handler task */
    xTaskCreate(button_task, "button_task", 2048, NULL, 5, NULL);

    /* Set initial locked state */
    lock_state = true;
    update_lock_mechanism(lock_state);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
