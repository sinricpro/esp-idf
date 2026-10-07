/*
 * esp32-camera compatible API on the ESP32-P4: sensor RAW over MIPI-CSI -> ISP (denoise,
 * demosaic, colour matrix with white balance, sRGB gamma, sharpen) -> BGR888, which the hardware
 * JPEG encoder takes directly and webrtc_camera converts on the PPA for the H.264 encoder. Packed
 * YUV420 straight from the ISP is also offered, but decodes green and colourless on rev < 3 chips.
 */
// The per-pixel statistics loops run on every few frames; the project default (-Og) slows them 3-4x
#pragma GCC optimize("O3")

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/i2c_master.h"
#include "driver/isp.h"
#include "driver/jpeg_encode.h"
#include "esp_cam_ctlr.h"
#include "esp_cam_ctlr_csi.h"
#include "esp_cam_sensor.h"
#include "esp_cam_sensor_detect.h"
#include "esp_sccb_i2c.h"
#include "esp_sccb_intf.h"
#include "esp_camera.h"
#include "img_converters.h"

// VDD_MIPI_DPHY is fed from the chip's internal LDO channel 3 on P4 boards
#define MIPI_PHY_LDO_CHAN       3
#define MIPI_PHY_LDO_MV         2500
#define CAM_SCCB_FREQ_HZ        100000

#define BOARD_CSI_LANES         2
// Must exceed the sensor pixel rate or the ISP input FIFO overflows; 120 MHz is an exact
// divider of the 240 MHz PLL source
#define ISP_CLK_HZ              (120 * 1000 * 1000)
#define FRAME_TIMEOUT_MS        1000
#define MODE_SETTLE_MS          300
// Native sensor modes. Scaling a large frame down costs more per frame (56 ms even with the
// hardware scaler) than switching the sensor to a small mode costs once.
#define SMALL_WIDTH             800
#define SMALL_HEIGHT            640
#define LARGE_WIDTH             1280
#define LARGE_HEIGHT            960
// Sensor on-chip AE target; the driver default (80) leaves highlights clipped once gamma is applied
#define AE_TARGET               48
// Each brightness or AE level (-2..2, as in esp32-camera) moves that target by this much
#define AE_TARGET_STEP          10
// Each contrast level below 0 scales the ISP colour contrast by this; the block cannot raise it
#define CONTRAST_STEP           0.15f

// Tuning for the OV5647, whose raw colour is muted
// The ISP colour block only attenuates (0..1), so all saturation gain lives in the CCM
#define CCM_SATURATION          1.70f
#define BF_DENOISE_LEVEL        8
// Damped so a continuous stream converges without visible flicker
#define AWB_STRENGTH            0.3f
#define AWB_EVERY_N_FRAMES      8
#define DEFAULT_JPEG_QUALITY    12

#define FRAME_BUFFERS           3
#define NO_BUFFER               (-1)

static const char *TAG = "p4_camera";

static const struct {
    framesize_t size;
    uint16_t width;
    uint16_t height;
} FRAME_SIZES[] = {
    {FRAMESIZE_96X96, 96, 96},     {FRAMESIZE_QQVGA, 160, 120},  {FRAMESIZE_128X128, 128, 128},
    {FRAMESIZE_QCIF, 176, 144},    {FRAMESIZE_HQVGA, 240, 176},  {FRAMESIZE_240X240, 240, 240},
    {FRAMESIZE_QVGA, 320, 240},    {FRAMESIZE_320X320, 320, 320}, {FRAMESIZE_CIF, 400, 296},
    {FRAMESIZE_HVGA, 480, 320},    {FRAMESIZE_VGA, 640, 480},    {FRAMESIZE_SVGA, 800, 600},
    {FRAMESIZE_XGA, 1024, 768},    {FRAMESIZE_HD, 1280, 720},    {FRAMESIZE_SXGA, 1280, 1024},
    {FRAMESIZE_UXGA, 1600, 1200},  {FRAMESIZE_FHD, 1920, 1080},
};

static esp_cam_sensor_device_t *s_cam;
static const esp_cam_sensor_format_t *s_fmt;    // mode the pipeline is running in
static const esp_cam_sensor_format_t *s_fmt_small;
static const esp_cam_sensor_format_t *s_fmt_large;
static bool s_yuv;                              // pipeline outputs YUV420 rather than BGR888
static esp_cam_ctlr_handle_t s_csi;
static isp_proc_handle_t s_isp;
static jpeg_encoder_handle_t s_jpeg;
// Recursive: a task holding a frame may change sensor settings before returning it
static SemaphoreHandle_t s_lock;
static SemaphoreHandle_t s_frame_done;
static bool s_started;

/*
 * Triple buffering. The CSI driver asks for the next frame's buffer at the same interrupt that
 * completes the current one, so capturing every sensor frame needs a buffer that is neither
 * being filled nor held by the reader. Each buffer doubles as JPEG encoder input.
 */
static uint8_t *s_frames[FRAME_BUFFERS];
static portMUX_TYPE s_frames_mux = portMUX_INITIALIZER_UNLOCKED;
static int s_filling = NO_BUFFER;
static int s_ready = NO_BUFFER;
static int s_reading = NO_BUFFER;
static size_t s_frame_len;
static uint8_t *s_jpeg_out;
static size_t s_jpeg_cap;

static camera_fb_t s_fb;
static bool s_fb_out;
static sensor_t s_sensor;
static uint32_t s_awb_countdown;
static float s_gain_r = 1.1f;
static float s_gain_b = 1.2f;
static float s_srgb_to_linear[256];

static bool IRAM_ATTR on_get_new_trans(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data)
{
    portENTER_CRITICAL_ISR(&s_frames_mux);
    // The frame completing right now still occupies s_filling; an older untaken frame may be overwritten
    int next = 0;
    while (next == s_filling || next == s_reading) {
        next++;
    }
    if (s_ready == next) {
        s_ready = NO_BUFFER;
    }
    s_filling = next;
    portEXIT_CRITICAL_ISR(&s_frames_mux);
    trans->buffer = s_frames[next];
    trans->buflen = s_frame_len;
    return false;
}

static bool IRAM_ATTR on_trans_finished(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data)
{
    BaseType_t woken = pdFALSE;
    for (int i = 0; i < FRAME_BUFFERS; i++) {
        if (trans->buffer == s_frames[i]) {
            portENTER_CRITICAL_ISR(&s_frames_mux);
            s_ready = i;
            portEXIT_CRITICAL_ISR(&s_frames_mux);
            xSemaphoreGiveFromISR(s_frame_done, &woken);
        }
    }
    return woken == pdTRUE;
}

static bool raw_isp_color(esp_cam_sensor_output_format_t f, isp_color_t *isp_in)
{
    switch (f) {
    case ESP_CAM_SENSOR_PIXFORMAT_RAW8:
        *isp_in = ISP_COLOR_RAW8;
        return true;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW10:
        *isp_in = ISP_COLOR_RAW10;
        return true;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW12:
        *isp_in = ISP_COLOR_RAW12;
        return true;
    default:
        return false;
    }
}

static color_raw_element_order_t isp_bayer_order(const esp_cam_sensor_format_t *fmt)
{
    if (!fmt->isp_info) {
        return COLOR_RAW_ELEMENT_ORDER_BGGR;
    }
    switch (fmt->isp_info->isp_v1_info.bayer_type) {
    case ESP_CAM_SENSOR_BAYER_RGGB: return COLOR_RAW_ELEMENT_ORDER_RGGB;
    case ESP_CAM_SENSOR_BAYER_GRBG: return COLOR_RAW_ELEMENT_ORDER_GRBG;
    case ESP_CAM_SENSOR_BAYER_GBRG: return COLOR_RAW_ELEMENT_ORDER_GBRG;
    default:                        return COLOR_RAW_ELEMENT_ORDER_BGGR;
    }
}

static uint32_t srgb_gamma_curve(uint32_t x)
{
    if (x >= 256) {
        return 256;
    }
    float v = x / 256.0f;
    float enc = v <= 0.0031308f ? 12.92f * v : 1.055f * powf(v, 1.0f / 2.4f) - 0.055f;
    uint32_t y = (uint32_t)lroundf(256.0f * enc);
    return y > 255 ? 255 : y;
}

/*
 * White balance has to live in the colour matrix: the dedicated WBG block needs chip rev >= 3.0.
 * Matrix = saturation(linear, Rec.709 luma) * diag(gain_r, 1, gain_b).
 * Saturation rows sum to 1, so neutral greys stay neutral.
 */
static void ccm_apply(void)
{
    static const float luma[3] = {0.2126f, 0.7152f, 0.0722f};
    const float wb[3] = {s_gain_r, 1.0f, s_gain_b};
    esp_isp_ccm_config_t cfg = {.saturation = true};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            float s = (1.0f - CCM_SATURATION) * luma[j] + (i == j ? CCM_SATURATION : 0.0f);
            cfg.matrix[i][j] = s * wb[j];
        }
    }
    esp_isp_ccm_configure(s_isp, &cfg);
}

static void clamp_gains(void)
{
    s_gain_r = fminf(fmaxf(s_gain_r, 0.5f), 3.5f);
    s_gain_b = fminf(fmaxf(s_gain_b, 0.5f), 3.5f);
}

// Grey-world estimate on BGR888: undo the sRGB curve, average unclipped pixels, nudge R/B gains
static void awb_update_bgr(const uint8_t *frame)
{
    float sum_r = 0, sum_g = 0, sum_b = 0;
    uint32_t n = 0;
    size_t pixels = (size_t)s_fmt->width * s_fmt->height;
    for (size_t i = 0; i < pixels; i += 29) {
        const uint8_t *p = frame + i * 3;
        uint8_t mx = p[0] > p[1] ? (p[0] > p[2] ? p[0] : p[2]) : (p[1] > p[2] ? p[1] : p[2]);
        if (mx >= 250 || mx < 12) {
            continue;
        }
        sum_b += s_srgb_to_linear[p[0]];
        sum_g += s_srgb_to_linear[p[1]];
        sum_r += s_srgb_to_linear[p[2]];
        n++;
    }
    if (n < 100 || sum_r <= 0 || sum_g <= 0 || sum_b <= 0) {
        return;
    }
    s_gain_r *= powf(sum_g / sum_r, AWB_STRENGTH);
    s_gain_b *= powf(sum_g / sum_b, AWB_STRENGTH);
    clamp_gains();
    ccm_apply();
}

/*
 * Grey-world on packed YUV420 (O_UYY_E_VYY): even lines carry U Y Y triplets, odd lines V Y Y.
 * A grey scene averages U = V = 128; red excess raises V and blue excess raises U.
 */
static void awb_update_yuv(const uint8_t *frame)
{
    const size_t stride = (size_t)s_fmt->width * 3 / 2;
    int64_t sum_u = 0, sum_v = 0;
    uint32_t n_u = 0, n_v = 0;
    for (size_t y = 0; y < s_fmt->height; y += 7) {
        const uint8_t *line = frame + y * stride;
        for (size_t x = 0; x + 2 < stride; x += 3 * 13) {
            uint8_t luma = line[x + 1];
            // Limited range: skip near-clipped highlights and near-black
            if (luma >= 230 || luma < 28) {
                continue;
            }
            if (y & 1) {
                sum_v += line[x];
                n_v++;
            } else {
                sum_u += line[x];
                n_u++;
            }
        }
    }
    if (n_u < 50 || n_v < 50) {
        return;
    }
    float du = (float)sum_u / n_u - 128.0f;
    float dv = (float)sum_v / n_v - 128.0f;
    s_gain_r *= expf(-0.01f * AWB_STRENGTH * dv);
    s_gain_b *= expf(-0.01f * AWB_STRENGTH * du);
    clamp_gains();
    ccm_apply();
}

// Enables the ISP stages; any stage the silicon rejects is skipped with a warning
static void isp_tuning_init(void)
{
    esp_isp_bf_config_t bf = {
        .denoising_level = BF_DENOISE_LEVEL,
        .padding_mode = ISP_BF_EDGE_PADDING_MODE_SRND_DATA,
        .bf_template = {{1, 2, 1}, {2, 4, 2}, {1, 2, 1}},
    };
    if (esp_isp_bf_configure(s_isp, &bf) != ESP_OK || esp_isp_bf_enable(s_isp) != ESP_OK) {
        ESP_LOGW(TAG, "ISP denoise unavailable");
    }

    ccm_apply();
    if (esp_isp_ccm_enable(s_isp) != ESP_OK) {
        ESP_LOGW(TAG, "ISP colour matrix unavailable");
    }

    isp_gamma_curve_points_t pts;
    esp_isp_gamma_fill_curve_points(srgb_gamma_curve, &pts);
    esp_isp_gamma_configure(s_isp, COLOR_COMPONENT_R, &pts);
    esp_isp_gamma_configure(s_isp, COLOR_COMPONENT_G, &pts);
    esp_isp_gamma_configure(s_isp, COLOR_COMPONENT_B, &pts);
    if (esp_isp_gamma_enable(s_isp) != ESP_OK) {
        ESP_LOGW(TAG, "ISP gamma unavailable");
    }

    // Coefficients are fixed point: integer part plus 5 fractional bits. The low threshold keeps
    // sensor noise below it from being sharpened into coloured speckle.
    esp_isp_sharpen_config_t sharpen = {
        .h_freq_coeff = {.integer = 1, .decimal = 8},
        .m_freq_coeff = {.integer = 1, .decimal = 0},
        .h_thresh = 255,
        .l_thresh = 16,
        .padding_mode = ISP_SHARPEN_EDGE_PADDING_MODE_SRND_DATA,
        .sharpen_template = {{1, 2, 1}, {2, 4, 2}, {1, 2, 1}},
    };
    if (esp_isp_sharpen_configure(s_isp, &sharpen) != ESP_OK || esp_isp_sharpen_enable(s_isp) != ESP_OK) {
        ESP_LOGW(TAG, "ISP sharpen unavailable");
    }

    // Contrast/saturation are 1.7 fixed point with a documented range of 0..1: 128 == 1.0
    int contrast = s_sensor.status.contrast < 0 ? s_sensor.status.contrast : 0;
    esp_isp_color_config_t color = {
        .color_contrast = {.val = (uint32_t)lroundf(128 * (1.0f + CONTRAST_STEP * contrast))},
        .color_saturation = {.val = 128},
    };
    if (esp_isp_color_configure(s_isp, &color) != ESP_OK || esp_isp_color_enable(s_isp) != ESP_OK) {
        ESP_LOGW(TAG, "ISP colour adjust unavailable");
    }
}

static esp_err_t detect_sensor(i2c_master_bus_handle_t bus)
{
    esp_cam_sensor_config_t cam_config = {
        .reset_pin = -1,
        .pwdn_pin = -1,
        .xclk_pin = -1,
        .sensor_port = ESP_CAM_SENSOR_MIPI_CSI,
    };
    // The accessor works whether esp_cam_sensor keeps its detect table in a linker section (ESP-IDF
    // default) or a static array (the Arduino core)
    esp_cam_sensor_detect_fn_t *first = NULL, *end = NULL;
    esp_cam_sensor_detect_get_array(&first, &end);
    for (esp_cam_sensor_detect_fn_t *p = first; p < end; ++p) {
        if (p->port != ESP_CAM_SENSOR_MIPI_CSI || i2c_master_probe(bus, p->sccb_addr, 50) != ESP_OK) {
            continue;
        }
        sccb_i2c_config_t sccb_config = {
            .scl_speed_hz = CAM_SCCB_FREQ_HZ,
            .device_address = p->sccb_addr,
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        };
        if (sccb_new_i2c_io(bus, &sccb_config, &cam_config.sccb_handle) != ESP_OK) {
            continue;
        }
        s_cam = (*(p->detect))(&cam_config);
        if (s_cam) {
            return ESP_OK;
        }
        esp_sccb_del_i2c_io(cam_config.sccb_handle);
    }
    return ESP_ERR_NOT_FOUND;
}

// Picks the usable mode whose pixel count is closest to the request
static const esp_cam_sensor_format_t *choose_format(int want_width, int want_height)
{
    esp_cam_sensor_format_array_t fmts = {0};
    if (esp_cam_sensor_query_format(s_cam, &fmts) != ESP_OK) {
        return NULL;
    }
    const esp_cam_sensor_format_t *best = NULL;
    long best_diff = 0;
    for (int i = 0; i < fmts.count; i++) {
        const esp_cam_sensor_format_t *f = &fmts.format_array[i];
        isp_color_t unused;
        if (f->port != ESP_CAM_SENSOR_MIPI_CSI || f->mipi_info.lane_num > BOARD_CSI_LANES || !raw_isp_color(f->format, &unused)) {
            continue;
        }
        long diff = labs((long)f->width * f->height - (long)want_width * want_height)
                    + labs((long)f->width - want_width) + labs((long)f->height - want_height);
        if (!best || diff < best_diff) {
            best = f;
            best_diff = diff;
        }
    }
    return best;
}

// The smallest native mode that holds the requested frame size
static const esp_cam_sensor_format_t *format_for_size(framesize_t size)
{
    for (size_t i = 0; i < sizeof(FRAME_SIZES) / sizeof(FRAME_SIZES[0]); i++) {
        if (FRAME_SIZES[i].size == size) {
            bool fits = FRAME_SIZES[i].width <= s_fmt_small->width && FRAME_SIZES[i].height <= s_fmt_small->height;
            return fits ? s_fmt_small : s_fmt_large;
        }
    }
    return s_fmt_small;
}

// Brightness and AE level both act on the sensor's auto-exposure target
static esp_err_t apply_exposure(void)
{
    int target = AE_TARGET + AE_TARGET_STEP * (s_sensor.status.ae_level + s_sensor.status.brightness);
    target = target < 16 ? 16 : (target > 120 ? 120 : target);
    return esp_cam_sensor_set_para_value(s_cam, ESP_CAM_SENSOR_EXPOSURE_VAL, &target, sizeof(target));
}

static esp_err_t apply_flip_mirror(void)
{
    int vflip = s_sensor.status.vflip;
    int hmirror = s_sensor.status.hmirror;
    esp_err_t ret = esp_cam_sensor_set_para_value(s_cam, ESP_CAM_SENSOR_VFLIP, &vflip, sizeof(vflip));
    if (ret == ESP_OK) {
        ret = esp_cam_sensor_set_para_value(s_cam, ESP_CAM_SENSOR_HMIRROR, &hmirror, sizeof(hmirror));
    }
    return ret;
}

// Sensor -> CSI PHY -> ISP -> CSI bridge -> DMA into s_frames
static esp_err_t pipeline_open(const esp_cam_sensor_format_t *fmt, bool yuv)
{
    isp_color_t isp_in = ISP_COLOR_RAW8;
    raw_isp_color(fmt->format, &isp_in);

    ESP_RETURN_ON_ERROR(esp_cam_sensor_set_format(s_cam, fmt), TAG, "sensor rejected the mode");
    apply_exposure();
    apply_flip_mirror();
    s_fmt = fmt;
    s_yuv = yuv;
    s_frame_len = (size_t)fmt->width * fmt->height * (yuv ? 3 : 6) / 2;

    // The ISP sits between the PHY and the CSI bridge, so the bridge already sees the ISP's
    // output format; the bridge itself cannot convert colour formats on chips below v3.0
    cam_ctlr_color_t bridge_color = yuv ? CAM_CTLR_COLOR_YUV420 : CAM_CTLR_COLOR_RGB888;
    esp_cam_ctlr_csi_config_t csi_config = {
        .ctlr_id = 0,
        .h_res = fmt->width,
        .v_res = fmt->height,
        .lane_bit_rate_mbps = fmt->mipi_info.mipi_clk / 1000000,
        .input_data_color_type = bridge_color,
        .output_data_color_type = bridge_color,
        .data_lane_num = fmt->mipi_info.lane_num,
        .byte_swap_en = false,
        .queue_items = 1,
    };
    ESP_RETURN_ON_ERROR(esp_cam_new_csi_ctlr(&csi_config, &s_csi), TAG, "CSI controller init failed");

    esp_cam_ctlr_evt_cbs_t cbs = {
        .on_get_new_trans = on_get_new_trans,
        .on_trans_finished = on_trans_finished,
    };
    ESP_RETURN_ON_ERROR(esp_cam_ctlr_register_event_callbacks(s_csi, &cbs, NULL), TAG, "CSI callbacks");
    ESP_RETURN_ON_ERROR(esp_cam_ctlr_enable(s_csi), TAG, "CSI enable");

    esp_isp_processor_cfg_t isp_config = {
        .clk_hz = ISP_CLK_HZ,
        .input_data_source = ISP_INPUT_DATA_SOURCE_CSI,
        .input_data_color_type = isp_in,
        .output_data_color_type = yuv ? ISP_COLOR_YUV420 : ISP_COLOR_RGB888,
        // esp_h264's SPS carries no video_signal_type, so decoders assume limited range
        .yuv_range = ISP_COLOR_RANGE_LIMIT,
        .yuv_std = ISP_YUV_CONV_STD_BT601,
        .has_line_start_packet = fmt->mipi_info.line_sync_en,
        .has_line_end_packet = fmt->mipi_info.line_sync_en,
        .h_res = fmt->width,
        .v_res = fmt->height,
        .bayer_order = isp_bayer_order(fmt),
    };
    ESP_RETURN_ON_ERROR(esp_isp_new_processor(&isp_config, &s_isp), TAG, "ISP init failed");
    ESP_RETURN_ON_ERROR(esp_isp_enable(s_isp), TAG, "ISP enable");
    isp_tuning_init();

    ESP_RETURN_ON_ERROR(esp_cam_ctlr_start(s_csi), TAG, "CSI start failed");
    int stream = 1;
    ESP_RETURN_ON_ERROR(esp_cam_sensor_ioctl(s_cam, ESP_CAM_SENSOR_IOC_S_STREAM, &stream), TAG, "stream on");
    // Let the sensor's auto exposure converge before a frame from the new mode is used
    vTaskDelay(pdMS_TO_TICKS(MODE_SETTLE_MS));
    s_awb_countdown = 0;
    return ESP_OK;
}

static void pipeline_close(void)
{
    if (!s_csi) {
        return;
    }
    int stream = 0;
    esp_cam_sensor_ioctl(s_cam, ESP_CAM_SENSOR_IOC_S_STREAM, &stream);
    esp_cam_ctlr_stop(s_csi);
    esp_isp_disable(s_isp);
    esp_isp_del_processor(s_isp);
    esp_cam_ctlr_disable(s_csi);
    esp_cam_ctlr_del(s_csi);
    s_isp = NULL;
    s_csi = NULL;
    // Frames from the old mode have a different size or format
    s_filling = NO_BUFFER;
    s_ready = NO_BUFFER;
    s_reading = NO_BUFFER;
    xSemaphoreTake(s_frame_done, 0);
}

static bool format_is_yuv(pixformat_t format)
{
    return format == PIXFORMAT_YUV422 || format == PIXFORMAT_YUV420;
}

// Switches pipeline mode when needed. Caller holds s_lock.
static esp_err_t select_mode(const esp_cam_sensor_format_t *fmt, bool yuv)
{
    if (fmt == s_fmt && yuv == s_yuv && s_csi) {
        return ESP_OK;
    }
    ESP_LOGI(TAG, "%ux%u %s", fmt->width, fmt->height, yuv ? "YUV420" : "JPEG");
    pipeline_close();
    return pipeline_open(fmt, yuv);
}

// esp32-camera JPEG quality runs 0-63 with lower meaning better; the encoder takes 1-100
static uint32_t jpeg_quality_from_camera(int quality)
{
    int q = 100 - quality * 2;
    return q < 10 ? 10 : (q > 95 ? 95 : q);
}

// src is BGR888, which the JPEG engine calls RGB888
static esp_err_t encode_jpeg(const uint8_t *src, size_t src_len, uint32_t quality,
                             uint8_t *out, size_t out_cap, uint32_t *out_len)
{
    jpeg_encode_cfg_t cfg = {
        .width = s_fmt->width,
        .height = s_fmt->height,
        .src_type = JPEG_ENCODE_IN_FORMAT_RGB888,
        .sub_sample = JPEG_DOWN_SAMPLING_YUV420,
        .image_quality = quality,
    };
    return jpeg_encoder_process(s_jpeg, &cfg, src, src_len, out, out_cap, out_len);
}

static int sensor_set_framesize(sensor_t *sensor, framesize_t framesize)
{
    xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
    esp_err_t ret = select_mode(format_for_size(framesize), s_yuv);
    if (ret == ESP_OK) {
        sensor->status.framesize = framesize;
    }
    xSemaphoreGiveRecursive(s_lock);
    return ret == ESP_OK ? 0 : -1;
}

static int sensor_set_quality(sensor_t *sensor, int quality)
{
    sensor->status.quality = quality < 0 ? 0 : (quality > 63 ? 63 : quality);
    return 0;
}

static int sensor_set_vflip(sensor_t *sensor, int enable)
{
    xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
    sensor->status.vflip = enable ? 1 : 0;
    esp_err_t ret = apply_flip_mirror();
    xSemaphoreGiveRecursive(s_lock);
    return ret == ESP_OK ? 0 : -1;
}

static int sensor_set_hmirror(sensor_t *sensor, int enable)
{
    xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
    sensor->status.hmirror = enable ? 1 : 0;
    esp_err_t ret = apply_flip_mirror();
    xSemaphoreGiveRecursive(s_lock);
    return ret == ESP_OK ? 0 : -1;
}

static int clamp_level(int level)
{
    return level < -2 ? -2 : (level > 2 ? 2 : level);
}

static int sensor_set_brightness(sensor_t *sensor, int level)
{
    xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
    sensor->status.brightness = clamp_level(level);
    esp_err_t ret = apply_exposure();
    xSemaphoreGiveRecursive(s_lock);
    return ret == ESP_OK ? 0 : -1;
}

static int sensor_set_ae_level(sensor_t *sensor, int level)
{
    xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
    sensor->status.ae_level = clamp_level(level);
    esp_err_t ret = apply_exposure();
    xSemaphoreGiveRecursive(s_lock);
    return ret == ESP_OK ? 0 : -1;
}

// Levels above 0 are accepted but leave contrast unchanged: the ISP colour block only attenuates
static int sensor_set_contrast(sensor_t *sensor, int level)
{
    xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
    sensor->status.contrast = clamp_level(level);
    int contrast = sensor->status.contrast < 0 ? sensor->status.contrast : 0;
    esp_isp_color_config_t color = {
        .color_contrast = {.val = (uint32_t)lroundf(128 * (1.0f + CONTRAST_STEP * contrast))},
        .color_saturation = {.val = 128},
    };
    esp_err_t ret = s_isp ? esp_isp_color_configure(s_isp, &color) : ESP_OK;
    xSemaphoreGiveRecursive(s_lock);
    return ret == ESP_OK ? 0 : -1;
}

esp_err_t esp_camera_init(const camera_config_t *config)
{
    ESP_RETURN_ON_FALSE(config && !s_started, ESP_ERR_INVALID_STATE, TAG, "already initialised or no config");
    ESP_RETURN_ON_FALSE(config->pin_sccb_sda >= 0 && config->pin_sccb_scl >= 0, ESP_ERR_INVALID_ARG, TAG,
                        "pin_sccb_sda and pin_sccb_scl are required");

    for (int i = 0; i < 256; i++) {
        float v = i / 255.0f;
        s_srgb_to_linear[i] = v <= 0.04045f ? v / 12.92f : powf((v + 0.055f) / 1.055f, 2.4f);
    }
    s_lock = xSemaphoreCreateRecursiveMutex();
    s_frame_done = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(s_lock && s_frame_done, ESP_ERR_NO_MEM, TAG, "no memory for locks");

    esp_ldo_channel_handle_t ldo_mipi_phy = NULL;
    esp_ldo_channel_config_t ldo_config = {
        .chan_id = MIPI_PHY_LDO_CHAN,
        .voltage_mv = MIPI_PHY_LDO_MV,
    };
    ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&ldo_config, &ldo_mipi_phy), TAG, "MIPI PHY LDO");

    i2c_master_bus_config_t i2c_bus_conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .sda_io_num = config->pin_sccb_sda,
        .scl_io_num = config->pin_sccb_scl,
        // Any free port: the application may already own port 0 for its own sensors
        .i2c_port = -1,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus = NULL;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&i2c_bus_conf, &bus), TAG, "camera I2C bus");
    // Sensors need a moment after power-up before they answer on SCCB
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_RETURN_ON_ERROR(detect_sensor(bus), TAG, "no camera sensor answered: check the ribbon cable (camera cable, not display)");
    s_fmt_small = choose_format(SMALL_WIDTH, SMALL_HEIGHT);
    s_fmt_large = choose_format(LARGE_WIDTH, LARGE_HEIGHT);
    ESP_RETURN_ON_FALSE(s_fmt_small && s_fmt_large, ESP_ERR_NOT_SUPPORTED, TAG, "sensor has no usable mode");
    ESP_LOGI(TAG, "%s: modes %ux%u and %ux%u", s_cam->name, s_fmt_small->width, s_fmt_small->height,
             s_fmt_large->width, s_fmt_large->height);

    // Buffers are sized for BGR888 at the larger mode and reused for everything smaller
    size_t pixels = (size_t)s_fmt_large->width * s_fmt_large->height;
    size_t small_pixels = (size_t)s_fmt_small->width * s_fmt_small->height;
    pixels = small_pixels > pixels ? small_pixels : pixels;
    jpeg_encode_memory_alloc_cfg_t in_cfg = {.buffer_direction = JPEG_ENC_ALLOC_INPUT_BUFFER};
    jpeg_encode_memory_alloc_cfg_t out_cfg = {.buffer_direction = JPEG_ENC_ALLOC_OUTPUT_BUFFER};
    size_t allocated = 0;
    for (int i = 0; i < FRAME_BUFFERS; i++) {
        s_frames[i] = jpeg_alloc_encoder_mem(pixels * 3, &in_cfg, &allocated);
        ESP_RETURN_ON_FALSE(s_frames[i], ESP_ERR_NO_MEM, TAG, "no memory for frame buffers");
    }
    // A camera JPEG stays well below one byte per pixel
    s_jpeg_out = jpeg_alloc_encoder_mem(pixels, &out_cfg, &s_jpeg_cap);
    ESP_RETURN_ON_FALSE(s_jpeg_out, ESP_ERR_NO_MEM, TAG, "no memory for the JPEG buffer");

    jpeg_encode_engine_cfg_t eng_cfg = {.timeout_ms = 2000};
    ESP_RETURN_ON_ERROR(jpeg_new_encoder_engine(&eng_cfg, &s_jpeg), TAG, "JPEG encoder");

    s_sensor = (sensor_t){
        .status = {
            .framesize = config->frame_size,
            .quality = config->jpeg_quality > 0 ? config->jpeg_quality : DEFAULT_JPEG_QUALITY,
        },
        .pixformat = config->pixel_format,
        .set_framesize = sensor_set_framesize,
        .set_quality = sensor_set_quality,
        .set_vflip = sensor_set_vflip,
        .set_hmirror = sensor_set_hmirror,
        .set_brightness = sensor_set_brightness,
        .set_contrast = sensor_set_contrast,
        .set_ae_level = sensor_set_ae_level,
    };

    xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
    esp_err_t ret = select_mode(format_for_size(config->frame_size), format_is_yuv(config->pixel_format));
    xSemaphoreGiveRecursive(s_lock);
    s_started = ret == ESP_OK;
    return ret;
}

esp_err_t esp_camera_deinit(void)
{
    if (!s_started) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
    pipeline_close();
    xSemaphoreGiveRecursive(s_lock);
    return ESP_OK;
}

esp_err_t esp_camera_reconfigure(const camera_config_t *config)
{
    ESP_RETURN_ON_FALSE(s_started && config, ESP_ERR_INVALID_STATE, TAG, "camera not initialised");
    xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
    esp_err_t ret = select_mode(format_for_size(config->frame_size), format_is_yuv(config->pixel_format));
    if (ret == ESP_OK) {
        s_sensor.status.framesize = config->frame_size;
        s_sensor.pixformat = config->pixel_format;
    }
    xSemaphoreGiveRecursive(s_lock);
    return ret;
}

camera_fb_t *esp_camera_fb_get(void)
{
    if (!s_started) {
        return NULL;
    }
    xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
    if (s_fb_out || !s_csi) {
        ESP_LOGE(TAG, "%s", s_fb_out ? "return the previous frame before getting another" : "pipeline is not running");
        xSemaphoreGiveRecursive(s_lock);
        return NULL;
    }

    // Wait for a frame newer than the last one taken, then claim it so the sensor leaves it alone
    int idx = NO_BUFFER;
    while (idx == NO_BUFFER) {
        if (xSemaphoreTake(s_frame_done, pdMS_TO_TICKS(FRAME_TIMEOUT_MS)) != pdTRUE) {
            xSemaphoreGiveRecursive(s_lock);
            return NULL;
        }
        portENTER_CRITICAL(&s_frames_mux);
        idx = s_ready;
        s_ready = NO_BUFFER;
        s_reading = idx;
        portEXIT_CRITICAL(&s_frames_mux);
    }
    const uint8_t *frame = s_frames[idx];

    if (s_awb_countdown-- == 0) {
        // Sampling costs tens of milliseconds of PSRAM cache misses, and the light rarely changes that fast
        if (s_yuv) {
            awb_update_yuv(frame);
        } else {
            awb_update_bgr(frame);
        }
        s_awb_countdown = AWB_EVERY_N_FRAMES - 1;
    }

    s_fb.width = s_fmt->width;
    s_fb.height = s_fmt->height;
    int64_t now = esp_timer_get_time();
    s_fb.timestamp.tv_sec = now / 1000000;
    s_fb.timestamp.tv_usec = now % 1000000;

    if (s_yuv || s_sensor.pixformat == PIXFORMAT_RGB888) {
        // Raw frames stay claimed until returned; the encoder reads them in place
        s_fb.buf = (uint8_t *)frame;
        s_fb.len = s_frame_len;
        s_fb.format = s_yuv ? PIXFORMAT_YUV420 : PIXFORMAT_RGB888;
    } else {
        uint32_t len = 0;
        esp_err_t ret = encode_jpeg(frame, s_frame_len, jpeg_quality_from_camera(s_sensor.status.quality),
                                    s_jpeg_out, s_jpeg_cap, &len);
        // The JPEG lives in its own buffer, so the raw frame goes back to the sensor already
        portENTER_CRITICAL(&s_frames_mux);
        s_reading = NO_BUFFER;
        portEXIT_CRITICAL(&s_frames_mux);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "JPEG encode failed: %s", esp_err_to_name(ret));
            xSemaphoreGiveRecursive(s_lock);
            return NULL;
        }
        s_fb.buf = s_jpeg_out;
        s_fb.len = len;
        s_fb.format = PIXFORMAT_JPEG;
    }
    // s_lock stays held until the frame is returned
    s_fb_out = true;
    return &s_fb;
}

void esp_camera_fb_return(camera_fb_t *fb)
{
    if (fb != &s_fb || !s_fb_out) {
        return;
    }
    portENTER_CRITICAL(&s_frames_mux);
    s_reading = NO_BUFFER;
    portEXIT_CRITICAL(&s_frames_mux);
    s_fb_out = false;
    xSemaphoreGiveRecursive(s_lock);
}

sensor_t *esp_camera_sensor_get(void)
{
    return s_started ? &s_sensor : NULL;
}

static inline uint8_t clamp_u8(int v)
{
    return v < 0 ? 0 : (v > 255 ? 255 : v);
}

/*
 * Packed YUV420 (O_UYY_E_VYY, limited range BT.601) to BGR888. Even lines carry U Y Y triplets and
 * odd lines V Y Y, so each 2x2 block shares the U from its even line and the V from its odd line.
 */
static void yuv420_to_bgr888(const uint8_t *src, uint8_t *dst, size_t width, size_t height)
{
    const size_t stride = width * 3 / 2;
    for (size_t y = 0; y + 1 < height; y += 2) {
        const uint8_t *even = src + y * stride;
        const uint8_t *odd = even + stride;
        uint8_t *out_even = dst + y * width * 3;
        uint8_t *out_odd = out_even + width * 3;
        for (size_t x = 0; x < width; x += 2) {
            const uint8_t *e = even + x * 3 / 2;
            const uint8_t *o = odd + x * 3 / 2;
            int u = e[0] - 128;
            int v = o[0] - 128;
            // 16.16 fixed point: 1.596, 0.392, 0.813, 2.017 (chroma scaled 224 -> 255)
            int r_off = 104595 * v;
            int g_off = -25690 * u - 53281 * v;
            int b_off = 132186 * u;
            const uint8_t luma[4] = {e[1], e[2], o[1], o[2]};
            uint8_t *px[4] = {out_even + x * 3, out_even + x * 3 + 3, out_odd + x * 3, out_odd + x * 3 + 3};
            for (int i = 0; i < 4; i++) {
                // 1.164 * (Y - 16) expands 16..235 to 0..255
                int yl = 76284 * (luma[i] - 16) + 32768;
                px[i][0] = clamp_u8((yl + b_off) >> 16);
                px[i][1] = clamp_u8((yl + g_off) >> 16);
                px[i][2] = clamp_u8((yl + r_off) >> 16);
            }
        }
    }
}

bool frame2jpg(camera_fb_t *fb, uint8_t quality, uint8_t **out, size_t *out_len)
{
    if (!fb || !out || !out_len || !s_started) {
        return false;
    }
    if (fb->format == PIXFORMAT_JPEG) {
        *out = malloc(fb->len);
        if (!*out) {
            return false;
        }
        memcpy(*out, fb->buf, fb->len);
        *out_len = fb->len;
        return true;
    }
    if (fb->format != PIXFORMAT_YUV420 && fb->format != PIXFORMAT_RGB888) {
        return false;
    }

    const uint8_t *src = fb->buf;
    size_t src_len = fb->len;
    uint8_t *converted = NULL;
    if (fb->format == PIXFORMAT_YUV420) {
        // The JPEG engine takes YUV420 only on chip rev >= 3.0, so convert to BGR888 first
        jpeg_encode_memory_alloc_cfg_t in_cfg = {.buffer_direction = JPEG_ENC_ALLOC_INPUT_BUFFER};
        size_t in_cap = 0;
        src_len = fb->width * fb->height * 3;
        converted = jpeg_alloc_encoder_mem(src_len, &in_cfg, &in_cap);
        if (!converted) {
            return false;
        }
        yuv420_to_bgr888(fb->buf, converted, fb->width, fb->height);
        src = converted;
    }

    jpeg_encode_memory_alloc_cfg_t out_cfg = {.buffer_direction = JPEG_ENC_ALLOC_OUTPUT_BUFFER};
    size_t cap = 0;
    uint8_t *buf = jpeg_alloc_encoder_mem(fb->width * fb->height, &out_cfg, &cap);
    if (!buf) {
        free(converted);
        return false;
    }
    uint32_t len = 0;
    xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
    esp_err_t ret = encode_jpeg(src, src_len, quality ? quality : 80, buf, cap, &len);
    xSemaphoreGiveRecursive(s_lock);
    free(converted);
    if (ret != ESP_OK) {
        free(buf);
        return false;
    }
    *out = buf;
    *out_len = len;
    return true;
}
