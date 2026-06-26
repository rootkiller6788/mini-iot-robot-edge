#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <stdint.h>

#define PID_OUTPUT_MIN (-1.0f)
#define PID_OUTPUT_MAX ( 1.0f)
#define PID_INTEGRAL_MIN (-0.3f)
#define PID_INTEGRAL_MAX ( 0.3f)
#define S_CURVE_MAX_PHASES 7
#define SERVO_MIN_ANGLE_DEG 0.0f
#define SERVO_MAX_ANGLE_DEG 180.0f
#define SERVO_MIN_PULSE_US 500
#define SERVO_MAX_PULSE_US 2500
#define STEPPER_MAX_SPEED_STEPS 4000
#define DC_MOTOR_MAX_PWM 255

typedef struct {
    float kp, ki, kd;
    float setpoint;
    float integral;
    float prev_error;
    float output;
    float integral_min, integral_max;
    float output_min, output_max;
    float dt;
} pid_controller_t;

typedef enum {
    DC_MOTOR_DRIVER_L298N = 0
} dc_driver_type_t;

typedef struct {
    int32_t position_ticks;
    float speed_rpm;
    float current_ma;
    uint32_t last_update_ms;
} dc_encoder_t;

typedef struct {
    dc_driver_type_t driver;
    dc_encoder_t encoder;
    pid_controller_t pos_pid;
    pid_controller_t vel_pid;
    float gear_ratio;
    uint16_t ticks_per_rev;
    uint16_t pwm_duty;
    uint8_t direction;
    uint8_t enabled;
} dc_motor_t;

typedef enum {
    STEPPER_DRIVER_A4988  = 0,
    STEPPER_DRIVER_TMC2209 = 1
} stepper_driver_t;

typedef enum {
    STEPPER_FULL     = 0,
    STEPPER_HALF     = 1,
    STEPPER_QUARTER  = 2,
    STEPPER_EIGHTH   = 3,
    STEPPER_SIXTEENTH = 4
} stepper_microstep_t;

typedef struct {
    stepper_driver_t driver;
    stepper_microstep_t microstep;
    int32_t target_steps;
    int32_t current_steps;
    float speed_steps_per_sec;
    float acceleration_steps_per_sec2;
    uint8_t direction;
    uint8_t enabled;
    uint32_t step_interval_us;
    uint32_t last_step_us;
} stepper_motor_t;

typedef struct {
    float angle_deg;
    float speed_dps;
    float target_angle_deg;
    float min_angle_deg;
    float max_angle_deg;
    uint16_t min_pulse_us;
    uint16_t max_pulse_us;
    uint8_t channel;
} servo_t;

typedef enum {
    TRAJ_IDLE       = 0,
    TRAJ_ACCELERATE = 1,
    TRAJ_CONSTANT   = 2,
    TRAJ_DECELERATE = 3,
    TRAJ_COMPLETE   = 4
} traj_phase_t;

typedef struct {
    float position;
    float velocity;
    float acceleration;
    float timestamp_s;
} traj_point_t;

typedef struct {
    traj_point_t points[16];
    uint8_t point_count;
    traj_phase_t phase;
    float jerk;
    float t;
    float duration_s;
    float start_pos;
    float end_pos;
    float max_vel;
    float max_acc;
    float current_pos;
    float current_vel;
} s_curve_trajectory_t;

void pid_init(pid_controller_t *pid, float kp, float ki, float kd, float dt);
void pid_set_limits(pid_controller_t *pid, float out_min, float out_max, float int_min, float int_max);
float pid_compute(pid_controller_t *pid, float measurement);
void pid_reset(pid_controller_t *pid);
void pid_set_setpoint(pid_controller_t *pid, float setpoint);
void pid_set_gains(pid_controller_t *pid, float kp, float ki, float kd);

void dc_motor_init(dc_motor_t *motor, dc_driver_type_t driver, float gear_ratio, uint16_t tpr);
void dc_motor_set_pwm(dc_motor_t *motor, uint16_t pwm, uint8_t dir);
void dc_motor_set_speed_rpm(dc_motor_t *motor, float rpm);
void dc_motor_update_encoder(dc_motor_t *motor, int32_t ticks, uint32_t ts);
void dc_motor_pos_control(dc_motor_t *motor);
void dc_motor_vel_control(dc_motor_t *motor);
void dc_motor_stop(dc_motor_t *motor);
void dc_motor_enable(dc_motor_t *motor);
void dc_motor_disable(dc_motor_t *motor);

void stepper_init(stepper_motor_t *stepper, stepper_driver_t driver, stepper_microstep_t ms);
void stepper_set_target(stepper_motor_t *stepper, int32_t steps, float speed_sps, float accel_sps2);
void stepper_set_speed(stepper_motor_t *stepper, float speed_sps);
void stepper_set_microstep(stepper_motor_t *stepper, stepper_microstep_t ms);
uint8_t stepper_step_update(stepper_motor_t *stepper, uint32_t now_us);
void stepper_move_to(stepper_motor_t *stepper, int32_t target, float speed_sps);
void stepper_enable(stepper_motor_t *stepper);
void stepper_disable(stepper_motor_t *stepper);
int32_t stepper_remaining(const stepper_motor_t *stepper);

void servo_init(servo_t *servo, uint8_t channel, float min_deg, float max_deg, uint16_t min_us, uint16_t max_us);
void servo_set_angle(servo_t *servo, float angle_deg);
void servo_sweep(servo_t *servo, float start_deg, float end_deg, float speed_dps);
uint16_t servo_angle_to_pulse(const servo_t *servo, float angle_deg);
float servo_pulse_to_angle(const servo_t *servo, uint16_t pulse_us);
void servo_set_speed_control(servo_t *servo, float speed_dps);

void s_curve_init(s_curve_trajectory_t *traj, float start, float end, float max_vel, float max_acc, float jerk);
float s_curve_evaluate(s_curve_trajectory_t *traj, float dt);
uint8_t s_curve_completed(const s_curve_trajectory_t *traj);
void s_curve_generate(s_curve_trajectory_t *traj);

#endif
