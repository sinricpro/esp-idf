/*
 * esp32-camera compatible API for MIPI-CSI cameras on the ESP32-P4.
 *
 * Covers the subset the SinricPro webrtc_camera component and examples use, so the same code
 * runs on a P4 without the parallel-camera driver. Names and types follow esp32-camera; the
 * implementation drives the sensor through esp_cam_sensor, the CSI controller and the ISP, and
 * encodes JPEG in hardware.
 *
 * Differences from esp32-camera:
 * - The sensor runs in one of two native modes (800x640 or 1280x960 on the OV5647); a frame size
 *   selects the smallest mode that holds it and frames come out at that mode's size, so check
 *   camera_fb_t width and height.
 * - PIXFORMAT_YUV422 and PIXFORMAT_YUV420 both deliver YUV420 in the packed O_UYY_E_VYY layout,
 *   the input format of the P4's hardware H.264 encoder.
 * - PIXFORMAT_RGB888 frames are stored B, G, R in memory, as the ISP and the H.264 and JPEG
 *   encoders all name "RGB888".
 * - One frame can be outstanding at a time: return it before getting the next.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PIXFORMAT_RGB565,
    PIXFORMAT_YUV422,
    PIXFORMAT_YUV420,
    PIXFORMAT_GRAYSCALE,
    PIXFORMAT_JPEG,
    PIXFORMAT_RGB888,
    PIXFORMAT_RAW,
    PIXFORMAT_RGB444,
    PIXFORMAT_RGB555,
} pixformat_t;

typedef enum {
    FRAMESIZE_96X96,
    FRAMESIZE_QQVGA,
    FRAMESIZE_128X128,
    FRAMESIZE_QCIF,
    FRAMESIZE_HQVGA,
    FRAMESIZE_240X240,
    FRAMESIZE_QVGA,
    FRAMESIZE_320X320,
    FRAMESIZE_CIF,
    FRAMESIZE_HVGA,
    FRAMESIZE_VGA,
    FRAMESIZE_SVGA,
    FRAMESIZE_XGA,
    FRAMESIZE_HD,
    FRAMESIZE_SXGA,
    FRAMESIZE_UXGA,
    FRAMESIZE_FHD,
    FRAMESIZE_INVALID,
} framesize_t;

typedef enum {
    CAMERA_GRAB_WHEN_EMPTY,
    CAMERA_GRAB_LATEST,
} camera_grab_mode_t;

typedef enum {
    CAMERA_FB_IN_PSRAM,
    CAMERA_FB_IN_DRAM,
} camera_fb_location_t;

/** The esp32-camera fields a MIPI camera can use. Parallel-bus pins and LEDC settings are absent;
 *  XCLK and frame buffer placement are accepted and ignored. */
typedef struct {
    int pin_pwdn;
    int pin_reset;
    int pin_xclk;
    int pin_sccb_sda;
    int pin_sccb_scl;
    int xclk_freq_hz;
    pixformat_t pixel_format;
    framesize_t frame_size;
    int jpeg_quality;               /**< 0-63, lower is better, as in esp32-camera */
    size_t fb_count;
    camera_fb_location_t fb_location;
    camera_grab_mode_t grab_mode;
} camera_config_t;

typedef struct {
    uint8_t *buf;
    size_t len;
    size_t width;
    size_t height;
    pixformat_t format;
    struct timeval timestamp;
} camera_fb_t;

typedef struct {
    framesize_t framesize;
    uint8_t quality;
    int8_t brightness;  /**< -2..2 */
    int8_t contrast;    /**< -2..2; levels above 0 leave contrast unchanged */
    int8_t ae_level;    /**< -2..2 */
    uint8_t vflip;
    uint8_t hmirror;
} camera_status_t;

typedef struct _sensor sensor_t;
struct _sensor {
    camera_status_t status;
    pixformat_t pixformat;
    int (*set_framesize)(sensor_t *sensor, framesize_t framesize);
    int (*set_quality)(sensor_t *sensor, int quality);
    int (*set_vflip)(sensor_t *sensor, int enable);
    int (*set_hmirror)(sensor_t *sensor, int enable);
    int (*set_brightness)(sensor_t *sensor, int level);
    int (*set_contrast)(sensor_t *sensor, int level);
    int (*set_ae_level)(sensor_t *sensor, int level);
};

/** Powers the MIPI PHY, finds the sensor on the board's camera I2C bus and starts streaming. */
esp_err_t esp_camera_init(const camera_config_t *config);

esp_err_t esp_camera_deinit(void);

/** Switches pixel format and frame size; restarts the pipeline, which takes a few hundred ms. */
esp_err_t esp_camera_reconfigure(const camera_config_t *config);

/** Waits for the next frame. NULL on timeout. */
camera_fb_t *esp_camera_fb_get(void);

void esp_camera_fb_return(camera_fb_t *fb);

sensor_t *esp_camera_sensor_get(void);

#ifdef __cplusplus
}
#endif
