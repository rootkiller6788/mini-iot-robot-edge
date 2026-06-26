#ifndef TOF_LIDAR_H
#define TOF_LIDAR_H

#include <stdint.h>

#define TOF_MAX_ZONES_4X4 16
#define TOF_MAX_ZONES_8X8 64
#define LIDAR_MAX_POINTS 2048
#define ULTRASONIC_MAX_RANGE_MM 4000
#define ULTRASONIC_MIN_RANGE_MM 20
#define OPTICAL_FLOW_MAX_FEATURES 64

typedef enum {
    TOF_VL53L0X = 0,
    TOF_VL53L1X = 1
} tof_model_t;

typedef enum {
    TOF_MODE_SINGLE   = 0,
    TOF_MODE_CONTINUOUS = 1
} tof_mode_t;

typedef enum {
    TOF_RANGE_SHORT  = 0,
    TOF_RANGE_MEDIUM = 1,
    TOF_RANGE_LONG   = 2
} tof_range_profile_t;

typedef struct {
    float distance_mm;
    float signal_rate_mcps;
    float ambient_rate_mcps;
    float effective_spad_count;
    uint8_t range_status;
    uint32_t timestamp_ms;
} tof_range_t;

typedef struct {
    uint8_t zones_x;
    uint8_t zones_y;
    float distances_mm[TOF_MAX_ZONES_8X8];
    uint8_t zone_count;
} tof_multizone_t;

typedef struct {
    tof_model_t model;
    tof_mode_t mode;
    tof_range_profile_t profile;
    uint32_t timing_budget_us;
    uint32_t inter_measurement_ms;
    uint8_t address;
    uint8_t initialized;
} tof_sensor_t;

typedef struct {
    float distance_mm;
    uint32_t pulse_width_us;
    uint32_t echo_time_us;
    uint32_t timestamp_ms;
    uint8_t valid;
} ultrasonic_range_t;

typedef struct {
    float trigger_timeout_us;
    float speed_of_sound_ms;
    float temperature_compensation_c;
    uint8_t num_samples_avg;
} ultrasonic_config_t;

typedef struct {
    float x_mm;
    float y_mm;
    float distance_mm;
    float intensity;
    float angle_deg;
} lidar_point_t;

typedef struct {
    lidar_point_t points[LIDAR_MAX_POINTS];
    uint16_t point_count;
    float scan_frequency_hz;
    float angular_resolution_deg;
    float start_angle_deg;
    float end_angle_deg;
    uint32_t timestamp_ms;
} lidar_scan_t;

typedef struct {
    int32_t dx;
    int32_t dy;
    float quality;
} optical_flow_vector_t;

typedef struct {
    optical_flow_vector_t vectors[OPTICAL_FLOW_MAX_FEATURES];
    uint8_t vector_count;
    float displacement_x_mm;
    float displacement_y_mm;
    float velocity_x_mms;
    float velocity_y_mms;
    uint32_t timestamp_ms;
} optical_flow_t;

void tof_init(tof_sensor_t *sensor, tof_model_t model);
void tof_set_mode(tof_sensor_t *sensor, tof_mode_t mode);
void tof_set_range_profile(tof_sensor_t *sensor, tof_range_profile_t profile);
void tof_set_timing_budget(tof_sensor_t *sensor, uint32_t budget_us);
void tof_set_inter_measurement(tof_sensor_t *sensor, uint32_t period_ms);
void tof_set_address(tof_sensor_t *sensor, uint8_t addr);
tof_range_t tof_range_once(tof_sensor_t *sensor);
tof_range_t tof_range_continuous(tof_sensor_t *sensor);
tof_range_t tof_range_with_status(const tof_range_t *raw);
float tof_distance_filter_median(float *buffer, uint8_t len);
float tof_distance_filter_moving_avg(const tof_range_t *history, uint8_t count);

void tof_multizone_init(tof_multizone_t *mz, uint8_t x, uint8_t y);
void tof_multizone_update(tof_multizone_t *mz, uint8_t zone_idx, float distance_mm);
float tof_multizone_min(const tof_multizone_t *mz);
float tof_multizone_max(const tof_multizone_t *mz);
float tof_multizone_avg(const tof_multizone_t *mz);
void tof_multizone_print(const tof_multizone_t *mz);

void ultrasonic_init(ultrasonic_config_t *cfg);
void ultrasonic_set_config(ultrasonic_config_t *cfg, float sos_ms, float temp_c, uint8_t avg);
ultrasonic_range_t ultrasonic_measure(float pulse_duration_us, float echo_time_us, uint32_t ts);
float ultrasonic_distance_from_echo(float echo_time_us, float speed_of_sound_ms);

void lidar_scan_init(lidar_scan_t *scan);
void lidar_scan_add_point(lidar_scan_t *scan, float x, float y, float dist, float intensity, float angle);
void lidar_scan_clear(lidar_scan_t *scan);
void lidar_scan_to_cartesian(lidar_scan_t *scan, float angle_deg, float distance_mm);
float lidar_scan_min_distance(const lidar_scan_t *scan);
float lidar_scan_max_distance(const lidar_scan_t *scan);
void lidar_scan_filter_outliers(lidar_scan_t *scan, float threshold_mm);
uint16_t lidar_scan_cluster(const lidar_scan_t *scan, float cluster_radius_mm, uint16_t *labels);

void optical_flow_init(optical_flow_t *flow);
void optical_flow_add_vector(optical_flow_t *flow, int32_t dx, int32_t dy, float quality);
void optical_flow_compute_displacement(optical_flow_t *flow, float pixels_to_mm);
void optical_flow_compute_velocity(optical_flow_t *flow, float dt_s);
void optical_flow_clear(optical_flow_t *flow);

#endif
