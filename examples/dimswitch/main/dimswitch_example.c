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
#include "sinricpro_dimswitch.h"

/* WiFi Configuration */
#define WIFI_SSID      "WIFI_SSID"
#define WIFI_PASS      "WIFI_PASS"

/* SinricPro Configuration */
#define DEVICE_ID      "DEVICE_ID"
#define APP_KEY        "APP_KEY"
#define APP_SECRET     "APP_SECRET"

/* LED PWM Configuration */
#define LED_GPIO                (25)
#define LED_TIMER               LEDC_TIMER_0
#define LED_CHANNEL             LEDC_CHANNEL_0
#define LED_MODE                LEDC_LOW_SPEED_MODE
#define LED_DUTY_RES            LEDC_TIMER_13_BIT  /* 13-bit resolution */
#define LED_FREQUENCY           (5000)             /* 5 kHz PWM */

/* Physical controls */
#define BUTTON_GPIO             (0)    /* BOOT button for on/off toggle */

static const char *TAG = "dimswitch_example";

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

/* DimSwitch state */
static bool dimswitch_power_state = false;
static int dimswitch_brightness = 75;  /* Brightness 0-100 */
static sinricpro_device_handle_t my_dimswitch = NULL;

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

/* LED PWM Initialization */
static void led_pwm_init(void)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LED_MODE,
        .timer_num        = LED_TIMER,
        .duty_resolution  = LED_DUTY_RES,
        .freq_hz          = LED_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .channel    = LED_CHANNEL,
        .duty       = 0,
        .gpio_num   = LED_GPIO,
        .speed_mode = LED_MODE,
        .hpoint     = 0,
        .timer_sel  = LED_TIMER
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ESP_LOGI(TAG, "LED PWM initialized on GPIO %d", LED_GPIO);
}

/* GPIO Initialization */
static void gpio_init(void)
{
    /* Button with internal pull-up */
    gpio_config_t button_conf = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 1,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&button_conf);

    ESP_LOGI(TAG, "GPIO initialized (Button:%d)", BUTTON_GPIO);
}

/* Update LED brightness via PWM */
static void update_led_brightness(void)
{
    if (!dimswitch_power_state) {
        /* LED off */
        ledc_set_duty(LED_MODE, LED_CHANNEL, 0);
        ledc_update_duty(LED_MODE, LED_CHANNEL);
        ESP_LOGI(TAG, "DimSwitch: OFF");
    } else {
        /* Calculate duty cycle (0-8191 for 13-bit) */
        uint32_t duty = (dimswitch_brightness * 8191) / 100;

        ledc_set_duty(LED_MODE, LED_CHANNEL, duty);
        ledc_update_duty(LED_MODE, LED_CHANNEL);
        ESP_LOGI(TAG, "DimSwitch: ON, Brightness: %d%% (duty: %lu)",
                 dimswitch_brightness, duty);
    }
}

/* SinricPro Callbacks */
static bool on_power_state(const char *device_id, bool *state, void *user_data)
{
    ESP_LOGI(TAG, "Power: %s", *state ? "ON" : "OFF");
    dimswitch_power_state = *state;
    update_led_brightness();
    return true;
}

static bool on_power_level(const char *device_id, int *level, void *user_data)
{
    ESP_LOGI(TAG, "Brightness: %d%%", *level);

    /* Validate range */
    if (*level < 0) *level = 0;
    if (*level > 100) *level = 100;

    dimswitch_brightness = *level;

    /* Auto turn on if brightness is set while off */
    if (dimswitch_brightness > 0 && !dimswitch_power_state) {
        dimswitch_power_state = true;
    }

    update_led_brightness();
    return true;
}

static bool on_adjust_power_level(const char *device_id, int *delta, void *user_data)
{
    dimswitch_brightness += *delta;

    /* Clamp to valid range */
    if (dimswitch_brightness < 0) dimswitch_brightness = 0;
    if (dimswitch_brightness > 100) dimswitch_brightness = 100;

    *delta = dimswitch_brightness;
    ESP_LOGI(TAG, "Adjust brightness, new: %d%%", dimswitch_brightness);
    update_led_brightness();
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

/* Physical button handler - toggle on/off */
static void button_task(void *arg)
{
    bool last_button_state = true;  /* Button is active low */

    while (1) {
        bool button_state = gpio_get_level(BUTTON_GPIO);

        /* Detect button press (transition from high to low) */
        if (button_state == false && last_button_state == true) {
            /* Toggle power state */
            dimswitch_power_state = !dimswitch_power_state;
            update_led_brightness();

            /* Send event to SinricPro */
            esp_err_t ret = sinricpro_dimswitch_send_power_state_event(
                my_dimswitch,
                dimswitch_power_state,
                SINRICPRO_CAUSE_PHYSICAL_INTERACTION
            );

            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Button: %s", dimswitch_power_state ? "ON" : "OFF");
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
    ESP_LOGI(TAG, " SinricPro DimSwitch Example");
    ESP_LOGI(TAG, " Version: %s", sinricpro_get_version());
    ESP_LOGI(TAG, "==============================================");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    gpio_init();
    led_pwm_init();
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

    ESP_LOGI(TAG, "Creating dimswitch device...");
    my_dimswitch = sinricpro_dimswitch_create(DEVICE_ID);
    if (my_dimswitch == NULL) {
        ESP_LOGE(TAG, "Failed to create dimswitch");
        return;
    }

    /* Register callbacks */
    sinricpro_dimswitch_on_power_state(my_dimswitch, on_power_state, NULL);
    sinricpro_dimswitch_on_power_level(my_dimswitch, on_power_level, NULL);
    sinricpro_dimswitch_on_adjust_power_level(my_dimswitch, on_adjust_power_level, NULL);

    ESP_LOGI(TAG, "Starting SinricPro...");
    ESP_ERROR_CHECK(sinricpro_start());

    ESP_LOGI(TAG, "DimSwitch ready!");
    ESP_LOGI(TAG, "Try: 'Alexa, turn on the light'");
    ESP_LOGI(TAG, "Try: 'Alexa, dim the light'");
    ESP_LOGI(TAG, "Try: 'Alexa, set light to 50%%'");
    ESP_LOGI(TAG, "Press BOOT button to toggle on/off");

    /* Start button handler task */
    xTaskCreate(button_task, "button_task", 2048, NULL, 5, NULL);

    /* Initial state: on at 75% brightness */
    dimswitch_power_state = true;
    dimswitch_brightness = 75;
    update_led_brightness();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
