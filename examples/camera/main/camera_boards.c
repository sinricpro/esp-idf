/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 *
 * Pin mappings follow Espressif's camera_pinout.h and LilyGO/esp32-camera-bme280.
 * ESP32-S3 WROOM camera boards do not all share one layout: check PWDN too.
 */

#include "camera_boards.h"
#include "sdkconfig.h"
#include "driver/gpio.h"

camera_config_t camera_board_config(void)
{
    camera_config_t camera = {
        .pin_pwdn = -1,
        .pin_reset = -1,
        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_VGA,
        .jpeg_quality = 16,
        .fb_count = 2,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_LATEST,
    };

#if CONFIG_CAMERA_BOARD_AI_THINKER
    camera.pin_d0 = 5;
    camera.pin_d1 = 18;
    camera.pin_d2 = 19;
    camera.pin_d3 = 21;
    camera.pin_d4 = 36;
    camera.pin_d5 = 39;
    camera.pin_d6 = 34;
    camera.pin_d7 = 35;
    camera.pin_xclk = 0;
    camera.pin_sccb_sda = 26;
    camera.pin_sccb_scl = 27;
    camera.pin_vsync = 25;
    camera.pin_href = 23;
    camera.pin_pclk = 22;
    camera.pin_pwdn = 32;
#elif CONFIG_CAMERA_BOARD_ESP_EYE
    camera.pin_d0 = 34;
    camera.pin_d1 = 13;
    camera.pin_d2 = 14;
    camera.pin_d3 = 35;
    camera.pin_d4 = 39;
    camera.pin_d5 = 38;
    camera.pin_d6 = 37;
    camera.pin_d7 = 36;
    camera.pin_xclk = 4;
    camera.pin_sccb_sda = 18;
    camera.pin_sccb_scl = 23;
    camera.pin_vsync = 5;
    camera.pin_href = 27;
    camera.pin_pclk = 25;
#elif CONFIG_CAMERA_BOARD_M5CAMERA || CONFIG_CAMERA_BOARD_M5CAMERA_B
    camera.pin_d0 = 32;
    camera.pin_d1 = 35;
    camera.pin_d2 = 34;
    camera.pin_d3 = 5;
    camera.pin_d4 = 39;
    camera.pin_d5 = 18;
    camera.pin_d6 = 36;
    camera.pin_d7 = 19;
    camera.pin_xclk = 27;
    camera.pin_sccb_scl = 23;
    camera.pin_href = 26;
    camera.pin_pclk = 21;
    camera.pin_reset = 15;
#if CONFIG_CAMERA_BOARD_M5CAMERA
    camera.pin_sccb_sda = 25;
    camera.pin_vsync = 22;
#else
    camera.pin_sccb_sda = 22;
    camera.pin_vsync = 25;
#endif
#elif CONFIG_CAMERA_BOARD_WROVER_KIT
    camera.pin_d0 = 4;
    camera.pin_d1 = 5;
    camera.pin_d2 = 18;
    camera.pin_d3 = 19;
    camera.pin_d4 = 36;
    camera.pin_d5 = 39;
    camera.pin_d6 = 34;
    camera.pin_d7 = 35;
    camera.pin_xclk = 21;
    camera.pin_sccb_sda = 26;
    camera.pin_sccb_scl = 27;
    camera.pin_vsync = 25;
    camera.pin_href = 23;
    camera.pin_pclk = 22;
#elif CONFIG_CAMERA_BOARD_LILYGO_CAMERA
    camera.pin_d0 = 5;
    camera.pin_d1 = 14;
    camera.pin_d2 = 4;
    camera.pin_d3 = 15;
    camera.pin_d4 = 18;
    camera.pin_d5 = 23;
    camera.pin_d6 = 36;
    camera.pin_d7 = 39;
    camera.pin_xclk = 32;
    camera.pin_sccb_sda = 13;
    camera.pin_sccb_scl = 12;
    camera.pin_vsync = 27;
    camera.pin_href = 25;
    camera.pin_pclk = 19;
    camera.pin_pwdn = 26;
#elif CONFIG_CAMERA_BOARD_XIAO_S3_SENSE
    camera.pin_d0 = 15;
    camera.pin_d1 = 17;
    camera.pin_d2 = 18;
    camera.pin_d3 = 16;
    camera.pin_d4 = 14;
    camera.pin_d5 = 12;
    camera.pin_d6 = 11;
    camera.pin_d7 = 48;
    camera.pin_xclk = 10;
    camera.pin_sccb_sda = 40;
    camera.pin_sccb_scl = 39;
    camera.pin_vsync = 38;
    camera.pin_href = 47;
    camera.pin_pclk = 13;
#elif CONFIG_CAMERA_BOARD_FREENOVE_S3 || CONFIG_CAMERA_BOARD_ESP32S3_WROOM || CONFIG_CAMERA_BOARD_ESP32S3_GOOUUU
    camera.pin_d0 = 11;
    camera.pin_d1 = 9;
    camera.pin_d2 = 8;
    camera.pin_d3 = 10;
    camera.pin_d4 = 12;
    camera.pin_d5 = 18;
    camera.pin_d6 = 17;
    camera.pin_d7 = 16;
    camera.pin_xclk = 15;
    camera.pin_sccb_sda = 4;
    camera.pin_sccb_scl = 5;
    camera.pin_vsync = 6;
    camera.pin_href = 7;
    camera.pin_pclk = 13;
#if CONFIG_CAMERA_BOARD_ESP32S3_WROOM
    camera.pin_pwdn = 38;
#endif
#endif

    return camera;
}

const char *camera_board_name(void)
{
#if CONFIG_CAMERA_BOARD_AI_THINKER
    return "AI-Thinker ESP32-CAM";
#elif CONFIG_CAMERA_BOARD_ESP_EYE
    return "ESP-EYE";
#elif CONFIG_CAMERA_BOARD_M5CAMERA
    return "M5Camera A";
#elif CONFIG_CAMERA_BOARD_M5CAMERA_B
    return "M5Camera B";
#elif CONFIG_CAMERA_BOARD_WROVER_KIT
    return "ESP-WROVER-KIT";
#elif CONFIG_CAMERA_BOARD_LILYGO_CAMERA
    return "LILYGO T-Camera (camera only)";
#elif CONFIG_CAMERA_BOARD_XIAO_S3_SENSE
    return "XIAO ESP32S3 Sense";
#elif CONFIG_CAMERA_BOARD_FREENOVE_S3
    return "Freenove ESP32-S3";
#elif CONFIG_CAMERA_BOARD_ESP32S3_WROOM
    return "ESP32-S3 WROOM (PWDN 38)";
#elif CONFIG_CAMERA_BOARD_ESP32S3_GOOUUU
    return "GOOUUU ESP32-S3";
#else
    return "Unknown camera board";
#endif
}

void camera_board_prepare(void)
{
#if CONFIG_CAMERA_BOARD_ESP_EYE
    /* ESP-EYE's camera does not probe without pull-ups on these pins. */
    gpio_config_t pull_ups = {
        .pin_bit_mask = BIT64(GPIO_NUM_13) | BIT64(GPIO_NUM_14),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&pull_ups);
#endif
}
