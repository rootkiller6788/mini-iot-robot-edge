#include "pid_controller.h"
#include "actuator_driver.h"
#include "gpio_pwm_adc.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>

/* ── simulated motor with inertia ── */
static float s_motor_speed = 0.0f;
static float s_motor_inertia = 0.1f;

static float simulate_motor_dynamics(float pwm_cmd, float dt)
{
    float alpha = 1.0f - expf(-dt / s_motor_inertia);
    s_motor_speed += alpha * (pwm_cmd - s_motor_speed);
    return s_motor_speed;
}

int main(void)
{
    printf("=== Example: PID Motor Speed Control ===\n\n");

    /* ── PID config: speed control ── */
    mhi_pid_config_t speed_cfg = {
        .kp             = 2.5f,
        .ki             = 0.8f,
        .kd             = 0.05f,
        .setpoint       = 0.0f,
        .sample_time_s  = 0.01f,
        .output_min     = -1.0f,
        .output_max     = 1.0f,
        .form           = MHI_PID_FORM_POSITIONAL,
        .anti_windup    = MHI_PID_ANTI_WINDUP_BOTH,
        .direction      = MHI_PID_DIRECT,
        .back_calc_gain = 0.5f,
        .derivative_filter_a = 0.1f
    };

    mhi_pid_t speed_pid;
    mhi_pid_init(&speed_pid, &speed_cfg);

    printf("[PID] Speed controller initialized\n");
    printf("  Kp=%.2f  Ki=%.2f  Kd=%.2f\n", speed_cfg.kp, speed_cfg.ki, speed_cfg.kd);
    printf("  Anti-windup: BOTH (clamping + back-calculation)\n");

    /* ── Simulate step response ── */
    printf("\n[PID] Step response: setpoint 0.0 -> 1.0 (100%% speed)\n");
    printf("  Time(ms)  |  Setpoint  |  Measured  |  PID Out  |  P  |  I  |  D\n");
    printf("  ----------+------------+------------+-----------+-----+-----+----\n");

    s_motor_speed = 0.0f;
    mhi_pid_reset(&speed_pid);

    float setpoint = 1.0f;
    uint32_t ms;
    int line_count = 0;
    for (ms = 0u; ms <= 1000u; ms += 20u) {
        float measured = simulate_motor_dynamics(speed_pid.output, 0.02f);
        float pid_out = mhi_pid_compute(&speed_pid, setpoint, measured, ms);

        if (line_count++ % 5 == 0) {
            printf("  %8u  |  %8.2f  |  %8.3f  |  %7.3f  | % 4.2f| % 4.2f| % 4.2f\n",
                   ms, setpoint, measured, pid_out,
                   speed_pid.p_term, speed_pid.i_term, speed_pid.d_term);
        }
    }

    printf("\n[PID] Steady-state: output=%.3f, P=%.3f I=%.3f D=%.3f saturated=%d\n",
           speed_pid.output,
           speed_pid.p_term,
           speed_pid.i_term,
           speed_pid.d_term,
           mhi_pid_is_saturated(&speed_pid));

    /* ── Reverse direction test ── */
    printf("\n[PID] Reverse command: setpoint 1.0 -> -0.5\n");
    setpoint = -0.5f;

    for (ms = 0u; ms <= 500u; ms += 20u) {
        float measured = simulate_motor_dynamics(speed_pid.output, 0.02f);
        float pid_out = mhi_pid_compute(&speed_pid, setpoint, measured,
                                        ms + 1000u);
        if (ms % 100u == 0u) {
            printf("  t=%ums  measured=%.3f  cmd=%.3f  ssp=%.2f\n",
                   ms, measured, pid_out, setpoint);
        }
    }

    /* ── Ziegler-Nichols Auto-Tuning ── */
    printf("\n[PID] Ziegler-Nichols auto-tuning demonstration:\n");
    float ku = 4.5f;   /* ultimate gain (oscillation method) */
    float tu = 0.08f;  /* ultimate period (80ms) */

    mhi_zn_result_t zn_p, zn_pi, zn_pid;
    mhi_zn_tune(MHI_ZN_TYPE_P,   ku, tu, &zn_p);
    mhi_zn_tune(MHI_ZN_TYPE_PI,  ku, tu, &zn_pi);
    mhi_zn_tune(MHI_ZN_TYPE_PID, ku, tu, &zn_pid);

    printf("  Ku=%.2f, Tu=%.3fs\n", ku, tu);
    printf("  ZN-P:   Kp=%.4f\n", zn_p.kp);
    printf("  ZN-PI:  Kp=%.4f  Ki=%.4f\n", zn_pi.kp, zn_pi.ki);
    printf("  ZN-PID: Kp=%.4f  Ki=%.4f  Kd=%.4f\n", zn_pid.kp, zn_pid.ki, zn_pid.kd);

    /* Apply ZN-PID gains */
    mhi_pid_set_gains(&speed_pid, zn_pid.kp, zn_pid.ki, zn_pid.kd);
    mhi_pid_reset(&speed_pid);
    printf("  Applied ZN-PID gains\n");

    /* ── Feed-forward test ── */
    printf("\n[PID] Feed-forward: constant 0.2 bias\n");
    mhi_pid_set_feed_forward(&speed_pid, 0.2f);

    s_motor_speed = 0.0f;
    mhi_pid_reset(&speed_pid);
    setpoint = 1.0f;

    for (ms = 0u; ms <= 300u; ms += 20u) {
        float measured = simulate_motor_dynamics(speed_pid.output, 0.02f);
        mhi_pid_compute(&speed_pid, setpoint, measured, ms);
        if (ms % 100u == 0u) {
            printf("  t=%ums  output=%.3f  ff=%.2f\n",
                   ms, speed_pid.output, speed_pid.ff_term);
        }
    }
    mhi_pid_set_feed_forward(&speed_pid, 0.0f);

    /* ── Cascaded PID: position + velocity loops ── */
    printf("\n[PID] Cascaded PID: position(outer) + velocity(inner)\n");

    mhi_pid_config_t pos_cfg = {
        .kp = 10.0f,  .ki = 0.1f,  .kd = 0.02f,
        .setpoint = 0.0f,
        .sample_time_s = 0.01f,
        .output_min = -2.0f, .output_max = 2.0f,
        .form = MHI_PID_FORM_POSITIONAL,
        .anti_windup = MHI_PID_ANTI_WINDUP_CLAMPING,
        .direction = MHI_PID_DIRECT,
        .back_calc_gain = 1.0f,
        .derivative_filter_a = 0.1f
    };

    mhi_pid_config_t vel_cfg = {
        .kp = 1.5f, .ki = 0.3f, .kd = 0.01f,
        .setpoint = 0.0f,
        .sample_time_s = 0.01f,
        .output_min = -1.0f, .output_max = 1.0f,
        .form = MHI_PID_FORM_POSITIONAL,
        .anti_windup = MHI_PID_ANTI_WINDUP_CLAMPING,
        .direction = MHI_PID_DIRECT,
        .back_calc_gain = 1.0f,
        .derivative_filter_a = 0.1f
    };

    mhi_cascaded_pid_t cascaded;
    mhi_cascaded_pid_init(&cascaded, &pos_cfg, &vel_cfg);

    float position = 0.0f;
    float velocity = 0.0f;
    float pos_setpoint = 10.0f; /* target: position = 10 */

    printf("  Outer: Kp=%.1f Ki=%.1f Kd=%.2f  Inner: Kp=%.1f Ki=%.1f Kd=%.2f\n",
           pos_cfg.kp, pos_cfg.ki, pos_cfg.kd,
           vel_cfg.kp, vel_cfg.ki, vel_cfg.kd);
    printf("  Position setpoint = %.0f\n", pos_setpoint);

    for (ms = 0u; ms <= 2000u; ms += 50u) {
        float cmd = mhi_cascaded_pid_compute(&cascaded,
                                             pos_setpoint, position,
                                             velocity, ms);
        velocity += 0.1f * cmd;
        position += velocity * 0.05f;

        if (ms % 200u == 0u) {
            printf("  t=%ums  pos=%.2f(->%.0f)  vel=%.3f  cmd=%.3f\n",
                   ms, position, pos_setpoint, velocity, cmd);
        }
    }

    printf("\n[PID] Final state: pos=%.2f  vel=%.3f\n", position, velocity);

    /* ── DC Motor actuator test ── */
    printf("\n[MOTOR] DC motor init: PWM ch3, IN1=pin4, IN2=pin5\n");
    mhi_dc_motor_t motor;
    mhi_dc_motor_init(&motor, 3u, 4u, 5u);
    motor.soft_start_ramp_s = 0.5f;

    printf("[MOTOR] Set speed 75%% CW:\n");
    mhi_dc_motor_set_speed(&motor, 0.75f);
    mhi_dc_motor_tick(&motor, 500u);
    printf("  direction=%d speed=%.2f\n", (int)motor.direction, motor.current_speed);

    printf("[MOTOR] Emergency STOP:\n");
    mhi_dc_motor_emergency_stop(&motor);
    printf("  estop=%d speed=%.2f\n", motor.emergency_stop, motor.current_speed);

    printf("[MOTOR] Resume, set speed -50%% CCW:\n");
    mhi_dc_motor_resume(&motor);
    mhi_dc_motor_set_speed(&motor, -0.5f);
    mhi_dc_motor_tick(&motor, 600u);
    printf("  direction=%d speed=%.2f\n", (int)motor.direction, motor.current_speed);

    printf("[MOTOR] Brake:\n");
    mhi_dc_motor_brake(&motor);
    printf("  direction=%d speed=%.2f\n", (int)motor.direction, motor.current_speed);

    /* ── Stepper motor test ── */
    printf("\n[STEPPER] Init: 200 steps/rev, full-step mode\n");
    mhi_stepper_pins_t stp_pins = { .coil_a1 = 6u, .coil_a2 = 7u,
                                     .coil_b1 = 8u, .coil_b2 = 9u };
    mhi_stepper_motor_t stepper;
    mhi_stepper_motor_init(&stepper, &stp_pins, 200u);

    printf("[STEPPER] Move to position 400 (2 revolutions):\n");
    mhi_stepper_motor_set_speed(&stepper, 200.0f);
    mhi_stepper_motor_move_to(&stepper, 400);

    uint32_t us;
    for (us = 0u; us <= 3000000u; us += 5000u) {
        mhi_stepper_motor_tick(&stepper, us);
    }
    printf("  final position = %d\n", stepper.position);

    /* ── Servo motor test ── */
    printf("\n[SERVO] Init on PWM ch2:\n");
    mhi_servo_motor_t servo;
    mhi_servo_motor_init(&servo, 2u);
    mhi_servo_motor_set_angle(&servo, 45.0f);
    printf("  angle = %.1f\n", mhi_servo_motor_get_angle(&servo));

    mhi_servo_motor_calibrate(&servo, 600u, 2400u, -90.0f, 90.0f);
    mhi_servo_motor_set_angle(&servo, -30.0f);
    printf("  calibrated: angle = %.1f\n", mhi_servo_motor_get_angle(&servo));
    mhi_servo_motor_disable(&servo);

    /* ── Cleanup ── */
    mhi_dc_motor_deinit(&motor);
    mhi_stepper_motor_deinit(&stepper);

    printf("\n=== Example complete ===\n");
    return 0;
}
