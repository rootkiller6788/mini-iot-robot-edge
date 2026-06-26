#ifndef CAMERA_MIPI_H
#define CAMERA_MIPI_H

#include <stdint.h>

#define CAMERA_MAX_WIDTH   1920
#define CAMERA_MAX_HEIGHT  1080
#define CAMERA_BAYER_MAX   (CAMERA_MAX_WIDTH * CAMERA_MAX_HEIGHT)
#define CAMERA_RGB_MAX     (CAMERA_MAX_WIDTH * CAMERA_MAX_HEIGHT * 3)
#define CAMERA_LANES_MAX   4
#define CAMERA_AE_HISTORY  32
#define CAMERA_AWB_SAMPLES 64

typedef enum {
    CAMERA_RES_VGA   = 0,
    CAMERA_RES_720P  = 1,
    CAMERA_RES_1080P = 2
} camera_resolution_t;

typedef enum {
    CAMERA_BAYER_RGGB = 0,
    CAMERA_BAYER_GRBG = 1,
    CAMERA_BAYER_GBRG = 2,
    CAMERA_BAYER_BGGR = 3
} camera_bayer_pattern_t;

typedef enum {
    CAMERA_FORMAT_RAW8  = 0,
    CAMERA_FORMAT_RAW10 = 1,
    CAMERA_FORMAT_RAW12 = 2,
    CAMERA_FORMAT_YUV422 = 3,
    CAMERA_FORMAT_JPEG   = 4
} camera_pixel_format_t;

typedef enum {
    CAMERA_AE_CENTER_WEIGHTED = 0,
    CAMERA_AE_SPOT            = 1,
    CAMERA_AE_MATRIX          = 2
} camera_ae_mode_t;

typedef enum {
    CAMERA_AWB_GRAY_WORLD     = 0,
    CAMERA_AWB_WHITE_PATCH    = 1,
    CAMERA_AWB_PREDEFINED     = 2
} camera_awb_mode_t;

typedef struct {
    camera_resolution_t resolution;
    camera_pixel_format_t format;
    uint16_t width;
    uint16_t height;
    uint8_t lanes;
    uint32_t lane_bitrate_mbps;
    uint8_t mipi_clk_mhz;
} camera_mipi_config_t;

typedef struct {
    uint8_t* raw_data;
    uint32_t raw_size;
    uint16_t width;
    uint16_t height;
    camera_bayer_pattern_t pattern;
    uint8_t bit_depth;
} camera_raw_frame_t;

typedef struct {
    uint8_t* rgb_data;
    uint16_t width;
    uint16_t height;
    uint32_t stride;
} camera_rgb_frame_t;

typedef struct {
    camera_ae_mode_t mode;
    float target_brightness;
    float current_brightness;
    uint32_t exposure_time_us;
    float analog_gain;
    float digital_gain;
    uint32_t min_exposure_us;
    uint32_t max_exposure_us;
    float min_gain;
    float max_gain;
    float convergence_speed;
    float history[CAMERA_AE_HISTORY];
    uint8_t history_idx;
} camera_ae_t;

typedef struct {
    camera_awb_mode_t mode;
    float r_gain;
    float g_gain;
    float b_gain;
    float kr, kg, kb;
    uint16_t predefined_temp_k;
    float convergence_speed;
} camera_awb_t;

typedef struct {
    float ccm[9];
    float gamma_lut[256];
    uint8_t brightness;
    float contrast;
    float saturation;
    float sharpness;
    uint8_t denoise_level;
} camera_isp_config_t;

typedef struct {
    camera_mipi_config_t mipi;
    camera_raw_frame_t raw;
    camera_rgb_frame_t rgb;
    camera_ae_t ae;
    camera_awb_t awb;
    camera_isp_config_t isp;
    camera_bayer_pattern_t bayer;
    float frame_rate_fps;
    uint32_t frame_interval_us;
    uint32_t last_frame_us;
    uint8_t streaming;
    uint8_t jpeg_enabled;
    uint8_t jpeg_quality;
} camera_sensor_t;

void camera_mipi_init(camera_sensor_t *cam, camera_resolution_t res, camera_pixel_format_t fmt, uint8_t lanes);
void camera_mipi_config(camera_sensor_t *cam, uint32_t bitrate_mbps, uint8_t clk_mhz);
void camera_mipi_start_stream(camera_sensor_t *cam);
void camera_mipi_stop_stream(camera_sensor_t *cam);
void camera_mipi_set_frame_rate(camera_sensor_t *cam, float fps);
void camera_mipi_set_resolution(camera_sensor_t *cam, camera_resolution_t res);

void camera_bayer_set_pattern(camera_sensor_t *cam, camera_bayer_pattern_t pattern);
void camera_bayer_demosaic_simple(const camera_raw_frame_t *raw, camera_rgb_frame_t *rgb);
void camera_bayer_demosaic_bilinear(const camera_raw_frame_t *raw, camera_rgb_frame_t *rgb);
void camera_bayer_demosaic_vng(const camera_raw_frame_t *raw, camera_rgb_frame_t *rgb);
uint8_t camera_get_pixel(const camera_raw_frame_t *raw, uint16_t x, uint16_t y);
void camera_set_pixel_rgb(camera_rgb_frame_t *rgb, uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b);

void camera_ae_init(camera_ae_t *ae, camera_ae_mode_t mode, float target);
void camera_ae_set_limits(camera_ae_t *ae, uint32_t min_exp, uint32_t max_exp, float min_gain, float max_gain);
void camera_ae_update(camera_ae_t *ae, float measured_brightness);
void camera_ae_set_convergence(camera_ae_t *ae, float speed);
float camera_ae_compute_brightness(const uint8_t* rgb, uint16_t w, uint16_t h, camera_ae_mode_t mode);
void camera_ae_reset(camera_ae_t *ae);

void camera_awb_init(camera_awb_t *awb, camera_awb_mode_t mode);
void camera_awb_set_predefined(camera_awb_t *awb, uint16_t temp_k);
void camera_awb_gray_world(camera_awb_t *awb, const uint8_t* rgb, uint16_t w, uint16_t h);
void camera_awb_white_patch(camera_awb_t *awb, const uint8_t* rgb, uint16_t w, uint16_t h);
void camera_awb_apply(const camera_awb_t *awb, uint8_t* rgb, uint16_t w, uint16_t h);
void camera_awb_set_convergence(camera_awb_t *awb, float speed);
void camera_awb_temperature_to_gains(uint16_t temp_k, float *rg, float *bg);

void camera_isp_init(camera_isp_config_t *isp);
void camera_isp_color_correct(const camera_isp_config_t *isp, uint8_t* rgb, uint16_t w, uint16_t h);
void camera_isp_apply_gamma(const camera_isp_config_t *isp, uint8_t* rgb, uint16_t w, uint16_t h);
void camera_isp_set_brightness(camera_isp_config_t *isp, uint8_t level);
void camera_isp_set_contrast(camera_isp_config_t *isp, float value);
void camera_isp_set_saturation(camera_isp_config_t *isp, float value);
void camera_isp_pipeline(camera_sensor_t *cam);

void camera_jpeg_set_quality(camera_sensor_t *cam, uint8_t q);
void camera_jpeg_enable(camera_sensor_t *cam, uint8_t enabled);
uint32_t camera_jpeg_compress_placeholder(const camera_rgb_frame_t *rgb, uint8_t *out, uint32_t max_out);

#endif
