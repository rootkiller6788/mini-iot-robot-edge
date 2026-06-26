/*
 * bench_core.c - Core Benchmarks for mini-sensor-actuator
 *
 * Measures performance of major API functions across all 5 modules:
 *   camera_mipi.h, imu_sensor.h, motor_control.h, temp_env.h, tof_lidar.h
 *
 * Usage: bench_core [N]
 *   N = iteration scale factor (default 5000)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdint.h>

#include "camera_mipi.h"
#include "imu_sensor.h"
#include "motor_control.h"
#include "temp_env.h"
#include "tof_lidar.h"

/* ---- helper: high-resolution timer ---- */
static double now_ms(void) {
    return (double)clock() * 1000.0 / (double)CLOCKS_PER_SEC;
}

static void bench_run(const char *name, void (*fn)(int), int n) {
    double t0 = now_ms();
    fn(n);
    double t1 = now_ms();
    double elapsed = t1 - t0;
    printf("  %-45s  %d ops in %9.1f ms  (%8.1f us/op)\n",
           name, n, elapsed, (elapsed * 1000.0) / (double)n);
}

/* ================================================================
 *  BENCHMARKS - camera_mipi.h
 * ================================================================ */

static void bm_camera_init(int n) {
    for (int i = 0; i < n; i++) {
        camera_sensor_t cam;
        camera_mipi_init(&cam, CAMERA_RES_VGA, CAMERA_FORMAT_RAW8, 2);
    }
}

static void bm_camera_bayer_demosaic(int n) {
    camera_sensor_t cam;
    camera_mipi_init(&cam, CAMERA_RES_720P, CAMERA_FORMAT_RAW10, 2);
    /* simulate raw frame */
    uint8_t raw_buf[CAMERA_BAYER_MAX];
    uint8_t rgb_buf[CAMERA_RGB_MAX];
    memset(raw_buf, 0x80, sizeof(raw_buf));
    camera_raw_frame_t raw = { raw_buf, sizeof(raw_buf), 640, 480, CAMERA_BAYER_RGGB, 8 };
    camera_rgb_frame_t rgb = { rgb_buf, 640, 480, 640 * 3 };
    for (int i = 0; i < n; i++) {
        camera_bayer_demosaic_vng(&raw, &rgb);
    }
    (void)cam;
}

static void bm_camera_ae_update(int n) {
    camera_ae_t ae;
    camera_ae_init(&ae, CAMERA_AE_CENTER_WEIGHTED, 128.0f);
    uint8_t rgb[64];
    memset(rgb, 128, sizeof(rgb));
    for (int i = 0; i < n; i++) {
        camera_ae_update(&ae, 120.0f + (float)(i % 20));
    }
    (void)rgb;
}

static void bm_camera_isp_pipeline(int n) {
    camera_sensor_t cam;
    camera_mipi_init(&cam, CAMERA_RES_720P, CAMERA_FORMAT_RAW10, 2);
    camera_isp_init(&cam.isp);
    uint8_t small_rgb[100 * 100 * 3];
    memset(small_rgb, 128, sizeof(small_rgb));
    cam.rgb.rgb_data = small_rgb;
    cam.rgb.width = 100;
    cam.rgb.height = 100;
    cam.rgb.stride = 100 * 3;
    for (int i = 0; i < n; i++) {
        camera_isp_pipeline(&cam);
    }
}

/* ================================================================
 *  BENCHMARKS - imu_sensor.h
 * ================================================================ */

static void bm_imu_read_all(int n) {
    imu_sensor_t imu;
    imu_init(&imu, IMU_AHRS_MADGWICK, 100.0f);
    imu_sample_t sample = { {0}, {0}, {0}, 0 };
    for (int i = 0; i < n; i++) {
        imu_update_all(&imu, &sample);
    }
}

static void bm_imu_ahrs_madgwick(int n) {
    imu_sensor_t imu;
    imu_init(&imu, IMU_AHRS_MADGWICK, 100.0f);
    imu_sample_t sample = { {0.1f, 0.2f, 9.8f}, {0.01f, 0.02f, -0.01f}, {20.0f, -10.0f, 30.0f}, 0 };
    for (int i = 0; i < n; i++) {
        imu_ahrs_update_madgwick(&imu, &sample);
    }
}

static void bm_imu_quat_to_euler(int n) {
    imu_quat_t q = { 1.0f, 0.0f, 0.0f, 0.0f };
    imu_euler_t e;
    for (int i = 0; i < n; i++) {
        imu_quat_to_euler(&q, &e);
    }
}

static void bm_imu_fifo_ops(int n) {
    imu_fifo_t fifo;
    memset(&fifo, 0, sizeof(fifo));
    imu_sample_t sample = { {0}, {0}, {0}, 100 };
    for (int i = 0; i < n; i++) {
        imu_fifo_push(&fifo, &sample);
        imu_fifo_pop(&fifo, &sample);
    }
}

/* ================================================================
 *  BENCHMARKS - motor_control.h
 * ================================================================ */

static void bm_pid_compute(int n) {
    pid_controller_t pid;
    pid_init(&pid, 1.0f, 0.1f, 0.05f, 0.01f);
    pid_set_limits(&pid, PID_OUTPUT_MIN, PID_OUTPUT_MAX, PID_INTEGRAL_MIN, PID_INTEGRAL_MAX);
    pid_set_setpoint(&pid, 100.0f);
    for (int i = 0; i < n; i++) {
        pid_compute(&pid, 100.0f + (float)(i % 10) - 5.0f);
    }
}

static void bm_servo_set_angle(int n) {
    servo_t servo;
    servo_init(&servo, 0, SERVO_MIN_ANGLE_DEG, SERVO_MAX_ANGLE_DEG,
               SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US);
    for (int i = 0; i < n; i++) {
        servo_set_angle(&servo, 45.0f + (float)(i % 90));
    }
}

static void bm_s_curve_evaluate(int n) {
    s_curve_trajectory_t traj;
    s_curve_init(&traj, 0.0f, 100.0f, 50.0f, 20.0f, 10.0f);
    float dt = 0.001f;
    for (int i = 0; i < n; i++) {
        s_curve_evaluate(&traj, dt);
    }
}

static void bm_stepper_step_update(int n) {
    stepper_motor_t stepper;
    stepper_init(&stepper, STEPPER_DRIVER_A4988, STEPPER_FULL);
    stepper_set_target(&stepper, 1000, 500.0f, 200.0f);
    uint32_t now = 1000000;
    for (int i = 0; i < n; i++) {
        stepper_step_update(&stepper, now);
        now += 100;
    }
}

/* ================================================================
 *  BENCHMARKS - temp_env.h
 * ================================================================ */

static void bm_env_read_all(int n) {
    env_sensor_t env;
    env_init(&env);
    env_temp_humi_update(&env.temp_humi, 25.0f, 60.0f, 1000);
    env_baro_update(&env.pressure, 1013.0f, 25.0f);
    env_gas_update_voc(&env.gas, 150.0f, 1000);
    env_dust_update(&env.dust, 5.0f, 12.0f, 20.0f, 3.5f, 1000);
    env_wind_update(&env.wind, 2.5f, 180.0f, 5.0f, 1000);
    env_rain_tip(&env.rain, 1000);
    env_uv_update(&env.uv, 3.5f, 1000);
    for (int i = 0; i < n; i++) {
        env_print_summary(&env);
    }
}

static void bm_env_dew_point_compute(int n) {
    for (int i = 0; i < n; i++) {
        env_dew_point(25.0f + (float)(i % 15), 50.0f + (float)(i % 40));
    }
}

/* ================================================================
 *  BENCHMARKS - tof_lidar.h
 * ================================================================ */

static void bm_tof_range(int n) {
    tof_sensor_t sensor;
    tof_init(&sensor, TOF_VL53L1X);
    tof_set_mode(&sensor, TOF_MODE_SINGLE);
    for (int i = 0; i < n; i++) {
        tof_range_once(&sensor);
    }
}

static void bm_lidar_scan(int n) {
    lidar_scan_t scan;
    lidar_scan_init(&scan);
    for (int i = 0; i < n; i++) {
        lidar_scan_add_point(&scan, (float)(i % 100), (float)(i % 100),
                             50.0f + (float)i, 100.0f, (float)(i % 360));
    }
}

/* ================================================================
 *  MAIN
 * ================================================================ */

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 5000;
    if (N <= 0) N = 5000;

    printf("\n=== mini-sensor-actuator Benchmarks (N=%d) ===\n\n", N);

    int n_sensor = N / 10;   /* sensor reads */
    int n_image  = N / 20;   /* image processing */
    int n_control = N / 5;   /* motor/PID */

    bench_run("camera_mipi_init (create config)",           bm_camera_init,          n_sensor * 5);
    bench_run("camera_bayer_demosaic_vng (VNG demosaic)",   bm_camera_bayer_demosaic, n_image);
    bench_run("camera_ae_update (auto-exposure)",            bm_camera_ae_update,      n_sensor * 5);
    bench_run("camera_isp_pipeline (color+gamma)",          bm_camera_isp_pipeline,   n_image);
    bench_run("imu_update_all (accel+gyro+mag)",             bm_imu_read_all,          n_sensor * 5);
    bench_run("imu_ahrs_update_madgwick (AHRS fusion)",     bm_imu_ahrs_madgwick,     n_sensor * 5);
    bench_run("imu_quat_to_euler (quaternion conv)",        bm_imu_quat_to_euler,     n_control);
    bench_run("imu_fifo_push/pop (FIFO roundtrip)",         bm_imu_fifo_ops,          n_sensor * 5);
    bench_run("pid_compute (PID iteration)",                 bm_pid_compute,           n_control);
    bench_run("servo_set_angle (servo update)",              bm_servo_set_angle,       n_control);
    bench_run("s_curve_evaluate (trajectory step)",          bm_s_curve_evaluate,      n_control);
    bench_run("stepper_step_update (stepper motor)",         bm_stepper_step_update,   n_control);
    bench_run("env_read_all + print (env sensor summary)",   bm_env_read_all,          n_sensor);
    bench_run("env_dew_point (thermodynamic calc)",          bm_env_dew_point_compute, n_control);
    bench_run("tof_range_once (TOF ranging)",               bm_tof_range,             n_sensor * 5);
    bench_run("lidar_scan_add_point (scan data)",            bm_lidar_scan,            n_control);

    printf("\nDone.\n");
    return 0;
}
