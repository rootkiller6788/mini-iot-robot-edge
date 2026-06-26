/*
 * test_core.c - Core Unit Tests for mini-sensor-actuator
 *
 * Tests all 5 modules: camera_mipi.h, imu_sensor.h, motor_control.h,
 *   temp_env.h, tof_lidar.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#include "camera_mipi.h"
#include "imu_sensor.h"
#include "motor_control.h"
#include "temp_env.h"
#include "tof_lidar.h"

/* ---- test harness ---- */
static int tests_run = 0, tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  TEST %s ... ", name); } while(0)
#define PASS()     do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg)  do { printf("FAIL: %s\n", msg); return 1; } while(0)
#define CHECK(cond, msg) if (!(cond)) FAIL(msg)

/* ================================================================
 *  camera_mipi.h tests
 * ================================================================ */

static int test_camera_init(void) {
    TEST("camera_mipi_init");
    camera_sensor_t cam;
    camera_mipi_init(&cam, CAMERA_RES_VGA, CAMERA_FORMAT_RAW8, 2);
    CHECK(cam.mipi.resolution == CAMERA_RES_VGA, "resolution mismatch");
    CHECK(cam.mipi.width == 640, "width mismatch for VGA");
    CHECK(cam.mipi.lanes == 2, "lanes mismatch");
    PASS();
    return 0;
}

static int test_camera_start_stop_stream(void) {
    TEST("camera_mipi_start_stream / stop_stream");
    camera_sensor_t cam;
    camera_mipi_init(&cam, CAMERA_RES_VGA, CAMERA_FORMAT_RAW8, 2);
    camera_mipi_start_stream(&cam);
    CHECK(cam.streaming == 1, "streaming not started");
    camera_mipi_stop_stream(&cam);
    CHECK(cam.streaming == 0, "streaming not stopped");
    PASS();
    return 0;
}

static int test_camera_bayer_demosaic(void) {
    TEST("camera_bayer_demosaic_simple");
    uint8_t raw_buf[64];
    uint8_t rgb_buf[192];
    memset(raw_buf, 0x80, sizeof(raw_buf));
    camera_raw_frame_t raw = { raw_buf, sizeof(raw_buf), 8, 8, CAMERA_BAYER_RGGB, 8 };
    camera_rgb_frame_t rgb = { rgb_buf, 8, 8, 8 * 3 };
    camera_bayer_demosaic_simple(&raw, &rgb);
    /* verify output RGB is non-zero (demosaic produced something) */
    int non_zero = 0;
    for (int i = 0; i < 192; i++) {
        if (rgb_buf[i] != 0) { non_zero = 1; break; }
    }
    CHECK(non_zero == 1, "demosaic produced all-zero output");
    PASS();
    return 0;
}

static int test_camera_ae_init_evaluate(void) {
    TEST("camera_ae_init / evaluate");
    camera_ae_t ae;
    camera_ae_init(&ae, CAMERA_AE_CENTER_WEIGHTED, 128.0f);
    CHECK(ae.mode == CAMERA_AE_CENTER_WEIGHTED, "AE mode mismatch");
    CHECK(ae.target_brightness == 128.0f, "target brightness mismatch");
    uint8_t rgb[64];
    memset(rgb, 128, sizeof(rgb));
    float b = camera_ae_compute_brightness(rgb, 8, 8, CAMERA_AE_CENTER_WEIGHTED);
    CHECK(b >= 0.0f, "brightness computation failed");
    camera_ae_update(&ae, b);
    PASS();
    return 0;
}

/* ================================================================
 *  imu_sensor.h tests
 * ================================================================ */

static int test_imu_init(void) {
    TEST("imu_init");
    imu_sensor_t imu;
    imu_init(&imu, IMU_AHRS_MADGWICK, 100.0f);
    CHECK(imu.ahrs_method == IMU_AHRS_MADGWICK, "AHRS method mismatch");
    CHECK(imu.sample_period > 0.0f, "sample period is zero");
    CHECK(imu.beta > 0.0f, "beta is zero");
    PASS();
    return 0;
}

static int test_imu_ahrs_madgwick(void) {
    TEST("imu_ahrs_update_madgwick");
    imu_sensor_t imu;
    imu_init(&imu, IMU_AHRS_MADGWICK, 100.0f);
    imu_sample_t sample = { {0.0f, 0.0f, 9.81f}, {0.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}, 0 };
    imu_ahrs_update_madgwick(&imu, &sample);
    /* quaternion should be non-zero and normalized */
    float norm = sqrtf(imu.orientation.w * imu.orientation.w +
                       imu.orientation.x * imu.orientation.x +
                       imu.orientation.y * imu.orientation.y +
                       imu.orientation.z * imu.orientation.z);
    CHECK(norm > 0.99f && norm < 1.01f, "quaternion not normalized after AHRS");
    PASS();
    return 0;
}

static int test_imu_quat_euler_conversion(void) {
    TEST("imu_quat_to_euler / imu_euler_to_quat roundtrip");
    imu_quat_t q_in = { 0.7071f, 0.0f, 0.7071f, 0.0f };
    imu_euler_t e;
    imu_quat_t q_out;
    imu_quat_to_euler(&q_in, &e);
    imu_euler_to_quat(&e, &q_out);
    float dot = fabsf(q_in.w * q_out.w + q_in.x * q_out.x +
                      q_in.y * q_out.y + q_in.z * q_out.z);
    CHECK(dot > 0.90f, "quaternion roundtrip deviation too large");
    PASS();
    return 0;
}

static int test_imu_fifo(void) {
    TEST("imu_fifo_push / pop / count / flush");
    imu_fifo_t fifo;
    memset(&fifo, 0, sizeof(fifo));
    imu_sample_t sample;
    memset(&sample, 0, sizeof(sample));
    for (int i = 0; i < 10; i++) {
        sample.timestamp_ms = (uint32_t)(100 * i);
        imu_fifo_push(&fifo, &sample);
    }
    CHECK(imu_fifo_count(&fifo) == 10, "fifo count mismatch after push");
    imu_sample_t popped;
    uint8_t ok = imu_fifo_pop(&fifo, &popped);
    CHECK(ok == 1, "fifo pop failed");
    CHECK(popped.timestamp_ms == 0, "fifo pop returned wrong sample");
    CHECK(imu_fifo_count(&fifo) == 9, "fifo count mismatch after pop");
    imu_fifo_flush(&fifo);
    CHECK(imu_fifo_count(&fifo) == 0, "fifo not empty after flush");
    PASS();
    return 0;
}

/* ================================================================
 *  motor_control.h tests
 * ================================================================ */

static int test_pid_controller(void) {
    TEST("pid_init / pid_compute / pid_reset / pid_set_gains");
    pid_controller_t pid;
    pid_init(&pid, 1.0f, 0.1f, 0.05f, 0.01f);
    CHECK(fabsf(pid.kp - 1.0f) < 0.001f, "kp mismatch");
    CHECK(fabsf(pid.ki - 0.1f) < 0.001f, "ki mismatch");
    CHECK(fabsf(pid.kd - 0.05f) < 0.001f, "kd mismatch");
    pid_set_setpoint(&pid, 50.0f);
    pid_set_limits(&pid, PID_OUTPUT_MIN, PID_OUTPUT_MAX, PID_INTEGRAL_MIN, PID_INTEGRAL_MAX);
    float out = pid_compute(&pid, 40.0f);
    CHECK(out > PID_OUTPUT_MIN - 0.1f && out < PID_OUTPUT_MAX + 0.1f, "pid output out of limits");
    pid_set_gains(&pid, 2.0f, 0.2f, 0.1f);
    CHECK(fabsf(pid.kp - 2.0f) < 0.001f, "pid_set_gains failed");
    pid_reset(&pid);
    CHECK(fabsf(pid.integral) < 0.001f, "pid_reset integral not cleared");
    PASS();
    return 0;
}

static int test_servo_angle(void) {
    TEST("servo_init / servo_set_angle / angle_to_pulse");
    servo_t servo;
    servo_init(&servo, 0, SERVO_MIN_ANGLE_DEG, SERVO_MAX_ANGLE_DEG,
               SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US);
    servo_set_angle(&servo, 90.0f);
    CHECK(fabsf(servo.angle_deg - 90.0f) < 1.0f, "servo angle not set correctly");
    uint16_t pulse = servo_angle_to_pulse(&servo, 90.0f);
    CHECK(pulse >= SERVO_MIN_PULSE_US && pulse <= SERVO_MAX_PULSE_US, "pulse out of range");
    float angle = servo_pulse_to_angle(&servo, pulse);
    CHECK(fabsf(angle - 90.0f) < 5.0f, "pulse-to-angle roundtrip mismatch");
    PASS();
    return 0;
}

static int test_s_curve_trajectory(void) {
    TEST("s_curve_init / s_curve_evaluate / s_curve_completed");
    s_curve_trajectory_t traj;
    s_curve_init(&traj, 0.0f, 100.0f, 50.0f, 20.0f, 10.0f);
    CHECK(fabsf(traj.start_pos - 0.0f) < 0.01f, "start_pos mismatch");
    CHECK(fabsf(traj.end_pos - 100.0f) < 0.01f, "end_pos mismatch");
    float pos = s_curve_evaluate(&traj, 0.0f);
    CHECK(fabsf(pos - 0.0f) < 1.0f, "s_curve initial position not at start");
    uint8_t done = s_curve_completed(&traj);
    CHECK(done == 0, "s_curve should not be completed at t=0");
    PASS();
    return 0;
}

/* ================================================================
 *  temp_env.h tests
 * ================================================================ */

static int test_env_dew_point(void) {
    TEST("env_dew_point (thermodynamic calculation)");
    float dp = env_dew_point(25.0f, 60.0f);
    /* dew point at 25C 60%RH should be ~16.7C */
    CHECK(dp > 15.0f && dp < 18.0f, "dew_point out of expected range");
    float hi = env_heat_index_c(30.0f, 70.0f);
    CHECK(hi > 25.0f, "heat_index out of expected range");
    PASS();
    return 0;
}

static int test_env_altitude(void) {
    TEST("env_altitude_from_pressure / sea_level_pressure");
    float alt = env_altitude_from_pressure(900.0f, PRESSURE_SEA_LEVEL_HPA);
    CHECK(alt > 0.0f, "altitude should be positive for lower pressure");
    float slp = env_sea_level_pressure(900.0f, 1000.0f, 20.0f);
    CHECK(slp > 900.0f, "sea level pressure should be higher than station");
    PASS();
    return 0;
}

/* ================================================================
 *  tof_lidar.h tests
 * ================================================================ */

static int test_tof_init_range(void) {
    TEST("tof_init / tof_range_once");
    tof_sensor_t sensor;
    tof_init(&sensor, TOF_VL53L1X);
    CHECK(sensor.model == TOF_VL53L1X, "TOF model mismatch");
    CHECK(sensor.mode == TOF_MODE_SINGLE, "default mode should be single");
    tof_set_range_profile(&sensor, TOF_RANGE_LONG);
    tof_set_timing_budget(&sensor, 33000);
    tof_range_t range = tof_range_once(&sensor);
    CHECK(range.distance_mm >= 0.0f, "TOF range returned negative distance");
    PASS();
    return 0;
}

static int test_lidar_scan(void) {
    TEST("lidar_scan_init / add_point / clear");
    lidar_scan_t scan;
    lidar_scan_init(&scan);
    lidar_scan_add_point(&scan, 10.0f, 20.0f, 22.36f, 100.0f, 45.0f);
    CHECK(scan.point_count == 1, "point count should be 1 after add");
    lidar_scan_add_point(&scan, 30.0f, 40.0f, 50.0f, 200.0f, 90.0f);
    CHECK(scan.point_count == 2, "point count should be 2 after second add");
    float min_d = lidar_scan_min_distance(&scan);
    float max_d = lidar_scan_max_distance(&scan);
    CHECK(min_d <= max_d, "min distance > max distance");
    lidar_scan_clear(&scan);
    CHECK(scan.point_count == 0, "point count should be 0 after clear");
    PASS();
    return 0;
}

/* ================================================================
 *  MAIN
 * ================================================================ */

int main(void) {
    printf("\n=== mini-sensor-actuator Unit Tests ===\n\n");

    /* camera_mipi.h */
    test_camera_init();
    test_camera_start_stop_stream();
    test_camera_bayer_demosaic();
    test_camera_ae_init_evaluate();

    /* imu_sensor.h */
    test_imu_init();
    test_imu_ahrs_madgwick();
    test_imu_quat_euler_conversion();
    test_imu_fifo();

    /* motor_control.h */
    test_pid_controller();
    test_servo_angle();
    test_s_curve_trajectory();

    /* temp_env.h */
    test_env_dew_point();
    test_env_altitude();

    /* tof_lidar.h */
    test_tof_init_range();
    test_lidar_scan();

    printf("\n%d / %d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
