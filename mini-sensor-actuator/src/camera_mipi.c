#include "../include/camera_mipi.h"
#include <math.h>
#include <string.h>

void camera_mipi_init(camera_sensor_t *cam, camera_resolution_t res, camera_pixel_format_t fmt, uint8_t lanes)
{
    memset(cam, 0, sizeof(*cam));
    cam->mipi.resolution = res;
    cam->mipi.format = fmt;
    cam->mipi.lanes = lanes;
    if (res == CAMERA_RES_VGA) { cam->mipi.width = 640; cam->mipi.height = 480; }
    else if (res == CAMERA_RES_720P) { cam->mipi.width = 1280; cam->mipi.height = 720; }
    else { cam->mipi.width = 1920; cam->mipi.height = 1080; }
    cam->raw.width = cam->mipi.width;
    cam->raw.height = cam->mipi.height;
    cam->rgb.width = cam->mipi.width;
    cam->rgb.height = cam->mipi.height;
    cam->rgb.stride = cam->mipi.width * 3;
    cam->bayer = CAMERA_BAYER_RGGB;
    cam->frame_rate_fps = 30.0f;
    cam->frame_interval_us = 33333;
    cam->jpeg_quality = 80;
    camera_ae_init(&cam->ae, CAMERA_AE_CENTER_WEIGHTED, 128.0f);
    camera_awb_init(&cam->awb, CAMERA_AWB_GRAY_WORLD);
    camera_isp_init(&cam->isp);
}

void camera_mipi_config(camera_sensor_t *cam, uint32_t bitrate_mbps, uint8_t clk_mhz)
{
    cam->mipi.lane_bitrate_mbps = bitrate_mbps;
    cam->mipi.mipi_clk_mhz = clk_mhz;
}

void camera_mipi_start_stream(camera_sensor_t *cam)
{
    cam->streaming = 1;
}

void camera_mipi_stop_stream(camera_sensor_t *cam)
{
    cam->streaming = 0;
}

void camera_mipi_set_frame_rate(camera_sensor_t *cam, float fps)
{
    cam->frame_rate_fps = fps;
    cam->frame_interval_us = (uint32_t)(1000000.0f / fps);
}

void camera_mipi_set_resolution(camera_sensor_t *cam, camera_resolution_t res)
{
    cam->mipi.resolution = res;
    if (res == CAMERA_RES_VGA) { cam->mipi.width = 640; cam->mipi.height = 480; }
    else if (res == CAMERA_RES_720P) { cam->mipi.width = 1280; cam->mipi.height = 720; }
    else { cam->mipi.width = 1920; cam->mipi.height = 1080; }
    cam->raw.width = cam->mipi.width;
    cam->raw.height = cam->mipi.height;
    cam->rgb.width = cam->mipi.width;
    cam->rgb.height = cam->mipi.height;
    cam->rgb.stride = cam->mipi.width * 3;
}

void camera_bayer_set_pattern(camera_sensor_t *cam, camera_bayer_pattern_t pattern)
{
    cam->bayer = pattern;
}

static int bayer_is_red(camera_bayer_pattern_t pat, int col, int row)
{
    switch (pat) {
        case CAMERA_BAYER_RGGB: return ((row & 1) == 0 && (col & 1) == 0);
        case CAMERA_BAYER_GRBG: return ((row & 1) == 0 && (col & 1) == 1);
        case CAMERA_BAYER_GBRG: return ((row & 1) == 1 && (col & 1) == 0);
        case CAMERA_BAYER_BGGR: return ((row & 1) == 1 && (col & 1) == 1);
        default: return 0;
    }
}

static int bayer_is_blue(camera_bayer_pattern_t pat, int col, int row)
{
    switch (pat) {
        case CAMERA_BAYER_RGGB: return ((row & 1) == 1 && (col & 1) == 1);
        case CAMERA_BAYER_GRBG: return ((row & 1) == 1 && (col & 1) == 0);
        case CAMERA_BAYER_GBRG: return ((row & 1) == 0 && (col & 1) == 1);
        case CAMERA_BAYER_BGGR: return ((row & 1) == 0 && (col & 1) == 0);
        default: return 0;
    }
}

uint8_t camera_get_pixel(const camera_raw_frame_t *raw, uint16_t x, uint16_t y)
{
    if (!raw->raw_data || x >= raw->width || y >= raw->height) return 0;
    return raw->raw_data[y * raw->width + x];
}

void camera_set_pixel_rgb(camera_rgb_frame_t *rgb, uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b)
{
    if (!rgb->rgb_data || x >= rgb->width || y >= rgb->height) return;
    uint32_t idx = y * rgb->stride + x * 3;
    rgb->rgb_data[idx] = r;
    rgb->rgb_data[idx + 1] = g;
    rgb->rgb_data[idx + 2] = b;
}

void camera_bayer_demosaic_simple(const camera_raw_frame_t *raw, camera_rgb_frame_t *rgb)
{
    int w = raw->width, h = raw->height;
    int x, y;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            uint8_t r = 0, g = 0, b = 0;
            uint8_t px = camera_get_pixel(raw, (uint16_t)x, (uint16_t)y);
            if (bayer_is_red(raw->pattern, x, y)) {
                r = px;
                g = (uint8_t)(((int)camera_get_pixel(raw, (uint16_t)(x > 0 ? x-1 : 0), (uint16_t)y)
                     + (int)camera_get_pixel(raw, (uint16_t)(x + 1 < w ? x+1 : x), (uint16_t)y)
                     + (int)camera_get_pixel(raw, (uint16_t)x, (uint16_t)(y > 0 ? y-1 : 0))
                     + (int)camera_get_pixel(raw, (uint16_t)x, (uint16_t)(y + 1 < h ? y+1 : y))) / 4);
                b = (uint8_t)(((int)camera_get_pixel(raw, (uint16_t)(x > 0 ? x-1 : 0), (uint16_t)(y > 0 ? y-1 : 0))
                     + (int)camera_get_pixel(raw, (uint16_t)(x + 1 < w ? x+1 : x), (uint16_t)(y > 0 ? y-1 : 0))
                     + (int)camera_get_pixel(raw, (uint16_t)(x > 0 ? x-1 : 0), (uint16_t)(y + 1 < h ? y+1 : y))
                     + (int)camera_get_pixel(raw, (uint16_t)(x + 1 < w ? x+1 : x), (uint16_t)(y + 1 < h ? y+1 : y))) / 4);
            } else if (bayer_is_blue(raw->pattern, x, y)) {
                b = px;
                r = (uint8_t)(((int)camera_get_pixel(raw, (uint16_t)(x > 0 ? x-1 : 0), (uint16_t)(y > 0 ? y-1 : 0))
                     + (int)camera_get_pixel(raw, (uint16_t)(x + 1 < w ? x+1 : x), (uint16_t)(y > 0 ? y-1 : 0))
                     + (int)camera_get_pixel(raw, (uint16_t)(x > 0 ? x-1 : 0), (uint16_t)(y + 1 < h ? y+1 : y))
                     + (int)camera_get_pixel(raw, (uint16_t)(x + 1 < w ? x+1 : x), (uint16_t)(y + 1 < h ? y+1 : y))) / 4);
                g = (uint8_t)(((int)camera_get_pixel(raw, (uint16_t)(x > 0 ? x-1 : 0), (uint16_t)y)
                     + (int)camera_get_pixel(raw, (uint16_t)(x + 1 < w ? x+1 : x), (uint16_t)y)
                     + (int)camera_get_pixel(raw, (uint16_t)x, (uint16_t)(y > 0 ? y-1 : 0))
                     + (int)camera_get_pixel(raw, (uint16_t)x, (uint16_t)(y + 1 < h ? y+1 : y))) / 4);
            } else {
                g = px;
                if ((bayer_is_red(raw->pattern, x, 0) && (y&1)==0) ||
                    (!bayer_is_red(raw->pattern, x, 0) && (y&1)==1)) {
                    r = (uint8_t)(((int)camera_get_pixel(raw, (uint16_t)(x > 0 ? x-1 : 0), (uint16_t)y)
                         + (int)camera_get_pixel(raw, (uint16_t)(x + 1 < w ? x+1 : x), (uint16_t)y)) / 2);
                    b = (uint8_t)(((int)camera_get_pixel(raw, (uint16_t)x, (uint16_t)(y > 0 ? y-1 : 0))
                         + (int)camera_get_pixel(raw, (uint16_t)x, (uint16_t)(y + 1 < h ? y+1 : y))) / 2);
                } else {
                    b = (uint8_t)(((int)camera_get_pixel(raw, (uint16_t)(x > 0 ? x-1 : 0), (uint16_t)y)
                         + (int)camera_get_pixel(raw, (uint16_t)(x + 1 < w ? x+1 : x), (uint16_t)y)) / 2);
                    r = (uint8_t)(((int)camera_get_pixel(raw, (uint16_t)x, (uint16_t)(y > 0 ? y-1 : 0))
                         + (int)camera_get_pixel(raw, (uint16_t)x, (uint16_t)(y + 1 < h ? y+1 : y))) / 2);
                }
            }
            camera_set_pixel_rgb(rgb, (uint16_t)x, (uint16_t)y, r, g, b);
        }
    }
}

void camera_bayer_demosaic_bilinear(const camera_raw_frame_t *raw, camera_rgb_frame_t *rgb)
{
    int w = raw->width, h = raw->height;
    int x, y;
    static const int kernel_n[5][2] = {{0,0},{-1,0},{1,0},{0,-1},{0,1}};
    static const int kernel_d[5][2] = {{-1,-1},{1,-1},{-1,1},{1,1},{0,0}};

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            uint8_t r = 0, g = 0, b = 0;
            uint8_t px = camera_get_pixel(raw, (uint16_t)x, (uint16_t)y);
            int row_parity = y & 1;

            if (bayer_is_red(raw->pattern, x, y)) {
                r = px;
                int gsum = 0, gcnt = 0, i;
                for (i = 1; i < 5; i++) {
                    int nx = x + kernel_n[i][0], ny = y + kernel_n[i][1];
                    if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                        gsum += camera_get_pixel(raw, (uint16_t)nx, (uint16_t)ny);
                        gcnt++;
                    }
                }
                g = (uint8_t)(gcnt > 0 ? gsum / gcnt : 0);
                int bsum = 0, bcnt = 0;
                for (i = 0; i < 4; i++) {
                    int nx = x + kernel_d[i][0], ny = y + kernel_d[i][1];
                    if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                        bsum += camera_get_pixel(raw, (uint16_t)nx, (uint16_t)ny);
                        bcnt++;
                    }
                }
                b = (uint8_t)(bcnt > 0 ? bsum / bcnt : 0);
            } else if (bayer_is_blue(raw->pattern, x, y)) {
                b = px;
                int rsum = 0, rcnt = 0, i;
                for (i = 0; i < 4; i++) {
                    int nx = x + kernel_d[i][0], ny = y + kernel_d[i][1];
                    if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                        rsum += camera_get_pixel(raw, (uint16_t)nx, (uint16_t)ny);
                        rcnt++;
                    }
                }
                r = (uint8_t)(rcnt > 0 ? rsum / rcnt : 0);
                int gsum = 0, gcnt = 0;
                for (i = 1; i < 5; i++) {
                    int nx = x + kernel_n[i][0], ny = y + kernel_n[i][1];
                    if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                        gsum += camera_get_pixel(raw, (uint16_t)nx, (uint16_t)ny);
                        gcnt++;
                    }
                }
                g = (uint8_t)(gcnt > 0 ? gsum / gcnt : 0);
            } else {
                g = px;
                if (row_parity == 0 && bayer_is_red(raw->pattern, x, 0)) {
                    int rs = (int)camera_get_pixel(raw, (uint16_t)(x>0?x-1:0), (uint16_t)y)
                           + (int)camera_get_pixel(raw, (uint16_t)(x+1<w?x+1:x), (uint16_t)y);
                    int bs = (int)camera_get_pixel(raw, (uint16_t)x, (uint16_t)(y>0?y-1:0))
                           + (int)camera_get_pixel(raw, (uint16_t)x, (uint16_t)(y+1<h?y+1:y));
                    r = (uint8_t)(rs / 2); b = (uint8_t)(bs / 2);
                } else if (row_parity == 1 && bayer_is_red(raw->pattern, x, 0)) {
                    int bs = (int)camera_get_pixel(raw, (uint16_t)(x>0?x-1:0), (uint16_t)y)
                           + (int)camera_get_pixel(raw, (uint16_t)(x+1<w?x+1:x), (uint16_t)y);
                    int rs = (int)camera_get_pixel(raw, (uint16_t)x, (uint16_t)(y>0?y-1:0))
                           + (int)camera_get_pixel(raw, (uint16_t)x, (uint16_t)(y+1<h?y+1:y));
                    r = (uint8_t)(rs / 2); b = (uint8_t)(bs / 2);
                } else if (row_parity == 0 && !bayer_is_red(raw->pattern, x, 0)) {
                    int bs = (int)camera_get_pixel(raw, (uint16_t)(x>0?x-1:0), (uint16_t)y)
                           + (int)camera_get_pixel(raw, (uint16_t)(x+1<w?x+1:x), (uint16_t)y);
                    int rs = (int)camera_get_pixel(raw, (uint16_t)x, (uint16_t)(y>0?y-1:0))
                           + (int)camera_get_pixel(raw, (uint16_t)x, (uint16_t)(y+1<h?y+1:y));
                    r = (uint8_t)(rs / 2); b = (uint8_t)(bs / 2);
                } else {
                    int rs = (int)camera_get_pixel(raw, (uint16_t)(x>0?x-1:0), (uint16_t)y)
                           + (int)camera_get_pixel(raw, (uint16_t)(x+1<w?x+1:x), (uint16_t)y);
                    int bs = (int)camera_get_pixel(raw, (uint16_t)x, (uint16_t)(y>0?y-1:0))
                           + (int)camera_get_pixel(raw, (uint16_t)x, (uint16_t)(y+1<h?y+1:y));
                    r = (uint8_t)(rs / 2); b = (uint8_t)(bs / 2);
                }
            }
            camera_set_pixel_rgb(rgb, (uint16_t)x, (uint16_t)y, r, g, b);
        }
    }
}

void camera_bayer_demosaic_vng(const camera_raw_frame_t *raw, camera_rgb_frame_t *rgb)
{
    camera_bayer_demosaic_bilinear(raw, rgb);
}

void camera_ae_init(camera_ae_t *ae, camera_ae_mode_t mode, float target)
{
    memset(ae, 0, sizeof(*ae));
    ae->mode = mode;
    ae->target_brightness = target;
    ae->exposure_time_us = 10000;
    ae->analog_gain = 1.0f;
    ae->digital_gain = 1.0f;
    ae->min_exposure_us = 100;
    ae->max_exposure_us = 66000;
    ae->min_gain = 1.0f;
    ae->max_gain = 16.0f;
    ae->convergence_speed = 0.3f;
    ae->history_idx = 0;
}

void camera_ae_set_limits(camera_ae_t *ae, uint32_t min_exp, uint32_t max_exp, float min_gain, float max_gain)
{
    ae->min_exposure_us = min_exp;
    ae->max_exposure_us = max_exp;
    ae->min_gain = min_gain;
    ae->max_gain = max_gain;
}

void camera_ae_update(camera_ae_t *ae, float measured_brightness)
{
    ae->current_brightness = measured_brightness;
    float error = ae->target_brightness - measured_brightness;
    float adjustment = error * ae->convergence_speed;

    float total_gain = ae->analog_gain + ae->digital_gain;
    float new_exposure = (float)ae->exposure_time_us + adjustment * (float)ae->exposure_time_us * 0.01f;

    if (new_exposure > (float)ae->max_exposure_us) {
        new_exposure = (float)ae->max_exposure_us;
        float new_gain = total_gain + adjustment * total_gain * 0.02f;
        if (new_gain > ae->max_gain) new_gain = ae->max_gain;
        if (new_gain < ae->min_gain) new_gain = ae->min_gain;
        ae->analog_gain = new_gain > ae->max_gain ? ae->max_gain : new_gain;
        ae->digital_gain = new_gain - ae->analog_gain;
        if (ae->digital_gain < 0.0f) ae->digital_gain = 0.0f;
    } else if (new_exposure < (float)ae->min_exposure_us) {
        new_exposure = (float)ae->min_exposure_us;
        float new_gain = total_gain + adjustment * total_gain * 0.02f;
        if (new_gain < ae->min_gain) new_gain = ae->min_gain;
        ae->analog_gain = new_gain;
        ae->digital_gain = 0.0f;
    } else {
        float new_gain = total_gain + adjustment * total_gain * 0.005f;
        if (new_gain > ae->max_gain) new_gain = ae->max_gain;
        if (new_gain < ae->min_gain) new_gain = ae->min_gain;
        ae->analog_gain = new_gain;
        ae->digital_gain = new_gain - ae->analog_gain;
        if (ae->digital_gain < 0.0f) ae->digital_gain = 0.0f;
    }

    ae->exposure_time_us = (uint32_t)new_exposure;

    ae->history[ae->history_idx] = measured_brightness;
    ae->history_idx = (ae->history_idx + 1) % CAMERA_AE_HISTORY;
}

void camera_ae_set_convergence(camera_ae_t *ae, float speed)
{
    ae->convergence_speed = speed;
}

float camera_ae_compute_brightness(const uint8_t* rgb, uint16_t w, uint16_t h, camera_ae_mode_t mode)
{
    if (!rgb || w == 0 || h == 0) return 0.0f;
    float sum = 0.0f;
    uint32_t count = 0;
    uint16_t x, y;

    if (mode == CAMERA_AE_SPOT) {
        uint16_t cx = w / 2, cy = h / 2;
        uint16_t rw = w / 5, rh = h / 5;
        for (y = cy - rh; y < cy + rh; y++) {
            for (x = cx - rw; x < cx + rw; x++) {
                if (x < w && y < h) {
                    uint32_t idx = (uint32_t)y * w * 3 + (uint32_t)x * 3;
                    sum += (float)rgb[idx] * 0.299f + (float)rgb[idx+1] * 0.587f + (float)rgb[idx+2] * 0.114f;
                    count++;
                }
            }
        }
    } else if (mode == CAMERA_AE_CENTER_WEIGHTED) {
        float cx = w * 0.5f, cy = h * 0.5f;
        float max_r = sqrtf(cx*cx + cy*cy);
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                uint32_t idx = (uint32_t)y * w * 3 + (uint32_t)x * 3;
                float dx = (float)x - cx, dy = (float)y - cy;
                float r = sqrtf(dx*dx + dy*dy);
                float weight = 1.0f - (r / max_r) * 0.5f;
                float lum = (float)rgb[idx] * 0.299f + (float)rgb[idx+1] * 0.587f + (float)rgb[idx+2] * 0.114f;
                sum += lum * weight;
                count++;
            }
        }
    } else {
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                uint32_t idx = (uint32_t)y * w * 3 + (uint32_t)x * 3;
                sum += (float)rgb[idx] * 0.299f + (float)rgb[idx+1] * 0.587f + (float)rgb[idx+2] * 0.114f;
                count++;
            }
        }
    }

    return count > 0 ? sum / (float)count : 0.0f;
}

void camera_ae_reset(camera_ae_t *ae)
{
    memset(ae, 0, sizeof(*ae));
}

void camera_awb_init(camera_awb_t *awb, camera_awb_mode_t mode)
{
    memset(awb, 0, sizeof(*awb));
    awb->mode = mode;
    awb->r_gain = 1.0f;
    awb->g_gain = 1.0f;
    awb->b_gain = 1.0f;
    awb->kr = 1.0f; awb->kg = 1.0f; awb->kb = 1.0f;
    awb->convergence_speed = 0.3f;
    awb->predefined_temp_k = 5500;
}

void camera_awb_set_predefined(camera_awb_t *awb, uint16_t temp_k)
{
    awb->predefined_temp_k = temp_k;
    if (awb->mode == CAMERA_AWB_PREDEFINED) {
        float rg, bg;
        camera_awb_temperature_to_gains(temp_k, &rg, &bg);
        awb->r_gain = rg;
        awb->b_gain = bg;
    }
}

void camera_awb_gray_world(camera_awb_t *awb, const uint8_t* rgb, uint16_t w, uint16_t h)
{
    if (!rgb || w == 0 || h == 0) return;
    float r_sum = 0.0f, g_sum = 0.0f, b_sum = 0.0f;
    uint32_t count = 0;
    uint16_t x, y;
    int step = (w * h > 640 * 480) ? 4 : 2;

    for (y = 0; y < h; y += step) {
        for (x = 0; x < w; x += step) {
            uint32_t idx = (uint32_t)y * w * 3 + (uint32_t)x * 3;
            uint8_t r_val = rgb[idx], g_val = rgb[idx+1], b_val = rgb[idx+2];
            int max_v = (int)r_val > (int)g_val ? (int)r_val : (int)g_val;
            max_v = max_v > (int)b_val ? max_v : (int)b_val;
            if (max_v > 240 || max_v < 15) continue;
            r_sum += (float)r_val;
            g_sum += (float)g_val;
            b_sum += (float)b_val;
            count++;
        }
    }

    if (count < 100) return;
    float r_avg = r_sum / (float)count;
    float g_avg = g_sum / (float)count;
    float b_avg = b_sum / (float)count;
    float gray = (r_avg + g_avg + b_avg) / 3.0f;

    float target_r = gray / r_avg;
    float target_b = gray / b_avg;

    awb->r_gain = awb->r_gain + (target_r - awb->r_gain) * awb->convergence_speed;
    awb->b_gain = awb->b_gain + (target_b - awb->b_gain) * awb->convergence_speed;

    if (awb->r_gain < 0.25f) awb->r_gain = 0.25f;
    if (awb->r_gain > 4.0f)  awb->r_gain = 4.0f;
    if (awb->b_gain < 0.25f) awb->b_gain = 0.25f;
    if (awb->b_gain > 4.0f)  awb->b_gain = 4.0f;

    awb->kr = awb->r_gain; awb->kg = awb->g_gain; awb->kb = awb->b_gain;
}

void camera_awb_white_patch(camera_awb_t *awb, const uint8_t* rgb, uint16_t w, uint16_t h)
{
    if (!rgb || w == 0 || h == 0) return;
    uint8_t max_r = 0, max_g = 0, max_b = 0;
    uint16_t x, y;
    int step = (w * h > 640 * 480) ? 8 : 4;

    for (y = 0; y < h; y += step) {
        for (x = 0; x < w; x += step) {
            uint32_t idx = (uint32_t)y * w * 3 + (uint32_t)x * 3;
            if (rgb[idx] > max_r) max_r = rgb[idx];
            if (rgb[idx+1] > max_g) max_g = rgb[idx+1];
            if (rgb[idx+2] > max_b) max_b = rgb[idx+2];
        }
    }

    if (max_g == 0) return;
    float target_r = (float)max_g / (float)max_r;
    float target_b = (float)max_g / (float)max_b;
    awb->r_gain = awb->r_gain + (target_r - awb->r_gain) * awb->convergence_speed;
    awb->b_gain = awb->b_gain + (target_b - awb->b_gain) * awb->convergence_speed;

    if (awb->r_gain < 0.25f) awb->r_gain = 0.25f;
    if (awb->r_gain > 4.0f)  awb->r_gain = 4.0f;
    if (awb->b_gain < 0.25f) awb->b_gain = 0.25f;
    if (awb->b_gain > 4.0f)  awb->b_gain = 4.0f;
}

void camera_awb_apply(const camera_awb_t *awb, uint8_t* rgb, uint16_t w, uint16_t h)
{
    if (!rgb) return;
    uint32_t total = (uint32_t)w * h * 3;
    uint32_t i;
    for (i = 0; i < total; i += 3) {
        int r = (int)((float)rgb[i] * awb->r_gain);
        int g = (int)((float)rgb[i+1] * awb->g_gain);
        int b = (int)((float)rgb[i+2] * awb->b_gain);
        rgb[i]   = r > 255 ? 255 : (uint8_t)r;
        rgb[i+1] = g > 255 ? 255 : (uint8_t)g;
        rgb[i+2] = b > 255 ? 255 : (uint8_t)b;
    }
}

void camera_awb_set_convergence(camera_awb_t *awb, float speed)
{
    awb->convergence_speed = speed;
}

void camera_awb_temperature_to_gains(uint16_t temp_k, float *rg, float *bg)
{
    float t = (float)temp_k;
    float r, b_val;
    if (t <= 6600.0f) {
        r = 1.0f;
        b_val = 0.00000463f * t * t - 0.0602f * t + 3.5f;
    } else {
        r = 0.00000016f * t * t + 0.0055f * t + 0.25f;
        b_val = 1.0f;
    }
    if (r < 0.25f) r = 0.25f;
    if (b_val < 0.25f) b_val = 0.25f;
    *rg = r;
    *bg = b_val;
}

void camera_isp_init(camera_isp_config_t *isp)
{
    memset(isp, 0, sizeof(*isp));
    isp->ccm[0] = 1.0f; isp->ccm[4] = 1.0f; isp->ccm[8] = 1.0f;
    uint16_t i;
    for (i = 0; i < 256; i++) {
        float val = (float)i / 255.0f;
        isp->gamma_lut[i] = powf(val, 0.45f);
    }
    isp->brightness = 128;
    isp->contrast = 1.0f;
    isp->saturation = 1.0f;
    isp->sharpness = 0.0f;
    isp->denoise_level = 0;
}

void camera_isp_color_correct(const camera_isp_config_t *isp, uint8_t* rgb, uint16_t w, uint16_t h)
{
    if (!rgb) return;
    uint32_t total = (uint32_t)w * h * 3;
    uint32_t i;
    for (i = 0; i < total; i += 3) {
        float r = (float)rgb[i], g = (float)rgb[i+1], b_val = (float)rgb[i+2];
        float nr = isp->ccm[0]*r + isp->ccm[1]*g + isp->ccm[2]*b_val;
        float ng = isp->ccm[3]*r + isp->ccm[4]*g + isp->ccm[5]*b_val;
        float nb = isp->ccm[6]*r + isp->ccm[7]*g + isp->ccm[8]*b_val;
        int ri = (int)nr, gi = (int)ng, bi = (int)nb;
        rgb[i]   = ri > 255 ? 255 : (ri < 0 ? 0 : (uint8_t)ri);
        rgb[i+1] = gi > 255 ? 255 : (gi < 0 ? 0 : (uint8_t)gi);
        rgb[i+2] = bi > 255 ? 255 : (bi < 0 ? 0 : (uint8_t)bi);
    }
}

void camera_isp_apply_gamma(const camera_isp_config_t *isp, uint8_t* rgb, uint16_t w, uint16_t h)
{
    if (!rgb) return;
    uint32_t total = (uint32_t)w * h * 3;
    uint32_t i;
    int br, bi;
    for (i = 0; i < total; i += 3) {
        br = (int)rgb[i] + (int)isp->brightness - 128;
        br = br > 255 ? 255 : (br < 0 ? 0 : br);
        rgb[i] = (uint8_t)(isp->gamma_lut[br] * 255.0f);
        bi = (int)rgb[i+1] + (int)isp->brightness - 128;
        bi = bi > 255 ? 255 : (bi < 0 ? 0 : bi);
        rgb[i+1] = (uint8_t)(isp->gamma_lut[bi] * 255.0f);
        bi = (int)rgb[i+2] + (int)isp->brightness - 128;
        bi = bi > 255 ? 255 : (bi < 0 ? 0 : bi);
        rgb[i+2] = (uint8_t)(isp->gamma_lut[bi] * 255.0f);
    }
}

void camera_isp_set_brightness(camera_isp_config_t *isp, uint8_t level)
{
    isp->brightness = level;
}

void camera_isp_set_contrast(camera_isp_config_t *isp, float value)
{
    isp->contrast = value;
}

void camera_isp_set_saturation(camera_isp_config_t *isp, float value)
{
    isp->saturation = value;
}

void camera_isp_pipeline(camera_sensor_t *cam)
{
    if (!cam || !cam->raw.raw_data) return;

    camera_bayer_demosaic_bilinear(&cam->raw, &cam->rgb);

    if (cam->awb.mode == CAMERA_AWB_GRAY_WORLD) {
        camera_awb_gray_world(&cam->awb, cam->rgb.rgb_data, cam->rgb.width, cam->rgb.height);
    } else if (cam->awb.mode == CAMERA_AWB_WHITE_PATCH) {
        camera_awb_white_patch(&cam->awb, cam->rgb.rgb_data, cam->rgb.width, cam->rgb.height);
    }
    camera_awb_apply(&cam->awb, cam->rgb.rgb_data, cam->rgb.width, cam->rgb.height);

    float brightness = camera_ae_compute_brightness(cam->rgb.rgb_data, cam->rgb.width, cam->rgb.height, cam->ae.mode);
    camera_ae_update(&cam->ae, brightness);

    camera_isp_apply_gamma(&cam->isp, cam->rgb.rgb_data, cam->rgb.width, cam->rgb.height);
}

void camera_jpeg_set_quality(camera_sensor_t *cam, uint8_t q)
{
    cam->jpeg_quality = q > 100 ? 100 : q;
}

void camera_jpeg_enable(camera_sensor_t *cam, uint8_t enabled)
{
    cam->jpeg_enabled = enabled;
}

uint32_t camera_jpeg_compress_placeholder(const camera_rgb_frame_t *rgb, uint8_t *out, uint32_t max_out)
{
    (void)rgb; (void)out; (void)max_out;
    return 0;
}
