#include "../include/motor_control.h"
#include <stdio.h>
#include <math.h>

int main(void)
{
    printf("=== mini-sensor-actuator: Motor Control Demo ===\n\n");

    printf("--- PID Controller Test ---\n");
    {
        pid_controller_t pid;
        pid_init(&pid, 1.0f, 0.1f, 0.05f, 0.01f);
        pid_set_setpoint(&pid, 100.0f);
        printf("PID: kp=%.2f ki=%.2f kd=%.2f dt=%.3f setpoint=%.1f\n",
               pid.kp, pid.ki, pid.kd, pid.dt, pid.setpoint);

        printf("%-6s %-12s %-12s %-12s\n", "Step", "Meas.", "Output", "Integral");
        float measurement = 0.0f;
        int i;
        for (i = 0; i < 20; i++) {
            measurement += (pid.setpoint - measurement) * 0.08f + 0.5f;
            float out = pid_compute(&pid, measurement);
            if (i % 4 == 0) {
                printf("%-6d %-12.2f %-12.4f %-12.4f\n",
                       i, measurement, out, pid.integral);
            }
        }
        printf("  Final measurement: %.2f (setpoint = %.1f)\n\n",
               measurement, pid.setpoint);
    }

    printf("--- DC Motor Control ---\n");
    {
        dc_motor_t motor;
        dc_motor_init(&motor, DC_MOTOR_DRIVER_L298N, 19.0f, 64);
        dc_motor_enable(&motor);

        printf("Motor: driver=L298N, gear=%.1f:1, tpr=%u\n",
               motor.gear_ratio, motor.ticks_per_rev);

        dc_motor_update_encoder(&motor, 0, 0);
        dc_motor_update_encoder(&motor, 128, 500);
        printf("  Encoder update: ticks=128, dt=500ms\n");
        printf("  Speed: %.2f RPM\n", motor.encoder.speed_rpm);

        printf("  Setting speed to 100 RPM...\n");
        dc_motor_set_speed_rpm(&motor, 100.0f);
        printf("  Velocity setpoint: %.1f RPM\n", motor.vel_pid.setpoint);

        dc_motor_set_pwm(&motor, 180, 0);
        printf("  PWM: %u/255, Direction: %s\n",
               motor.pwm_duty, motor.direction == 0 ? "Forward" : "Reverse");

        printf("  Stopping motor...\n");
        dc_motor_stop(&motor);
        printf("  PWM after stop: %u\n\n", motor.pwm_duty);
    }

    printf("--- Stepper Motor Control ---\n");
    {
        stepper_motor_t stepper;
        stepper_init(&stepper, STEPPER_DRIVER_TMC2209, STEPPER_SIXTEENTH);
        stepper_enable(&stepper);

        printf("Stepper: driver=TMC2209, microstep=1/16\n");

        stepper_set_target(&stepper, 3200, 800.0f, 1600.0f);
        printf("  Target: %d steps @ %.0f sps, accel=%.0f sps^2\n",
               stepper.target_steps, stepper.speed_steps_per_sec,
               stepper.acceleration_steps_per_sec2);

        uint32_t t = 0;
        int i;
        for (i = 0; i < 8; i++) {
            t += stepper.step_interval_us;
            uint8_t stepped = stepper_step_update(&stepper, t);
            printf("    t=%6u us: step=%d, remaining=%d, triggered=%d\n",
                   t, stepper.current_steps,
                   (int)stepper_remaining(&stepper), stepped);
        }

        printf("  Position: %d / %d\n", stepper.current_steps, stepper.target_steps);
        printf("  Remaining steps: %d\n\n", (int)stepper_remaining(&stepper));
    }

    printf("--- Servo Motor Control ---\n");
    {
        servo_t servo;
        servo_init(&servo, 0, 0.0f, 180.0f, 500, 2500);

        printf("Servo: ch=0, range=[%.0f, %.0f] deg, pulse=[%u, %u] us\n",
               servo.min_angle_deg, servo.max_angle_deg,
               servo.min_pulse_us, servo.max_pulse_us);

        float test_angles[] = {0.0f, 45.0f, 90.0f, 135.0f, 180.0f};
        printf("  Angle -> Pulse mapping:\n");
        int i;
        for (i = 0; i < 5; i++) {
            uint16_t pulse = servo_angle_to_pulse(&servo, test_angles[i]);
            float back = servo_pulse_to_angle(&servo, pulse);
            printf("    %6.1f deg -> %4u us -> %6.1f deg\n",
                   test_angles[i], pulse, back);
        }

        servo_set_angle(&servo, 90.0f);
        printf("  Set angle to 90 deg\n");

        servo_set_speed_control(&servo, 45.0f);
        printf("  Speed: %.1f deg/s\n\n", servo.speed_dps);
    }

    printf("--- S-Curve Trajectory ---\n");
    {
        s_curve_trajectory_t traj;
        s_curve_init(&traj, 0.0f, 100.0f, 50.0f, 100.0f, 500.0f);

        printf("S-Curve: start=%.1f, end=%.1f, vmax=%.1f, amax=%.1f, jerk=%.1f\n",
               traj.start_pos, traj.end_pos, traj.max_vel,
               traj.max_acc, traj.jerk);
        printf("  Duration: %.4f s\n", traj.duration_s);

        float t = 0.0f;
        int i;
        for (i = 0; i < 12; i++) {
            float pos = s_curve_evaluate(&traj, 0.05f);
            printf("    t=%.3f  pos=%.4f  vel=%.4f  phase=%d\n",
                   t, pos, traj.current_vel, (int)traj.phase);
            t += 0.05f;
            if (s_curve_completed(&traj)) {
                printf("    Trajectory complete at t=%.3f\n", traj.t);
                break;
            }
        }
    }

    printf("\nDemo complete.\n");
    return 0;
}
