#include "../include/imu_sensor.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

static void print_euler(const char* label, const imu_euler_t* e)
{
    printf("%-12s Roll: %8.2f   Pitch: %8.2f   Yaw: %8.2f\n",
           label, e->roll, e->pitch, e->yaw);
}

static void simulate_sensor_data(imu_sample_t* s, float t)
{
    s->accel.ax = 0.0f;
    s->accel.ay = 0.0f;
    s->accel.az = IMU_GRAVITY;

    s->gyro.gx = 5.0f * sinf(t * 0.5f);
    s->gyro.gy = 3.0f * cosf(t * 0.7f);
    s->gyro.gz = 2.0f * sinf(t * 1.1f);

    s->mag.mx = 25.0f * cosf(t * 0.3f);
    s->mag.my = 10.0f * sinf(t * 0.4f);
    s->mag.mz = 40.0f;

    s->timestamp_ms = (uint32_t)(t * 1000.0f);
}

int main(void)
{
    printf("=== mini-sensor-actuator: IMU Sensor Fusion Demo ===\n\n");

    imu_sensor_t imu;
    imu_init(&imu, IMU_AHRS_MADGWICK, 100.0f);
    printf("IMU initialized: AHRS=Madgwick, SampleRate=100Hz, Beta=%.2f\n\n", imu.beta);

    imu_calib_t calib;
    memset(&calib, 0, sizeof(calib));
    calib.az_bias = -0.02f;
    imu_set_calibration(&imu, &calib);
    printf("Calibration applied: az_bias = %.4f\n\n", calib.az_bias);

    printf("Running sensor fusion simulation (200 iterations)...\n\n");
    printf("%-6s %-12s %-46s %-28s\n",
           "Step", "Time(s)", "Accel (ax,ay,az)", "Euler (Roll, Pitch, Yaw)");
    printf("%-6s %-12s %-46s %-28s\n",
           "----", "-------", "-----------------------", "------------------------");

    int i;
    for (i = 0; i < 200; i++) {
        float t = (float)i * 0.01f;
        imu_sample_t sample;
        simulate_sensor_data(&sample, t);
        imu_update_all(&imu, &sample);

        if (i % 40 == 0) {
            printf("%-6d %-12.2f (%7.2f, %7.2f, %7.2f)     %c %8.2f %8.2f %8.2f %c\n",
                   i, t,
                   sample.accel.ax, sample.accel.ay, sample.accel.az,
                   '(', imu.angles.roll, imu.angles.pitch, imu.angles.yaw, ')');
        }
    }

    printf("\n=== Final Orientation ===\n");
    print_euler("Madgwick:", &imu.angles);
    printf("  Quaternion: w=%.4f x=%.4f y=%.4f z=%.4f\n",
           imu.orientation.w, imu.orientation.x,
           imu.orientation.y, imu.orientation.z);

    printf("\n=== Compare: Mahony Filter ===\n");
    imu_sensor_t imu_mahony;
    imu_init(&imu_mahony, IMU_AHRS_MAHONY, 100.0f);
    imu_set_calibration(&imu_mahony, &calib);
    for (i = 0; i < 200; i++) {
        float t = (float)i * 0.01f;
        imu_sample_t sample;
        simulate_sensor_data(&sample, t);
        imu_update_all(&imu_mahony, &sample);
    }
    print_euler("Mahony:", &imu_mahony.angles);
    printf("  Quaternion: w=%.4f x=%.4f y=%.4f z=%.4f\n",
           imu_mahony.orientation.w, imu_mahony.orientation.x,
           imu_mahony.orientation.y, imu_mahony.orientation.z);

    printf("\n=== Motion Detection ===\n");
    imu_accel_t freefall_acc = {0.0f, 0.0f, 0.3f};
    imu_motion_evt_t ff = imu_detect_freefall(&freefall_acc, 1.0f);
    printf("Freefall test (accel=0.3g): %s\n",
           ff == IMU_MOTION_FREEFALL ? "FREEFALL DETECTED" : "normal");

    printf("\n=== FIFO Buffer Test ===\n");
    printf("FIFO count before flush: %u\n", imu_fifo_count(&imu.fifo));
    imu_fifo_flush(&imu.fifo);
    printf("FIFO count after flush:  %u\n", imu_fifo_count(&imu.fifo));

    printf("\n=== Bias Estimation ===\n");
    imu_sample_t bias_samples[100];
    memset(bias_samples, 0, sizeof(bias_samples));
    imu_estimate_bias(&imu, bias_samples, 100);
    printf("Estimated gyro biases: gx=%.4f gy=%.4f gz=%.4f\n",
           imu.calib.gx_bias, imu.calib.gy_bias, imu.calib.gz_bias);

    printf("\n=== Quaternion Operations ===\n");
    imu_quat_t q1 = {0.707f, 0.707f, 0.0f, 0.0f};
    imu_quat_t q2 = {0.707f, 0.0f, 0.707f, 0.0f};
    imu_quat_t q_result;
    imu_quat_multiply(&q_result, &q1, &q2);
    printf("q1 * q2 = (%.4f, %.4f, %.4f, %.4f)\n",
           q_result.w, q_result.x, q_result.y, q_result.z);

    imu_euler_t euler_from_q;
    imu_quat_to_euler(&q_result, &euler_from_q);
    print_euler("From q:", &euler_from_q);

    printf("\nDemo complete.\n");
    return 0;
}
