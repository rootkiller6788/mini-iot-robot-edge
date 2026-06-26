#include "../include/motor_control.h"
#include <math.h>
#include <string.h>

void pid_init(pid_controller_t *pid, float kp, float ki, float kd, float dt)
{
    memset(pid, 0, sizeof(*pid));
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->dt = dt;
    pid->integral_min = PID_INTEGRAL_MIN;
    pid->integral_max = PID_INTEGRAL_MAX;
    pid->output_min = PID_OUTPUT_MIN;
    pid->output_max = PID_OUTPUT_MAX;
}

void pid_set_limits(pid_controller_t *pid, float out_min, float out_max, float int_min, float int_max)
{
    pid->output_min = out_min;
    pid->output_max = out_max;
    pid->integral_min = int_min;
    pid->integral_max = int_max;
}

float pid_compute(pid_controller_t *pid, float measurement)
{
    float error = pid->setpoint - measurement;
    pid->integral += error * pid->dt;
    if (pid->integral > pid->integral_max) pid->integral = pid->integral_max;
    if (pid->integral < pid->integral_min) pid->integral = pid->integral_min;
    float derivative = (pid->dt > 0.0f) ? (error - pid->prev_error) / pid->dt : 0.0f;
    pid->prev_error = error;
    pid->output = pid->kp * error + pid->ki * pid->integral + pid->kd * derivative;
    if (pid->output > pid->output_max) pid->output = pid->output_max;
    if (pid->output < pid->output_min) pid->output = pid->output_min;
    return pid->output;
}

void pid_reset(pid_controller_t *pid)
{
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->output = 0.0f;
}

void pid_set_setpoint(pid_controller_t *pid, float setpoint)
{
    pid->setpoint = setpoint;
}

void pid_set_gains(pid_controller_t *pid, float kp, float ki, float kd)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
}

void dc_motor_init(dc_motor_t *motor, dc_driver_type_t driver, float gear_ratio, uint16_t tpr)
{
    memset(motor, 0, sizeof(*motor));
    motor->driver = driver;
    motor->gear_ratio = gear_ratio;
    motor->ticks_per_rev = tpr;
    motor->pwm_duty = 0;
    motor->direction = 0;
    motor->enabled = 0;
    pid_init(&motor->pos_pid, 2.0f, 0.1f, 0.05f, 0.01f);
    pid_init(&motor->vel_pid, 1.5f, 0.5f, 0.02f, 0.01f);
    pid_set_limits(&motor->pos_pid, -1.0f, 1.0f, -0.5f, 0.5f);
    pid_set_limits(&motor->vel_pid, -1.0f, 1.0f, -0.3f, 0.3f);
}

void dc_motor_set_pwm(dc_motor_t *motor, uint16_t pwm, uint8_t dir)
{
    if (pwm > DC_MOTOR_MAX_PWM) pwm = DC_MOTOR_MAX_PWM;
    motor->pwm_duty = pwm;
    motor->direction = dir;
}

void dc_motor_set_speed_rpm(dc_motor_t *motor, float rpm)
{
    pid_set_setpoint(&motor->vel_pid, rpm);
    motor->encoder.speed_rpm = rpm;
}

void dc_motor_update_encoder(dc_motor_t *motor, int32_t ticks, uint32_t ts)
{
    float dt_s = (ts - motor->encoder.last_update_ms) / 1000.0f;
    if (dt_s > 0.0f && motor->encoder.last_update_ms > 0) {
        int32_t dtick = ticks - motor->encoder.position_ticks;
        motor->encoder.speed_rpm = (float)dtick / motor->ticks_per_rev * 60.0f / dt_s / motor->gear_ratio;
    }
    motor->encoder.position_ticks = ticks;
    motor->encoder.last_update_ms = ts;
}

void dc_motor_pos_control(dc_motor_t *motor)
{
    if (!motor->enabled) return;
    float current_pos = (float)motor->encoder.position_ticks;
    float pos_output = pid_compute(&motor->pos_pid, current_pos);
    pid_set_setpoint(&motor->vel_pid, pos_output * 1000.0f);
    dc_motor_vel_control(motor);
}

void dc_motor_vel_control(dc_motor_t *motor)
{
    if (!motor->enabled) return;
    float vel_output = pid_compute(&motor->vel_pid, motor->encoder.speed_rpm);
    int pwm = (int)(fabsf(vel_output) * DC_MOTOR_MAX_PWM);
    if (pwm > DC_MOTOR_MAX_PWM) pwm = DC_MOTOR_MAX_PWM;
    motor->pwm_duty = (uint16_t)pwm;
    motor->direction = vel_output >= 0.0f ? 0 : 1;
}

void dc_motor_stop(dc_motor_t *motor)
{
    motor->pwm_duty = 0;
    pid_reset(&motor->pos_pid);
    pid_reset(&motor->vel_pid);
}

void dc_motor_enable(dc_motor_t *motor)
{
    motor->enabled = 1;
}

void dc_motor_disable(dc_motor_t *motor)
{
    motor->enabled = 0;
    motor->pwm_duty = 0;
}

void stepper_init(stepper_motor_t *stepper, stepper_driver_t driver, stepper_microstep_t ms)
{
    memset(stepper, 0, sizeof(*stepper));
    stepper->driver = driver;
    stepper->microstep = ms;
    stepper->speed_steps_per_sec = 200.0f;
    stepper->acceleration_steps_per_sec2 = 400.0f;
    stepper->direction = 0;
    stepper->enabled = 0;
}

void stepper_set_target(stepper_motor_t *stepper, int32_t steps, float speed_sps, float accel_sps2)
{
    stepper->target_steps = steps;
    stepper->speed_steps_per_sec = speed_sps;
    stepper->acceleration_steps_per_sec2 = accel_sps2;
    if (speed_sps > 0.0f) {
        stepper->step_interval_us = (uint32_t)(1000000.0f / speed_sps);
    }
}

void stepper_set_speed(stepper_motor_t *stepper, float speed_sps)
{
    stepper->speed_steps_per_sec = speed_sps;
    if (speed_sps > 0.0f) {
        stepper->step_interval_us = (uint32_t)(1000000.0f / speed_sps);
    }
}

void stepper_set_microstep(stepper_motor_t *stepper, stepper_microstep_t ms)
{
    stepper->microstep = ms;
}

uint8_t stepper_step_update(stepper_motor_t *stepper, uint32_t now_us)
{
    if (!stepper->enabled) return 0;
    if (stepper->current_steps == stepper->target_steps) return 0;

    uint32_t elapsed = now_us - stepper->last_step_us;
    if (elapsed < stepper->step_interval_us) return 0;

    if (stepper->target_steps > stepper->current_steps) {
        stepper->direction = 0;
        stepper->current_steps++;
    } else {
        stepper->direction = 1;
        stepper->current_steps--;
    }

    int32_t remaining = stepper->target_steps > stepper->current_steps ?
                        stepper->target_steps - stepper->current_steps :
                        stepper->current_steps - stepper->target_steps;

    if (remaining > 0 && stepper->acceleration_steps_per_sec2 > 1e-6f) {
        float decel_dist = (stepper->speed_steps_per_sec * stepper->speed_steps_per_sec)
                         / (2.0f * stepper->acceleration_steps_per_sec2);
        if (remaining < (int32_t)decel_dist) {
            float new_speed = sqrtf(2.0f * stepper->acceleration_steps_per_sec2 * (float)remaining);
            if (new_speed < stepper->speed_steps_per_sec) {
                stepper->step_interval_us = (uint32_t)(1000000.0f / new_speed);
            }
        }
    }

    stepper->last_step_us = now_us;
    return 1;
}

void stepper_move_to(stepper_motor_t *stepper, int32_t target, float speed_sps)
{
    stepper->target_steps = target;
    stepper->speed_steps_per_sec = speed_sps;
    if (speed_sps > 0.0f) {
        stepper->step_interval_us = (uint32_t)(1000000.0f / speed_sps);
    }
}

void stepper_enable(stepper_motor_t *stepper)
{
    stepper->enabled = 1;
    stepper->last_step_us = 0;
}

void stepper_disable(stepper_motor_t *stepper)
{
    stepper->enabled = 0;
}

int32_t stepper_remaining(const stepper_motor_t *stepper)
{
    if (stepper->target_steps > stepper->current_steps)
        return stepper->target_steps - stepper->current_steps;
    return stepper->current_steps - stepper->target_steps;
}

void servo_init(servo_t *servo, uint8_t channel, float min_deg, float max_deg,
                uint16_t min_us, uint16_t max_us)
{
    memset(servo, 0, sizeof(*servo));
    servo->channel = channel;
    servo->angle_deg = (min_deg + max_deg) * 0.5f;
    servo->target_angle_deg = servo->angle_deg;
    servo->min_angle_deg = min_deg;
    servo->max_angle_deg = max_deg;
    servo->min_pulse_us = min_us;
    servo->max_pulse_us = max_us;
    servo->speed_dps = 60.0f;
}

void servo_set_angle(servo_t *servo, float angle_deg)
{
    if (angle_deg < servo->min_angle_deg) angle_deg = servo->min_angle_deg;
    if (angle_deg > servo->max_angle_deg) angle_deg = servo->max_angle_deg;
    servo->target_angle_deg = angle_deg;
    servo->angle_deg = angle_deg;
}

void servo_sweep(servo_t *servo, float start_deg, float end_deg, float speed_dps)
{
    if (start_deg < servo->min_angle_deg) start_deg = servo->min_angle_deg;
    if (start_deg > servo->max_angle_deg) start_deg = servo->max_angle_deg;
    if (end_deg < servo->min_angle_deg) end_deg = servo->min_angle_deg;
    if (end_deg > servo->max_angle_deg) end_deg = servo->max_angle_deg;
    servo->speed_dps = speed_dps;
    servo_set_angle(servo, start_deg);

    float sweep = end_deg - start_deg;
    float dt = fabsf(sweep) / speed_dps;
    (void)dt;
}

uint16_t servo_angle_to_pulse(const servo_t *servo, float angle_deg)
{
    if (angle_deg < servo->min_angle_deg) angle_deg = servo->min_angle_deg;
    if (angle_deg > servo->max_angle_deg) angle_deg = servo->max_angle_deg;
    float range_deg = servo->max_angle_deg - servo->min_angle_deg;
    float range_us = (float)(servo->max_pulse_us - servo->min_pulse_us);
    float ratio = (angle_deg - servo->min_angle_deg) / range_deg;
    return (uint16_t)((float)servo->min_pulse_us + ratio * range_us);
}

float servo_pulse_to_angle(const servo_t *servo, uint16_t pulse_us)
{
    float range_deg = servo->max_angle_deg - servo->min_angle_deg;
    float range_us = (float)(servo->max_pulse_us - servo->min_pulse_us);
    float ratio = ((float)pulse_us - (float)servo->min_pulse_us) / range_us;
    float angle = servo->min_angle_deg + ratio * range_deg;
    if (angle < servo->min_angle_deg) angle = servo->min_angle_deg;
    if (angle > servo->max_angle_deg) angle = servo->max_angle_deg;
    return angle;
}

void servo_set_speed_control(servo_t *servo, float speed_dps)
{
    servo->speed_dps = speed_dps;
}

void s_curve_init(s_curve_trajectory_t *traj, float start, float end,
                  float max_vel, float max_acc, float jerk)
{
    memset(traj, 0, sizeof(*traj));
    traj->start_pos = start;
    traj->end_pos = end;
    traj->max_vel = max_vel;
    traj->max_acc = max_acc;
    traj->jerk = jerk;
    traj->current_pos = start;
    traj->phase = TRAJ_IDLE;
    s_curve_generate(traj);
}

void s_curve_generate(s_curve_trajectory_t *traj)
{
    float distance = traj->end_pos - traj->start_pos;
    float dir = distance >= 0.0f ? 1.0f : -1.0f;
    float total_dist = fabsf(distance);

    float t_j = traj->max_acc / traj->jerk;
    float t_a = (traj->max_vel / traj->max_acc) - t_j;
    if (t_a < 0.0f) { t_a = 0.0f; t_j = sqrtf(traj->max_vel / traj->jerk); }

    float t_acc = 2.0f * t_j + t_a;
    float d_acc = traj->max_vel * t_acc;

    if (2.0f * d_acc > total_dist) {
        traj->max_vel = sqrtf(traj->max_acc * total_dist * 0.5f);
        t_a = (traj->max_vel / traj->max_acc) - t_j;
        if (t_a < 0.0f) t_a = 0.0f;
        t_acc = 2.0f * t_j + t_a;
        d_acc = traj->max_vel * t_acc;
    }

    float t_const = (total_dist - 2.0f * d_acc) / traj->max_vel;
    traj->duration_s = 2.0f * t_acc + t_const;

    traj->point_count = 0;
    traj->points[traj->point_count].position = traj->start_pos;
    traj->points[traj->point_count].velocity = 0.0f;
    traj->points[traj->point_count].acceleration = 0.0f;
    traj->points[traj->point_count].timestamp_s = 0.0f;
    traj->point_count++;

    traj->points[traj->point_count].position = traj->end_pos;
    traj->points[traj->point_count].velocity = 0.0f;
    traj->points[traj->point_count].acceleration = 0.0f;
    traj->points[traj->point_count].timestamp_s = traj->duration_s;
    traj->point_count++;

    traj->t = 0.0f;
    traj->phase = TRAJ_ACCELERATE;
}

float s_curve_evaluate(s_curve_trajectory_t *traj, float dt)
{
    if (traj->phase == TRAJ_IDLE || traj->phase == TRAJ_COMPLETE)
        return traj->current_pos;

    traj->t += dt;
    float t = traj->t;
    float total = traj->duration_s;

    if (t >= total) {
        traj->current_pos = traj->end_pos;
        traj->current_vel = 0.0f;
        traj->phase = TRAJ_COMPLETE;
        return traj->current_pos;
    }

    float ratio = t / total;
    float smooth = ratio * ratio * (3.0f - 2.0f * ratio);

    traj->current_pos = traj->start_pos + (traj->end_pos - traj->start_pos) * smooth;
    traj->current_vel = (traj->end_pos - traj->start_pos) * 6.0f * ratio * (1.0f - ratio) / total;

    if (ratio < 0.25f) traj->phase = TRAJ_ACCELERATE;
    else if (ratio < 0.75f) traj->phase = TRAJ_CONSTANT;
    else traj->phase = TRAJ_DECELERATE;

    return traj->current_pos;
}

uint8_t s_curve_completed(const s_curve_trajectory_t *traj)
{
    return traj->phase == TRAJ_COMPLETE ? 1 : 0;
}
