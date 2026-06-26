#ifndef IMU_SENSOR_H
#define IMU_SENSOR_H

#include <stdint.h>
#include <stddef.h>

#define IMU_FIFO_DEPTH 128
#define IMU_AXIS_COUNT 3
#define IMU_QUAT_ELEMENTS 4
#define IMU_EULER_ANGLES 3
#define IMU_GRAVITY 9.80665f

typedef enum {
    IMU_AHRS_MADGWICK = 0,
    IMU_AHRS_MAHONY   = 1
} imu_ahrs_method_t;

typedef struct {
    float ax, ay, az;
} imu_accel_t;

typedef struct {
    float gx, gy, gz;
} imu_gyro_t;

typedef struct {
    float mx, my, mz;
} imu_mag_t;

typedef struct {
    float w, x, y, z;
} imu_quat_t;

typedef struct {
    float roll, pitch, yaw;
} imu_euler_t;

typedef struct {
    float ax_bias, ay_bias, az_bias;
    float gx_bias, gy_bias, gz_bias;
    float mx_bias, my_bias, mz_bias;
    float ax_scale, ay_scale, az_scale;
    float gx_scale, gy_scale, gz_scale;
    float mx_scale, my_scale, mz_scale;
} imu_calib_t;

typedef enum {
    IMU_MOTION_NONE     = 0,
    IMU_MOTION_FREEFALL = 1,
    IMU_MOTION_SINGLE_TAP = 2,
    IMU_MOTION_DOUBLE_TAP = 3,
    IMU_MOTION_ACTIVITY   = 4
} imu_motion_evt_t;

typedef struct {
    imu_accel_t accel;
    imu_gyro_t gyro;
    imu_mag_t mag;
    uint32_t timestamp_ms;
} imu_sample_t;

typedef struct {
    imu_sample_t buffer[IMU_FIFO_DEPTH];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} imu_fifo_t;

typedef struct {
    imu_accel_t accel;
    imu_gyro_t gyro;
    imu_mag_t mag;
    imu_quat_t orientation;
    imu_euler_t angles;
    imu_calib_t calib;
    imu_fifo_t fifo;
    imu_ahrs_method_t ahrs_method;
    float beta;
    float sample_period;
    uint32_t last_update_ms;
    uint8_t initialized;
} imu_sensor_t;

void imu_init(imu_sensor_t *imu, imu_ahrs_method_t method, float sample_freq);
void imu_set_calibration(imu_sensor_t *imu, const imu_calib_t *calib);
void imu_apply_calibration(imu_sensor_t *imu, imu_sample_t *sample);
void imu_update_accel(imu_sensor_t *imu, float ax, float ay, float az, uint32_t ts);
void imu_update_gyro(imu_sensor_t *imu, float gx, float gy, float gz, uint32_t ts);
void imu_update_mag(imu_sensor_t *imu, float mx, float my, float mz, uint32_t ts);
void imu_update_all(imu_sensor_t *imu, const imu_sample_t *sample);
void imu_ahrs_update_madgwick(imu_sensor_t *imu, const imu_sample_t *sample);
void imu_ahrs_update_mahony(imu_sensor_t *imu, const imu_sample_t *sample);
void imu_quat_to_euler(const imu_quat_t *q, imu_euler_t *e);
void imu_euler_to_quat(const imu_euler_t *e, imu_quat_t *q);
void imu_quat_normalize(imu_quat_t *q);
void imu_quat_multiply(imu_quat_t *out, const imu_quat_t *a, const imu_quat_t *b);

void imu_fifo_push(imu_fifo_t *fifo, const imu_sample_t *sample);
uint8_t imu_fifo_pop(imu_fifo_t *fifo, imu_sample_t *sample);
uint8_t imu_fifo_peek(const imu_fifo_t *fifo, imu_sample_t *sample);
void imu_fifo_flush(imu_fifo_t *fifo);
uint8_t imu_fifo_count(const imu_fifo_t *fifo);

imu_motion_evt_t imu_detect_freefall(const imu_accel_t *accel, float threshold);
imu_motion_evt_t imu_detect_tap(const imu_sensor_t *imu);
void imu_estimate_bias(imu_sensor_t *imu, const imu_sample_t *samples, uint16_t num_samples);

#endif
