/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
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
#include "sinricpro_light.h"

/* WiFi Configuration */
#define WIFI_SSID      "WIFI_SSID"
#define WIFI_PASS      "WIFI_PASS"

/* SinricPro Configuration */
#define DEVICE_ID      "DEVICE_ID"
#define APP_KEY        "APP_KEY"
#define APP_SECRET     "APP_SECRET"

/* LED PWM Configuration - RGB LED */
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_RED_GPIO           (25)
#define LEDC_GREEN_GPIO         (26)
#define LEDC_BLUE_GPIO          (27)
#define LEDC_RED_CHANNEL        LEDC_CHANNEL_0
#define LEDC_GREEN_CHANNEL      LEDC_CHANNEL_1
#define LEDC_BLUE_CHANNEL       LEDC_CHANNEL_2
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT  /* 13-bit resolution */
#define LEDC_FREQUENCY          (5000)             /* 5 kHz */

static const char *TAG = "light_example";

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

/* Light state */
static bool light_power_state = false;
static int light_brightness = 100;  /* 0-100 */
static sinricpro_color_t light_color = {255, 255, 255};  /* RGB */
static int light_color_temperature = 2700;  /* Kelvin: 2200-7000 */
static sinricpro_device_handle_t my_light = NULL;

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
static void ledc_init(void)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel[] = {
        {
            .channel    = LEDC_RED_CHANNEL,
            .duty       = 0,
            .gpio_num   = LEDC_RED_GPIO,
            .speed_mode = LEDC_MODE,
            .hpoint     = 0,
            .timer_sel  = LEDC_TIMER
        },
        {
            .channel    = LEDC_GREEN_CHANNEL,
            .duty       = 0,
            .gpio_num   = LEDC_GREEN_GPIO,
            .speed_mode = LEDC_MODE,
            .hpoint     = 0,
            .timer_sel  = LEDC_TIMER
        },
        {
            .channel    = LEDC_BLUE_CHANNEL,
            .duty       = 0,
            .gpio_num   = LEDC_BLUE_GPIO,
            .speed_mode = LEDC_MODE,
            .hpoint     = 0,
            .timer_sel  = LEDC_TIMER
        }
    };

    for (int ch = 0; ch < 3; ch++) {
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel[ch]));
    }

    ESP_LOGI(TAG, "LEDC PWM initialized (R:%d, G:%d, B:%d)",
             LEDC_RED_GPIO, LEDC_GREEN_GPIO, LEDC_BLUE_GPIO);
}

/* Convert color temperature (Kelvin) to RGB */
static void kelvin_to_rgb(int kelvin, uint8_t *r, uint8_t *g, uint8_t *b)
{
    float temp = kelvin / 100.0;
    float red, green, blue;

    /* Red */
    if (temp <= 66) {
        red = 255;
    } else {
        red = temp - 60;
        red = 329.698727446 * pow(red, -0.1332047592);
        if (red < 0) red = 0;
        if (red > 255) red = 255;
    }

    /* Green */
    if (temp <= 66) {
        green = temp;
        green = 99.4708025861 * log(green) - 161.1195681661;
    } else {
        green = temp - 60;
        green = 288.1221695283 * pow(green, -0.0755148492);
    }
    if (green < 0) green = 0;
    if (green > 255) green = 255;

    /* Blue */
    if (temp >= 66) {
        blue = 255;
    } else {
        if (temp <= 19) {
            blue = 0;
        } else {
            blue = temp - 10;
            blue = 138.5177312231 * log(blue) - 305.0447927307;
            if (blue < 0) blue = 0;
            if (blue > 255) blue = 255;
        }
    }

    *r = (uint8_t)red;
    *g = (uint8_t)green;
    *b = (uint8_t)blue;
}

/* Update RGB LED */
static void update_led(void)
{
    if (!light_power_state) {
        ledc_set_duty(LEDC_MODE, LEDC_RED_CHANNEL, 0);
        ledc_set_duty(LEDC_MODE, LEDC_GREEN_CHANNEL, 0);
        ledc_set_duty(LEDC_MODE, LEDC_BLUE_CHANNEL, 0);
    } else {
        uint8_t r, g, b;

        /* If using color temperature mode, convert to RGB */
        if (light_color_temperature > 0) {
            kelvin_to_rgb(light_color_temperature, &r, &g, &b);
        } else {
            r = light_color.r;
            g = light_color.g;
            b = light_color.b;
        }

        /* Apply brightness */
        float brightness_factor = light_brightness / 100.0;
        r = (uint8_t)(r * brightness_factor);
        g = (uint8_t)(g * brightness_factor);
        b = (uint8_t)(b * brightness_factor);

        /* Convert to PWM duty cycle (0-8191 for 13-bit) */
        uint32_t duty_r = (r * 8191) / 255;
        uint32_t duty_g = (g * 8191) / 255;
        uint32_t duty_b = (b * 8191) / 255;

        ledc_set_duty(LEDC_MODE, LEDC_RED_CHANNEL, duty_r);
        ledc_set_duty(LEDC_MODE, LEDC_GREEN_CHANNEL, duty_g);
        ledc_set_duty(LEDC_MODE, LEDC_BLUE_CHANNEL, duty_b);
    }

    ledc_update_duty(LEDC_MODE, LEDC_RED_CHANNEL);
    ledc_update_duty(LEDC_MODE, LEDC_GREEN_CHANNEL);
    ledc_update_duty(LEDC_MODE, LEDC_BLUE_CHANNEL);
}

/* SinricPro Callbacks */
static bool on_power_state(const char *device_id, bool *state, void *user_data)
{
    ESP_LOGI(TAG, "Power: %s", *state ? "ON" : "OFF");
    light_power_state = *state;
    update_led();
    return true;
}

static bool on_brightness(const char *device_id, int *brightness, void *user_data)
{
    ESP_LOGI(TAG, "Brightness: %d%%", *brightness);
    light_brightness = *brightness;
    update_led();
    return true;
}

static bool on_adjust_brightness(const char *device_id, int *delta, void *user_data)
{
    light_brightness += *delta;
    if (light_brightness < 0) light_brightness = 0;
    if (light_brightness > 100) light_brightness = 100;
    *delta = light_brightness;
    ESP_LOGI(TAG, "Adjust brightness, new: %d%%", light_brightness);
    update_led();
    return true;
}

static bool on_color(const char *device_id, sinricpro_color_t *color, void *user_data)
{
    ESP_LOGI(TAG, "Color: R=%d, G=%d, B=%d", color->r, color->g, color->b);
    light_color = *color;
    light_color_temperature = 0;  /* Disable color temperature mode */
    update_led();
    return true;
}

static bool on_color_temperature(const char *device_id, int *color_temperature, void *user_data)
{
    ESP_LOGI(TAG, "Color Temperature: %dK", *color_temperature);
    light_color_temperature = *color_temperature;
    update_led();
    return true;
}

static bool on_adjust_color_temperature(const char *device_id, int *delta, void *user_data)
{
    light_color_temperature += *delta;
    if (light_color_temperature < 2200) light_color_temperature = 2200;
    if (light_color_temperature > 7000) light_color_temperature = 7000;
    *delta = light_color_temperature;
    ESP_LOGI(TAG, "Adjust color temperature, new: %dK", light_color_temperature);
    update_led();
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

void app_main(void)
{
    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, " SinricPro Smart Light Example");
    ESP_LOGI(TAG, " Version: %s", sinricpro_get_version());
    ESP_LOGI(TAG, "==============================================");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ledc_init();
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

    ESP_LOGI(TAG, "Creating light device...");
    my_light = sinricpro_light_create(DEVICE_ID);
    if (my_light == NULL) {
        ESP_LOGE(TAG, "Failed to create light");
        return;
    }

    /* Register callbacks */
    sinricpro_light_on_power_state(my_light, on_power_state, NULL);
    sinricpro_light_on_brightness(my_light, on_brightness, NULL);
    sinricpro_light_on_adjust_brightness(my_light, on_adjust_brightness, NULL);
    sinricpro_light_on_color(my_light, on_color, NULL);
    sinricpro_light_on_color_temperature(my_light, on_color_temperature, NULL);
    sinricpro_light_on_adjust_color_temperature(my_light, on_adjust_color_temperature, NULL);

    ESP_LOGI(TAG, "Starting SinricPro...");
    ESP_ERROR_CHECK(sinricpro_start());

    ESP_LOGI(TAG, "Smart light ready!");
    ESP_LOGI(TAG, "Try: 'Alexa, turn on the light'");
    ESP_LOGI(TAG, "Try: 'Alexa, set light brightness to 50%%'");
    ESP_LOGI(TAG, "Try: 'Alexa, set light to red'");
    ESP_LOGI(TAG, "Try: 'Alexa, set light to warm white'");

    /* Turn on light with default settings */
    light_power_state = true;
    update_led();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
