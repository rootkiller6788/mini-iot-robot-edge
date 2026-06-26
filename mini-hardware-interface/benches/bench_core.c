#include "actuator_driver.h"
#include "gpio_pwm_adc.h"
#include "pid_controller.h"
#include "sensor_polling.h"
#include "signal_cond.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_ms(void)
{
    return (double)clock() * 1000.0 / (double)CLOCKS_PER_SEC;
}

int main(int argc, char **argv)
{
    int N = 5000;
    if (argc > 1) N = atoi(argv[1]);
    if (N < 1) N = 5000;

    double t0, elapsed;
    int i;

    printf("\n=== mini-hardware-interface Benchmarks (N=%d) ===\n\n", N);

    /* ── GPIO ──────────────────────────────────────────── */
    {
        mhi_gpio_init(5, MHI_GPIO_MODE_OUTPUT);
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            mhi_gpio_write(5, MHI_GPIO_HIGH);
            mhi_gpio_write(5, MHI_GPIO_LOW);
        }
        elapsed = now_ms() - t0;
        printf("  gpio_write/read:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 2, elapsed, (elapsed / (N * 2)) * 1000.0);
        mhi_gpio_deinit(5);
    }

    /* ── PWM ────────────────────────────────────────────── */
    {
        mhi_pwm_init(0, 1000.0f, MHI_PWM_RES_16BIT);
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            mhi_pwm_set_duty(0, (float)(i % 101));
        }
        elapsed = now_ms() - t0;
        printf("  pwm_set_duty:  %d ops in %.1f ms  (%.1f us/op)\n",
               N, elapsed, (elapsed / N) * 1000.0);
        mhi_pwm_deinit(0);
    }

    /* ── ADC ────────────────────────────────────────────── */
    {
        uint32_t raw;
        float voltage;
        mhi_adc_init(0, MHI_ADC_RES_12BIT, 3.3f);
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            mhi_adc_read_single(0, &raw, &voltage);
        }
        elapsed = now_ms() - t0;
        printf("  adc_read_single:  %d ops in %.1f ms  (%.1f us/op)\n",
               N, elapsed, (elapsed / N) * 1000.0);

        t0 = now_ms();
        for (i = 0; i < N; i++) {
            mhi_adc_raw_to_voltage((uint32_t)(i % 4096), 3.3f, 4095);
        }
        elapsed = now_ms() - t0;
        printf("  adc_raw_to_voltage:  %d ops in %.1f ms  (%.1f us/op)\n",
               N, elapsed, (elapsed / N) * 1000.0);
        mhi_adc_deinit(0);
    }

    /* ── Servo ──────────────────────────────────────────── */
    {
        mhi_servo_config_t scfg;
        memset(&scfg, 0, sizeof(scfg));
        scfg.pwm_channel = 1;
        scfg.pulse_min_us = 500;
        scfg.pulse_max_us = 2500;
        scfg.angle_min_deg = 0.0f;
        scfg.angle_max_deg = 180.0f;
        scfg.period_us = 20000;
        mhi_servo_init(&scfg);
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            mhi_servo_set_angle(1, (float)(i % 180));
        }
        elapsed = now_ms() - t0;
        printf("  servo_set_angle:  %d ops in %.1f ms  (%.1f us/op)\n",
               N, elapsed, (elapsed / N) * 1000.0);
        mhi_servo_deinit(1);
    }

    /* ── DC Motor ───────────────────────────────────────── */
    {
        mhi_dc_motor_t motor;
        memset(&motor, 0, sizeof(motor));
        mhi_dc_motor_init(&motor, 0, 1, 2);
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            mhi_dc_motor_set_speed(&motor, (float)(i % 200 - 100) / 100.0f);
        }
        elapsed = now_ms() - t0;
        printf("  dc_motor_set_speed:  %d ops in %.1f ms  (%.1f us/op)\n",
               N, elapsed, (elapsed / N) * 1000.0);

        t0 = now_ms();
        for (i = 0; i < N; i++) {
            mhi_dc_motor_set_direction(&motor, (i & 1) ? MHI_DC_DIR_CCW : MHI_DC_DIR_CW);
            mhi_dc_motor_brake(&motor);
        }
        elapsed = now_ms() - t0;
        printf("  dc_motor_dir+brake:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 2, elapsed, (elapsed / (N * 2)) * 1000.0);
        mhi_dc_motor_deinit(&motor);
    }

    /* ── BLDC Motor ─────────────────────────────────────── */
    {
        mhi_bldc_motor_t bldc;
        mhi_bldc_pins_t pins;
        memset(&pins, 0, sizeof(pins));
        memset(&bldc, 0, sizeof(bldc));
        pins.hall_a = 3; pins.hall_b = 4; pins.hall_c = 5;
        pins.pwm_uh = 6; pins.pwm_ul = 7;
        pins.pwm_vh = 8; pins.pwm_vl = 9;
        pins.pwm_wh = 10; pins.pwm_wl = 11;
        mhi_bldc_motor_init(&bldc, &pins);
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            mhi_bldc_motor_set_speed(&bldc, (float)(i % 200 - 100) / 100.0f);
            mhi_bldc_motor_commutate(&bldc);
        }
        elapsed = now_ms() - t0;
        printf("  bldc_set_speed+commutate:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 2, elapsed, (elapsed / (N * 2)) * 1000.0);
        mhi_bldc_motor_deinit(&bldc);
    }

    /* ── Stepper Motor ──────────────────────────────────── */
    {
        mhi_stepper_motor_t stepper;
        mhi_stepper_pins_t spins;
        memset(&stepper, 0, sizeof(stepper));
        memset(&spins, 0, sizeof(spins));
        spins.coil_a1 = 1; spins.coil_a2 = 2;
        spins.coil_b1 = 3; spins.coil_b2 = 4;
        mhi_stepper_motor_init(&stepper, &spins, 200);
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            mhi_stepper_motor_set_speed(&stepper, (float)(100 + (i % 500)));
            mhi_stepper_motor_move_to(&stepper, (int32_t)(i * 10));
        }
        elapsed = now_ms() - t0;
        printf("  stepper_set_speed+move:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 2, elapsed, (elapsed / (N * 2)) * 1000.0);
        mhi_stepper_motor_deinit(&stepper);
    }

    /* ── Servo Motor ────────────────────────────────────── */
    {
        mhi_servo_motor_t servo;
        memset(&servo, 0, sizeof(servo));
        mhi_servo_motor_init(&servo, 2);
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            mhi_servo_motor_set_angle(&servo, (float)(i % 180));
        }
        elapsed = now_ms() - t0;
        printf("  servo_motor_set_angle:  %d ops in %.1f ms  (%.1f us/op)\n",
               N, elapsed, (elapsed / N) * 1000.0);
        mhi_servo_motor_disable(&servo);
    }

    /* ── PID Controller ─────────────────────────────────── */
    {
        mhi_pid_t pid;
        mhi_pid_config_t cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.kp = 2.0f; cfg.ki = 0.5f; cfg.kd = 0.1f;
        cfg.setpoint = 100.0f;
        cfg.sample_time_s = 0.01f;
        cfg.output_min = -100.0f; cfg.output_max = 100.0f;
        cfg.form = MHI_PID_FORM_POSITIONAL;
        cfg.anti_windup = MHI_PID_ANTI_WINDUP_CLAMPING;
        cfg.direction = MHI_PID_DIRECT;
        mhi_pid_init(&pid, &cfg);
        float measurement = 0.0f;
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            measurement += (float)((i & 1) ? 1.0f : -0.5f);
            if (measurement > 200.0f) measurement = 100.0f;
            if (measurement < -50.0f) measurement = 0.0f;
            mhi_pid_compute(&pid, 100.0f, measurement, (uint32_t)(i * 10));
        }
        elapsed = now_ms() - t0;
        printf("  pid_compute:  %d ops in %.1f ms  (%.1f us/op)\n",
               N, elapsed, (elapsed / N) * 1000.0);

        /* Ziegler-Nichols tuning */
        mhi_zn_result_t zn;
        t0 = now_ms();
        for (i = 0; i < N / 2; i++) {
            mhi_zn_tune(MHI_ZN_TYPE_PID, 2.5f, 1.0f, &zn);
        }
        elapsed = now_ms() - t0;
        printf("  zn_tune:  %d ops in %.1f ms  (%.1f us/op)\n",
               N / 2, elapsed, (elapsed / (N / 2)) * 1000.0);

        /* Cascaded PID */
        mhi_cascaded_pid_t cpid;
        mhi_pid_config_t outer_cfg = cfg;
        mhi_pid_config_t inner_cfg = cfg;
        inner_cfg.kp = 1.0f; inner_cfg.ki = 0.1f; inner_cfg.kd = 0.01f;
        mhi_cascaded_pid_init(&cpid, &outer_cfg, &inner_cfg);
        float outer_meas = 0.0f, inner_meas = 0.0f;
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            outer_meas += (float)((i & 3) ? 0.3f : -0.2f);
            inner_meas += (float)((i & 1) ? 0.5f : -0.3f);
            mhi_cascaded_pid_compute(&cpid, 50.0f, outer_meas, inner_meas, (uint32_t)(i * 10));
        }
        elapsed = now_ms() - t0;
        printf("  cascaded_pid_compute:  %d ops in %.1f ms  (%.1f us/op)\n",
               N, elapsed, (elapsed / N) * 1000.0);
    }

    /* ── Sensor Bus ──────────────────────────────────────── */
    {
        mhi_bus_config_t bcfg;
        memset(&bcfg, 0, sizeof(bcfg));
        bcfg.type = MHI_BUS_I2C;
        bcfg.address = 0x76;
        bcfg.speed_hz = 400000;
        mhi_bus_init(&bcfg);
        uint8_t tx[4] = {0xAA, 0xBB, 0xCC, 0xDD};
        uint8_t rx[4];
        t0 = now_ms();
        for (i = 0; i < N / 2; i++) {
            mhi_bus_write_reg(0xF4, tx, 2);
            mhi_bus_read_reg(0xF7, rx, 2);
        }
        elapsed = now_ms() - t0;
        printf("  bus_write_reg+read_reg:  %d ops in %.1f ms  (%.1f us/op)\n",
               N, elapsed, (elapsed / N) * 1000.0);
        mhi_bus_deinit();
    }

    /* ── Moving Average Filter ──────────────────────────── */
    {
        mhi_moving_average_t ma;
        float buf[32];
        mhi_ma_init(&ma, buf, 32);
        t0 = now_ms();
        for (i = 0; i < N * 5; i++) {
            mhi_ma_update(&ma, (float)(i % 100));
        }
        elapsed = now_ms() - t0;
        printf("  ma_update:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 5, elapsed, (elapsed / (N * 5)) * 1000.0);
    }

    /* ── Median Filter ───────────────────────────────────── */
    {
        mhi_median_filter_t mf;
        float mbuf[16], msort[16];
        mhi_median_init(&mf, mbuf, msort, 15);
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            mhi_median_update(&mf, (float)(i % 200));
        }
        elapsed = now_ms() - t0;
        printf("  median_update:  %d ops in %.1f ms  (%.1f us/op)\n",
               N, elapsed, (elapsed / N) * 1000.0);
    }

    /* ── Kalman Filter ──────────────────────────────────── */
    {
        mhi_kalman_1d_t kf;
        mhi_kalman_1d_init(&kf, 25.0f, 0.01f, 0.1f);
        t0 = now_ms();
        for (i = 0; i < N * 3; i++) {
            mhi_kalman_1d_update(&kf, (float)(25.0f + (i & 7) - 3.5f));
        }
        elapsed = now_ms() - t0;
        printf("  kalman_1d_update:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 3, elapsed, (elapsed / (N * 3)) * 1000.0);
    }

    /* ── Calibration ────────────────────────────────────── */
    {
        mhi_calibration_t cal;
        mhi_calibration_init(&cal);
        mhi_calibration_set_zero(&cal, 0.0f, 0.0f);
        mhi_calibration_set_span(&cal, 1000.0f, 100.0f);
        t0 = now_ms();
        for (i = 0; i < N * 3; i++) {
            mhi_calibration_apply(&cal, (float)(i % 1000));
        }
        elapsed = now_ms() - t0;
        printf("  calibration_apply:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 3, elapsed, (elapsed / (N * 3)) * 1000.0);
    }

    /* ── Op-Amp ─────────────────────────────────────────── */
    {
        mhi_opamp_t opamp;
        mhi_opamp_config_t ocfg;
        memset(&ocfg, 0, sizeof(ocfg));
        ocfg.topology = MHI_OPAMP_NON_INVERTING;
        ocfg.r1 = 10000.0f;
        ocfg.r2 = 100000.0f;
        mhi_opamp_init(&opamp, &ocfg, 5.0f, 0.0f);
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            mhi_opamp_non_inverting(&opamp, (float)(i % 330) / 100.0f);
            mhi_opamp_inverting(&opamp, (float)(i % 330) / 100.0f);
            mhi_opamp_compute(&opamp, (float)(i % 330) / 100.0f, 0.0f);
        }
        elapsed = now_ms() - t0;
        printf("  opamp_compute:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 3, elapsed, (elapsed / (N * 3)) * 1000.0);
    }

    /* ── RC Filter ──────────────────────────────────────── */
    {
        mhi_rc_filter_t rc;
        mhi_rc_filter_init(&rc, MHI_FILTER_LOWPASS, 1000.0f, 1e-6f, 0.001f);
        t0 = now_ms();
        for (i = 0; i < N * 5; i++) {
            mhi_rc_filter_update(&rc, (float)(i % 500) / 100.0f);
        }
        elapsed = now_ms() - t0;
        printf("  rc_filter_update:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 5, elapsed, (elapsed / (N * 5)) * 1000.0);

        t0 = now_ms();
        for (i = 0; i < N * 2; i++) {
            mhi_rc_filter_cutoff_freq(&rc);
        }
        elapsed = now_ms() - t0;
        printf("  rc_filter_cutoff_freq:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 2, elapsed, (elapsed / (N * 2)) * 1000.0);
    }

    /* ── 2nd-Order Filter ───────────────────────────────── */
    {
        mhi_filter_2nd_t f2;
        mhi_filter_2nd_init(&f2, MHI_FILTER_LOWPASS, 100.0f, 0.707f, 1000.0f);
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            mhi_filter_2nd_update(&f2, (float)(i % 500) / 100.0f);
        }
        elapsed = now_ms() - t0;
        printf("  filter_2nd_update:  %d ops in %.1f ms  (%.1f us/op)\n",
               N, elapsed, (elapsed / N) * 1000.0);
    }

    /* ── Wheatstone Bridge ──────────────────────────────── */
    {
        mhi_wheatstone_t wb;
        mhi_wheatstone_init(&wb, MHI_WHEATSTONE_FULL, 5.0f, 2.0f, 350.0f);
        t0 = now_ms();
        for (i = 0; i < N * 3; i++) {
            mhi_wheatstone_output_voltage(&wb, (float)(i % 1000) * 1e-6f);
        }
        elapsed = now_ms() - t0;
        printf("  wheatstone_output_voltage:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 3, elapsed, (elapsed / (N * 3)) * 1000.0);
    }

    /* ── Optocoupler ────────────────────────────────────── */
    {
        mhi_optocoupler_t oc;
        mhi_optocoupler_init(&oc, 1.0f, 1.2f, 20.0f, 3.3f, 10000.0f);
        t0 = now_ms();
        for (i = 0; i < N * 2; i++) {
            mhi_optocoupler_output_voltage(&oc, (float)(i % 20) + 1.0f);
        }
        elapsed = now_ms() - t0;
        printf("  optocoupler_output_voltage:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 2, elapsed, (elapsed / (N * 2)) * 1000.0);
    }

    /* ── Anti-Alias Filter Design ───────────────────────── */
    {
        mhi_anti_alias_t aa;
        t0 = now_ms();
        for (i = 0; i < N / 2; i++) {
            mhi_anti_alias_design(&aa, 1000.0f, 400.0f, 2);
        }
        elapsed = now_ms() - t0;
        printf("  anti_alias_design:  %d ops in %.1f ms  (%.1f us/op)\n",
               N / 2, elapsed, (elapsed / (N / 2)) * 1000.0);
    }

    /* ── Sensor Manager ─────────────────────────────────── */
    {
        mhi_sensor_t sensors[4];
        float r0[1] = {0}, f0[1] = {0};
        float r1[1] = {0}, f1[1] = {0};
        float r2[1] = {0}, f2[1] = {0};
        float r3[1] = {0}, f3[1] = {0};
        memset(sensors, 0, sizeof(sensors));
        for (i = 0; i < 4; i++) {
            sensors[i].id = (uint8_t)i;
            sensors[i].type = (mhi_sensor_type_t)i;
            sensors[i].poll_interval_ms = 100;
            sensors[i].value_count = 1;
            sensors[i].raw_values = (i == 0) ? r0 : (i == 1) ? r1 : (i == 2) ? r2 : r3;
            sensors[i].filtered_values = (i == 0) ? f0 : (i == 1) ? f1 : (i == 2) ? f2 : f3;
        }
        mhi_sensor_manager_t mgr;
        mhi_sensor_manager_init(&mgr, sensors, 4);
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            mhi_sensor_manager_service(&mgr, (uint32_t)(i * 10));
        }
        elapsed = now_ms() - t0;
        printf("  sensor_manager_service:  %d ops in %.1f ms  (%.1f us/op)\n",
               N, elapsed, (elapsed / N) * 1000.0);
    }

    printf("\n=== Benchmarks Complete ===\n\n");
    return 0;
}
