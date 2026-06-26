/*
 * demo_full.c - Full Demonstration of mini-sensor-actuator
 *
 * Walks through all 5 sub-modules:
 *   camera_mipi.h, imu_sensor.h, motor_control.h, temp_env.h, tof_lidar.h
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

int main(void) {
    printf("\n");
    printf("******************************************************************\n");
    printf("*                                                                *\n");
    printf("*    MINI-SENSOR-ACTUATOR  --  Full Feature Demonstration        *\n");
    printf("*  Camera | IMU | Motor | Temp/Env | ToF/LiDAR                   *\n");
    printf("*                                                                *\n");
    printf("******************************************************************\n");
    printf("\n");

    /* ---------------------------------------------------------------
     *  SECTION 1 -- camera_mipi.h  (MIPI Camera Pipeline)
     * --------------------------------------------------------------- */
    printf("--- Section 1: Camera MIPI Pipeline ---\n\n");

    camera_sensor_t cam;
    camera_mipi_init(&cam, CAMERA_RES_1080P, CAMERA_FORMAT_RAW10, 4);
    printf("[OK] camera_mipi_init() -- 1080p RAW10, %d lanes\n", cam.mipi.lanes);

    camera_mipi_set_frame_rate(&cam, 30.0f);
    printf("[OK] camera_mipi_set_frame_rate() -- %.1f FPS\n", cam.frame_rate_fps);

    camera_bayer_set_pattern(&cam, CAMERA_BAYER_RGGB);
    printf("[OK] camera_bayer_set_pattern() -- RGGB Bayer\n");

    camera_mipi_start_stream(&cam);
    printf("[OK] camera_mipi_start_stream() -- streaming started\n");

    /* Simulate a raw frame */
    uint8_t sample_raw[16 * 16];
    memset(sample_raw, 0x80, sizeof(sample_raw));
    camera_raw_frame_t raw = { sample_raw, sizeof(sample_raw), 16, 16, CAMERA_BAYER_RGGB, 8 };

    /* Bayer demosaic */
    uint8_t sample_rgb[16 * 16 * 3];
    camera_rgb_frame_t rgb_out = { sample_rgb, 16, 16, 16 * 3 };
    camera_bayer_demosaic_bilinear(&raw, &rgb_out);
    printf("[OK] camera_bayer_demosaic_bilinear() -- 16x16 demosaiced\n");

    /* AE (Auto Exposure) */
    camera_ae_t ae;
    camera_ae_init(&ae, CAMERA_AE_MATRIX, 128.0f);
    camera_ae_set_limits(&ae, 100, 100000, 1.0f, 32.0f);
    camera_ae_set_convergence(&ae, 0.5f);
    float brightness = camera_ae_compute_brightness(sample_rgb, 16, 16, CAMERA_AE_MATRIX);
    camera_ae_update(&ae, brightness);
    printf("[OK] camera_ae_init/update() -- brightness=%.1f, exp=%dus\n",
           brightness, ae.exposure_time_us);

    /* AWB (Auto White Balance) */
    camera_awb_t awb;
    camera_awb_init(&awb, CAMERA_AWB_GRAY_WORLD);
    camera_awb_gray_world(&awb, sample_rgb, 16, 16);
    camera_awb_apply(&awb, sample_rgb, 16, 16);
    printf("[OK] camera_awb_init/apply() -- gains: R=%.2f G=%.2f B=%.2f\n",
           awb.r_gain, awb.g_gain, awb.b_gain);

    /* ISP Pipeline */
    camera_isp_config_t isp;
    camera_isp_init(&isp);
    camera_isp_set_brightness(&isp, 5);
    camera_isp_set_contrast(&isp, 1.0f);
    camera_isp_set_saturation(&isp, 1.0f);
    camera_isp_apply_gamma(&isp, sample_rgb, 16, 16);
    printf("[OK] camera_isp_init/apply_gamma() -- brightness=%d contrast=%.1f\n",
           isp.brightness, isp.contrast);

    /* JPEG compression estimate */
    cam.jpeg_enabled = 1;
    cam.jpeg_quality = 85;
    uint8_t jpeg_buf[1024];
    uint32_t jpeg_size = camera_jpeg_compress_placeholder(&rgb_out, jpeg_buf, sizeof(jpeg_buf));
    printf("[OK] camera_jpeg_compress_placeholder() -- %u bytes, quality=%d%%\n",
           jpeg_size, cam.jpeg_quality);

    camera_mipi_stop_stream(&cam);
    printf("[OK] camera_mipi_stop_stream() -- streaming stopped\n");

    /* ---------------------------------------------------------------
     *  SECTION 2 -- imu_sensor.h  (IMU + AHRS Fusion)
     * --------------------------------------------------------------- */
    printf("\n--- Section 2: IMU Sensor Fusion ---\n\n");

    imu_sensor_t imu;
    imu_init(&imu, IMU_AHRS_MADGWICK, 100.0f);
    printf("[OK] imu_init() -- MADGWICK AHRS at 100 Hz\n");

    /* Simulate a sample (device flat on table pointing north) */
    imu_sample_t sample;
    sample.accel.ax = 0.01f;
    sample.accel.ay = 0.02f;
    sample.accel.az = 9.81f;
    sample.gyro.gx = 0.001f;
    sample.gyro.gy = -0.002f;
    sample.gyro.gz = 0.003f;
    sample.mag.mx = 20.0f;
    sample.mag.my = -5.0f;
    sample.mag.mz = 35.0f;
    sample.timestamp_ms = 100;

    imu_update_all(&imu, &sample);
    printf("[OK] imu_update_all() -- accel(%.2f,%.2f,%.2f) gyro(%.3f,%.3f,%.3f)\n",
           sample.accel.ax, sample.accel.ay, sample.accel.az,
           sample.gyro.gx, sample.gyro.gy, sample.gyro.gz);

    /* Run AHRS */
    imu_ahrs_update_madgwick(&imu, &sample);
    printf("[OK] imu_ahrs_update_madgwick() -- quat(%.4f,%.4f,%.4f,%.4f)\n",
           imu.orientation.w, imu.orientation.x,
           imu.orientation.y, imu.orientation.z);

    /* Convert to Euler angles */
    imu_quat_to_euler(&imu.orientation, &imu.angles);
    printf("[OK] imu_quat_to_euler() -- roll=%.1f pitch=%.1f yaw=%.1f deg\n",
           imu.angles.roll, imu.angles.pitch, imu.angles.yaw);

    /* Verify roundtrip */
    imu_quat_t q_rt;
    imu_euler_to_quat(&imu.angles, &q_rt);
    printf("[OK] imu_euler_to_quat() -- roundtrip quat(%.4f,%.4f,%.4f,%.4f)\n",
           q_rt.w, q_rt.x, q_rt.y, q_rt.z);

    /* IMU FIFO */
    imu_fifo_push(&imu.fifo, &sample);
    imu_fifo_push(&imu.fifo, &sample);
    printf("[OK] imu_fifo_push x2 -- fifo count=%d\n", imu_fifo_count(&imu.fifo));
    imu_fifo_flush(&imu.fifo);
    printf("[OK] imu_fifo_flush() -- fifo cleared\n");

    /* Motion detection */
    imu_motion_evt_t evt = imu_detect_freefall(&sample.accel, 0.5f);
    printf("[OK] imu_detect_freefall() -- event=%d (NONE expected)\n", evt);

    /* ---------------------------------------------------------------
     *  SECTION 3 -- motor_control.h  (PID + DC + Stepper + Servo + S-Curve)
     * --------------------------------------------------------------- */
    printf("\n--- Section 3: Motor Control ---\n\n");

    /* PID Controller */
    pid_controller_t pid;
    pid_init(&pid, 2.0f, 1.5f, 0.1f, 0.01f);
    pid_set_limits(&pid, PID_OUTPUT_MIN, PID_OUTPUT_MAX, PID_INTEGRAL_MIN, PID_INTEGRAL_MAX);
    pid_set_setpoint(&pid, 100.0f);
    float pid_out = pid_compute(&pid, 95.0f);
    printf("[OK] pid_init/compute() -- setpoint=100 meas=95 -> output=%.3f\n", pid_out);

    pid_set_gains(&pid, 3.0f, 2.0f, 0.15f);
    printf("[OK] pid_set_gains() -- kp=%.1f ki=%.1f kd=%.2f\n", pid.kp, pid.ki, pid.kd);

    pid_reset(&pid);
    printf("[OK] pid_reset() -- integral=%.4f\n", pid.integral);

    /* DC Motor */
    dc_motor_t motor;
    dc_motor_init(&motor, DC_MOTOR_DRIVER_L298N, 19.2f, 48);
    dc_motor_set_pwm(&motor, 128, 1);
    printf("[OK] dc_motor_init/set_pwm() -- duty=%d dir=%d, gear=%.1f\n",
           motor.pwm_duty, motor.direction, motor.gear_ratio);
    dc_motor_stop(&motor);
    printf("[OK] dc_motor_stop()\n");

    /* Stepper Motor */
    stepper_motor_t stepper;
    stepper_init(&stepper, STEPPER_DRIVER_A4988, STEPPER_SIXTEENTH);
    stepper_set_target(&stepper, 800, 200.0f, 100.0f);
    printf("[OK] stepper_init/set_target() -- target=%d, speed=%.0f SPS, microstep=16\n",
           stepper.target_steps, stepper.speed_steps_per_sec);
    uint8_t stepped = stepper_step_update(&stepper, 1000000);
    printf("[OK] stepper_step_update() -- stepped=%d, remaining=%d\n",
           stepped, stepper_remaining(&stepper));

    /* Servo */
    servo_t servo;
    servo_init(&servo, 0, 0.0f, 180.0f, 500, 2500);
    servo_set_angle(&servo, 45.0f);
    uint16_t pulse = servo_angle_to_pulse(&servo, 45.0f);
    printf("[OK] servo_init/set_angle() -- angle=45 deg -> pulse=%d us\n", pulse);

    /* S-Curve Trajectory */
    s_curve_trajectory_t traj;
    s_curve_init(&traj, 0.0f, 50.0f, 30.0f, 15.0f, 5.0f);
    float pos = s_curve_evaluate(&traj, 0.5f);
    printf("[OK] s_curve_init/evaluate() -- pos at t=0.5s: %.2f mm\n", pos);

    /* ---------------------------------------------------------------
     *  SECTION 4 -- temp_env.h  (Environmental Sensors)
     * --------------------------------------------------------------- */
    printf("\n--- Section 4: Environmental Sensors ---\n\n");

    env_sensor_t env;
    env_init(&env);

    env_temp_humi_update(&env.temp_humi, 25.0f, 55.0f, 1000);
    env.has_temp_humi = 1;
    printf("[OK] env_temp_humi_update() -- temp=%.1f C, humidity=%.1f %%\n",
           env.temp_humi.temperature_c, env.temp_humi.humidity_pct);

    float dew_pt = env_dew_point(25.0f, 55.0f);
    printf("[OK] env_dew_point() -- %.2f C\n", dew_pt);

    float hi = env_heat_index_c(30.0f, 70.0f);
    printf("[OK] env_heat_index_c() -- %.1f C\n", hi);

    float abs_hum = env_absolute_humidity(25.0f, 55.0f);
    printf("[OK] env_absolute_humidity() -- %.2f g/m^3\n", abs_hum);

    env_baro_update(&env.pressure, 1013.25f, 25.0f);
    env.has_pressure = 1;
    printf("[OK] env_baro_update() -- %.2f hPa\n", env.pressure.pressure_hpa);

    float alt = env_altitude_from_pressure(1013.25f, PRESSURE_SEA_LEVEL_HPA);
    printf("[OK] env_altitude_from_pressure() -- %.1f m\n", alt);

    env_gas_update_voc(&env.gas, 120.0f, 1000);
    env_gas_update_co2(&env.gas, 410.0f, 1000);
    env.has_gas = 1;
    printf("[OK] env_gas_update() -- VOC=%.0f ppb, CO2=%.0f ppm\n",
           env.gas.voc_ppb, env.gas.co2_ppm);

    env_dust_update(&env.dust, 5.0f, 10.0f, 15.0f, 2.5f, 1000);
    env.has_dust = 1;
    float aqi = env_dust_aqi_pm25(10.0f);
    printf("[OK] env_dust_update() -- PM2.5=%.1f ug/m3, AQI=%.0f\n", env.dust.pm2_5_ugm3, aqi);

    env_wind_update(&env.wind, 3.5f, 270.0f, 6.0f, 1000);
    env.has_wind = 1;
    wind_direction_bin_t wbin = env_wind_to_bin(270.0f);
    printf("[OK] env_wind_update() -- speed=%.1f m/s dir=%s\n",
           env.wind.speed_ms, env_wind_bin_name(wbin));

    env_rain_tip(&env.rain, 1000);
    env.has_rain = 1;
    float rain_rate = env_rain_rate(&env.rain);
    printf("[OK] env_rain_tip() -- tip_count=%d, rate=%.2f mm/h\n",
           env.rain.tip_count, rain_rate);

    env_uv_update(&env.uv, 5.5f, 1000);
    env.has_uv = 1;
    printf("[OK] env_uv_update() -- UV index=%.1f (%s)\n",
           env.uv.uv_index, env_uv_level_name(env.uv.level));

    printf("\n[ Summary ]\n");
    env_print_summary(&env);

    /* ---------------------------------------------------------------
     *  SECTION 5 -- tof_lidar.h  (ToF + LiDAR + Ultrasonic + Optical Flow)
     * --------------------------------------------------------------- */
    printf("\n--- Section 5: ToF / LiDAR Distance Sensors ---\n\n");

    /* ToF Sensor */
    tof_sensor_t tof;
    tof_init(&tof, TOF_VL53L1X);
    tof_set_mode(&tof, TOF_MODE_SINGLE);
    tof_set_range_profile(&tof, TOF_RANGE_LONG);
    tof_set_timing_budget(&tof, 50000);
    printf("[OK] tof_init/set_mode() -- VL53L1X, long range, 50ms budget\n");

    tof_range_t range = tof_range_once(&tof);
    printf("[OK] tof_range_once() -- distance=%.1f mm, signal=%.1f Mcps\n",
           range.distance_mm, range.signal_rate_mcps);

    /* ToF Multi-zone */
    tof_multizone_t mz;
    tof_multizone_init(&mz, 4, 4);
    for (uint8_t z = 0; z < 4; z++) {
        tof_multizone_update(&mz, z, 100.0f * (z + 1));
    }
    float mz_min = tof_multizone_min(&mz);
    float mz_max = tof_multizone_max(&mz);
    float mz_avg = tof_multizone_avg(&mz);
    printf("[OK] tof_multizone 4x4 -- min=%.1f max=%.1f avg=%.1f mm\n", mz_min, mz_max, mz_avg);

    /* LiDAR Scan */
    lidar_scan_t scan;
    lidar_scan_init(&scan);
    scan.scan_frequency_hz = 10.0f;
    scan.angular_resolution_deg = 1.0f;

    /* Simulate scan points (circle with radius 500mm) */
    for (int deg = 0; deg < 360; deg += 10) {
        float rad = (float)deg * 3.14159f / 180.0f;
        float x = 500.0f * cosf(rad);
        float y = 500.0f * sinf(rad);
        lidar_scan_add_point(&scan, x, y, 500.0f, 100.0f, (float)deg);
    }
    printf("[OK] lidar_scan_add_point x36 -- %d points, min_dist=%.1f max_dist=%.1f mm\n",
           scan.point_count, lidar_scan_min_distance(&scan), lidar_scan_max_distance(&scan));

    /* Filter outliers */
    lidar_scan_filter_outliers(&scan, 100.0f);
    printf("[OK] lidar_scan_filter_outliers() -- threshold=100mm, %d points remain\n",
           scan.point_count);

    /* ToF calibration */
    tof_offset_set(&tof, 5.0f);
    printf("[OK] tof_offset_set() -- offset=5.0mm\n");

    /* Ultrasonic simulation */
    ultrasonic_config_t us_cfg;
    ultrasonic_init(&us_cfg);
    ultrasonic_set_config(&us_cfg, 343.0f, 20.0f, 8);
    ultrasonic_range_t us_range = ultrasonic_measure(10, 580, 1000);
    printf("[OK] ultrasonic_init/measure() -- %.1f mm (config: %d samples avg)\n",
           us_range.distance_mm, us_cfg.num_samples_avg);

    /* Optical Flow */
    optical_flow_t flow;
    optical_flow_init(&flow);
    optical_flow_add_vector(&flow, 5, -3, 0.95f);
    optical_flow_add_vector(&flow, 6, -2, 0.90f);
    optical_flow_compute_displacement(&flow, 1.0f);
    optical_flow_compute_velocity(&flow, 0.033f);
    printf("[OK] optical_flow -- %d vectors, vel=(%.1f,%.1f) mm/s\n",
           flow.vector_count, flow.velocity_x_mms, flow.velocity_y_mms);

    /* ---------------------------------------------------------------
     *  COMPLETION BANNER
     * --------------------------------------------------------------- */
    printf("\n*************************************************************\n");
    printf("*      MINI-SENSOR-ACTUATOR  --  Demo Complete               *\n");
    printf("*  5 Modules | 15+ Sensor Types | 20+ Actuator Functions     *\n");
    printf("*************************************************************\n\n");

    return 0;
}
