#include "../include/tof_lidar.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define TOF_SPEED_OF_LIGHT_MM_US 0.299702547f

void tof_init(tof_sensor_t *sensor, tof_model_t model)
{
    memset(sensor, 0, sizeof(*sensor));
    sensor->model = model;
    sensor->mode = TOF_MODE_SINGLE;
    sensor->profile = TOF_RANGE_LONG;
    sensor->timing_budget_us = 33000;
    sensor->inter_measurement_ms = 100;
    sensor->address = 0x29;
    sensor->initialized = 1;
}

void tof_set_mode(tof_sensor_t *sensor, tof_mode_t mode)
{
    sensor->mode = mode;
}

void tof_set_range_profile(tof_sensor_t *sensor, tof_range_profile_t profile)
{
    sensor->profile = profile;
    if (profile == TOF_RANGE_SHORT) sensor->timing_budget_us = 20000;
    else if (profile == TOF_RANGE_MEDIUM) sensor->timing_budget_us = 33000;
    else sensor->timing_budget_us = 50000;
}

void tof_set_timing_budget(tof_sensor_t *sensor, uint32_t budget_us)
{
    sensor->timing_budget_us = budget_us;
}

void tof_set_inter_measurement(tof_sensor_t *sensor, uint32_t period_ms)
{
    sensor->inter_measurement_ms = period_ms;
}

void tof_set_address(tof_sensor_t *sensor, uint8_t addr)
{
    sensor->address = addr;
}

tof_range_t tof_range_once(tof_sensor_t *sensor)
{
    tof_range_t result;
    memset(&result, 0, sizeof(result));
    (void)sensor;
    return result;
}

tof_range_t tof_range_continuous(tof_sensor_t *sensor)
{
    tof_range_t result;
    memset(&result, 0, sizeof(result));
    (void)sensor;
    return result;
}

tof_range_t tof_range_with_status(const tof_range_t *raw)
{
    tof_range_t result = *raw;
    if (result.range_status != 0) {
        result.distance_mm = -1.0f;
    }
    return result;
}

float tof_distance_filter_median(float *buffer, uint8_t len)
{
    if (len == 0) return 0.0f;
    uint8_t i, j;
    float sorted[16];
    for (i = 0; i < len && i < 16; i++) sorted[i] = buffer[i];
    len = (len < 16) ? len : 16;
    for (i = 0; i < len - 1; i++) {
        for (j = i + 1; j < len; j++) {
            if (sorted[i] > sorted[j]) {
                float tmp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = tmp;
            }
        }
    }
    return sorted[len / 2];
}

float tof_distance_filter_moving_avg(const tof_range_t *history, uint8_t count)
{
    if (count == 0) return 0.0f;
    float sum = 0.0f;
    uint8_t i;
    for (i = 0; i < count; i++) sum += history[i].distance_mm;
    return sum / (float)count;
}

void tof_multizone_init(tof_multizone_t *mz, uint8_t x, uint8_t y)
{
    memset(mz, 0, sizeof(*mz));
    mz->zones_x = x;
    mz->zones_y = y;
    mz->zone_count = x * y;
}

void tof_multizone_update(tof_multizone_t *mz, uint8_t zone_idx, float distance_mm)
{
    if (zone_idx < mz->zone_count) {
        mz->distances_mm[zone_idx] = distance_mm;
    }
}

float tof_multizone_min(const tof_multizone_t *mz)
{
    float min_val = 1e9f;
    uint8_t i;
    for (i = 0; i < mz->zone_count; i++) {
        if (mz->distances_mm[i] > 0 && mz->distances_mm[i] < min_val)
            min_val = mz->distances_mm[i];
    }
    return min_val;
}

float tof_multizone_max(const tof_multizone_t *mz)
{
    float max_val = 0.0f;
    uint8_t i;
    for (i = 0; i < mz->zone_count; i++) {
        if (mz->distances_mm[i] > max_val)
            max_val = mz->distances_mm[i];
    }
    return max_val;
}

float tof_multizone_avg(const tof_multizone_t *mz)
{
    if (mz->zone_count == 0) return 0.0f;
    float sum = 0.0f;
    int valid = 0;
    uint8_t i;
    for (i = 0; i < mz->zone_count; i++) {
        if (mz->distances_mm[i] > 0) {
            sum += mz->distances_mm[i];
            valid++;
        }
    }
    return (valid > 0) ? sum / (float)valid : 0.0f;
}

void tof_multizone_print(const tof_multizone_t *mz)
{
    uint8_t row, col;
    printf("Multi-Zone %dx%d:\n", mz->zones_x, mz->zones_y);
    for (row = 0; row < mz->zones_y; row++) {
        for (col = 0; col < mz->zones_x; col++) {
            printf("%6.0f ", mz->distances_mm[row * mz->zones_x + col]);
        }
        printf("\n");
    }
}

void ultrasonic_init(ultrasonic_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->speed_of_sound_ms = 343.0f;
    cfg->temperature_compensation_c = 20.0f;
    cfg->trigger_timeout_us = 38000.0f;
    cfg->num_samples_avg = 3;
}

void ultrasonic_set_config(ultrasonic_config_t *cfg, float sos_ms, float temp_c, uint8_t avg)
{
    cfg->speed_of_sound_ms = sos_ms;
    cfg->temperature_compensation_c = temp_c;
    cfg->num_samples_avg = avg;
}

ultrasonic_range_t ultrasonic_measure(float pulse_duration_us, float echo_time_us, uint32_t ts)
{
    ultrasonic_range_t result;
    memset(&result, 0, sizeof(result));
    result.echo_time_us = (uint32_t)echo_time_us;
    result.pulse_width_us = (uint32_t)pulse_duration_us;
    result.timestamp_ms = ts;
    result.distance_mm = ultrasonic_distance_from_echo(echo_time_us, 343.0f);
    result.valid = (echo_time_us > 116.0f && echo_time_us < 38000.0f) ? 1 : 0;
    return result;
}

float ultrasonic_distance_from_echo(float echo_time_us, float speed_of_sound_ms)
{
    return (echo_time_us * speed_of_sound_ms) / (2.0f * 1000.0f);
}

void lidar_scan_init(lidar_scan_t *scan)
{
    memset(scan, 0, sizeof(*scan));
    scan->scan_frequency_hz = 5.5f;
    scan->angular_resolution_deg = 1.0f;
}

void lidar_scan_add_point(lidar_scan_t *scan, float x, float y, float dist, float intensity, float angle)
{
    if (scan->point_count < LIDAR_MAX_POINTS) {
        lidar_point_t *p = &scan->points[scan->point_count++];
        p->x_mm = x;
        p->y_mm = y;
        p->distance_mm = dist;
        p->intensity = intensity;
        p->angle_deg = angle;
    }
}

void lidar_scan_clear(lidar_scan_t *scan)
{
    scan->point_count = 0;
}

void lidar_scan_to_cartesian(lidar_scan_t *scan, float angle_deg, float distance_mm)
{
    if (scan->point_count >= LIDAR_MAX_POINTS) return;
    float rad = angle_deg * (M_PI / 180.0f);
    (void)rad;
    lidar_scan_add_point(scan,
        distance_mm * cosf(angle_deg * (M_PI / 180.0f)),
        distance_mm * sinf(angle_deg * (M_PI / 180.0f)),
        distance_mm, 1.0f, angle_deg);
}

float lidar_scan_min_distance(const lidar_scan_t *scan)
{
    float min_val = 1e9f;
    uint16_t i;
    for (i = 0; i < scan->point_count; i++) {
        if (scan->points[i].distance_mm > 0 && scan->points[i].distance_mm < min_val)
            min_val = scan->points[i].distance_mm;
    }
    return (min_val == 1e9f) ? 0.0f : min_val;
}

float lidar_scan_max_distance(const lidar_scan_t *scan)
{
    float max_val = 0.0f;
    uint16_t i;
    for (i = 0; i < scan->point_count; i++) {
        if (scan->points[i].distance_mm > max_val)
            max_val = scan->points[i].distance_mm;
    }
    return max_val;
}

void lidar_scan_filter_outliers(lidar_scan_t *scan, float threshold_mm)
{
    uint16_t write = 0;
    uint16_t i;
    for (i = 0; i < scan->point_count; i++) {
        if (scan->points[i].distance_mm > 0.0f && scan->points[i].distance_mm < threshold_mm) {
            scan->points[write++] = scan->points[i];
        }
    }
    scan->point_count = write;
}

uint16_t lidar_scan_cluster(const lidar_scan_t *scan, float cluster_radius_mm, uint16_t *labels)
{
    uint16_t i, j, cluster_count = 0;
    for (i = 0; i < scan->point_count; i++) labels[i] = 0xFFFF;
    for (i = 0; i < scan->point_count; i++) {
        if (labels[i] != 0xFFFF) continue;
        labels[i] = cluster_count;
        for (j = i + 1; j < scan->point_count; j++) {
            if (labels[j] != 0xFFFF) continue;
            float dx = scan->points[i].x_mm - scan->points[j].x_mm;
            float dy = scan->points[i].y_mm - scan->points[j].y_mm;
            if (sqrtf(dx*dx + dy*dy) < cluster_radius_mm) labels[j] = cluster_count;
        }
        cluster_count++;
    }
    return cluster_count;
}

void optical_flow_init(optical_flow_t *flow)
{
    memset(flow, 0, sizeof(*flow));
}

void optical_flow_add_vector(optical_flow_t *flow, int32_t dx, int32_t dy, float quality)
{
    if (flow->vector_count < OPTICAL_FLOW_MAX_FEATURES) {
        flow->vectors[flow->vector_count].dx = dx;
        flow->vectors[flow->vector_count].dy = dy;
        flow->vectors[flow->vector_count].quality = quality;
        flow->vector_count++;
    }
}

void optical_flow_compute_displacement(optical_flow_t *flow, float pixels_to_mm)
{
    int32_t sum_dx = 0, sum_dy = 0;
    uint8_t i;
    for (i = 0; i < flow->vector_count; i++) {
        sum_dx += flow->vectors[i].dx;
        sum_dy += flow->vectors[i].dy;
    }
    if (flow->vector_count > 0) {
        flow->displacement_x_mm = ((float)sum_dx / flow->vector_count) * pixels_to_mm;
        flow->displacement_y_mm = ((float)sum_dy / flow->vector_count) * pixels_to_mm;
    }
}

void optical_flow_compute_velocity(optical_flow_t *flow, float dt_s)
{
    if (dt_s > 0.0f) {
        flow->velocity_x_mms = flow->displacement_x_mm / dt_s;
        flow->velocity_y_mms = flow->displacement_y_mm / dt_s;
    }
}

void optical_flow_clear(optical_flow_t *flow)
{
    flow->vector_count = 0;
    flow->displacement_x_mm = 0.0f;
    flow->displacement_y_mm = 0.0f;
}
