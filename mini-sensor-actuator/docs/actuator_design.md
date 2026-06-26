# Actuator Design Patterns — mini-sensor-actuator

Design patterns and best practices for motor control and actuation in embedded robotics.

## Actuator Overview

### Actuator Types Supported

| Type | Control | Feedback | Typical Use | Precision |
|------|---------|----------|-------------|-----------|
| DC Motor (brushed) | PWM duty cycle + H-bridge direction | Encoder (position/speed) | Wheels, conveyors, fans | Medium |
| DC Motor (brushless) | 3-phase commutation (ESC) | Hall sensors / back-EMF | Drones, high-speed spindles | High |
| Stepper Motor | Step/Dir pulses | None (open-loop) or encoder (closed-loop) | 3D printers, CNC, camera gimbals | Very High |
| Servo Motor | PWM pulse width (500-2500µs) | Internal pot (angle) | Steering, robotic arms, grippers | Low-Medium |
| Linear Actuator | H-bridge direction | Limit switches, potentiometer | Push/pull mechanisms | Medium |
| Solenoid | GPIO on/off | None | Valves, latches, percussive | Binary |
| Piezo Actuator | High-voltage waveform | None | Precision positioning, haptics | Sub-micron |

## PID Control Theory

### PID Equation

```
u(t) = Kp * e(t) + Ki * ∫e(t)dt + Kd * de(t)/dt

Where:
  u(t)  = control output
  e(t)  = error = setpoint - measurement
  Kp    = proportional gain
  Ki    = integral gain
  Kd    = derivative gain
```

### Discrete Form (Implemented)

```
error = setpoint - measurement
integral += error * dt
derivative = (error - prev_error) / dt
output = Kp*error + Ki*integral + Kd*derivative

Anti-windup: clamp integral to [integral_min, integral_max]
Output limiting: clamp output to [output_min, output_max]
```

### PID Parameter Effects

| Parameter | Rise Time | Overshoot | Settling Time | Steady-State Error | Stability |
|-----------|-----------|-----------|---------------|--------------------|-----------|
| Increase Kp | Decrease | Increase | Small change | Decrease | Degrade |
| Increase Ki | Decrease | Increase | Increase | Eliminate | Degrade |
| Increase Kd | Small change | Decrease | Decrease | No effect | Improve (if small) |

### Tuning Methods

#### 1. Manual Tuning (Ziegler-Nichols approximation)

```
Step 1: Set Ki = 0, Kd = 0
Step 2: Increase Kp until system oscillates with constant amplitude
Step 3: Record critical gain (Ku) and oscillation period (Tu)
Step 4: Apply tuning rules:

P-only:   Kp = 0.5 * Ku
PI:       Kp = 0.45 * Ku,    Ki = 0.54 * Ku / Tu
PID:      Kp = 0.6 * Ku,     Ki = 1.2 * Ku / Tu,    Kd = 0.075 * Ku * Tu
```

#### 2. Iterative Tuning Procedure

```c
void pid_tune_iterative(pid_controller_t *pid, float target, int iterations)
{
    pid_init(pid, 0.0f, 0.0f, 0.0f, 0.01f);
    pid_set_setpoint(pid, target);

    /* Step 1: Find Kp */
    float kp = 0.1f;
    while (kp < 10.0f) {
        pid_set_gains(pid, kp, 0.0f, 0.0f);
        float response = measure_step_response(pid);
        if (overshoot_too_large(response)) break;
        kp *= 1.5f;
    }

    /* Step 2: Add Kd to dampen */
    float kd = kp * 0.1f;
    pid_set_gains(pid, kp, 0.0f, kd);

    /* Step 3: Add Ki to eliminate steady-state error */
    float ki = kp * 0.01f;
    pid_set_gains(pid, kp, ki, kd);
}
```

## DC Motor Patterns

### H-Bridge Control (L298N / L293D)

```
+-----------+     Motor
|   L298N   |    +------+
|           |    |      |
|  OUT1 ----+----+      +----+
|           |              |
|  OUT2 ----+--------------+
|           |
|  ENA  ---- PWM Speed Control
|  IN1  ---- Direction Bit 0
|  IN2  ---- Direction Bit 1
+-----------+

Truth Table:
  IN1=0, IN2=0  ->  Motor STOP (coast)
  IN1=1, IN2=0  ->  Forward
  IN1=0, IN2=1  ->  Reverse
  IN1=1, IN2=1  ->  Motor BRAKE (short)
```

### DC Motor Speed Control Loop

```
Target Speed (RPM)
      |
      v
+--------------+
| Velocity PID |---> PWM Duty ----+
+--------------+                   |
      ^                            v
      |                    +---------------+
      |                    |  H-Bridge     |
      +--- Encoder RPM <---+  + Motor     |
                           +-------+-------+
                                   |
                               Encoder
```

### Differential Drive Pattern

```c
typedef struct {
    dc_motor_t left;
    dc_motor_t right;
    float wheel_radius_mm;
    float wheel_base_mm;
    float linear_velocity_mms;
    float angular_velocity_rads;
} diff_drive_t;

void diff_drive_set_velocity(diff_drive_t *dd, float linear_mms, float angular_rads)
{
    float left_rpm  = (linear_mms - angular_rads * dd->wheel_base_mm * 0.5f)
                      / (2.0f * M_PI * dd->wheel_radius_mm) * 60.0f;
    float right_rpm = (linear_mms + angular_rads * dd->wheel_base_mm * 0.5f)
                      / (2.0f * M_PI * dd->wheel_radius_mm) * 60.0f;

    dc_motor_set_speed_rpm(&dd->left, left_rpm);
    dc_motor_set_speed_rpm(&dd->right, right_rpm);

    dc_motor_vel_control(&dd->left);
    dc_motor_vel_control(&dd->right);
}
```

## Stepper Motor Patterns

### Driver Comparison

| Driver | Max Current | Microstepping | Stall Detection | Interface | Notes |
|--------|------------|---------------|-----------------|-----------|-------|
| A4988 | 2A | Full, 1/2, 1/4, 1/8, 1/16 | No | Step/Dir | Basic, cheap |
| DRV8825 | 2.5A | Up to 1/32 | No | Step/Dir | Higher microstep |
| TMC2209 | 2A | Up to 1/256 | Yes (StallGuard) | Step/Dir + UART | Silent, sensorless homing |
| TMC5160 | 10A | Up to 1/256 | Yes (StallGuard2) | SPI + Step/Dir | High power, industrial |

### Microstepping Resolution

| Microstep | Steps/Rev (200 step motor) | Angular Resolution | Torque |
|-----------|---------------------------|--------------------|--------|
| Full | 200 | 1.8° | 100% |
| 1/2 | 400 | 0.9° | 71% |
| 1/4 | 800 | 0.45° | 38% |
| 1/8 | 1600 | 0.225° | 20% |
| 1/16 | 3200 | 0.1125° | 10% |
| 1/256 | 51200 | 0.007° | ~1% |

### Stepper Speed Profile (Trapezoidal)

```
Speed ^
      |      +-----------+
      |     /|           |\
      |    / |           | \
      |   /  |           |  \
      |  /   |           |   \
      | /    |           |    \
      |/     |           |     \
      +------+-----------+------+---> Time
       Accel  Constant   Decel

t1: Acceleration time (from 0 to max_speed)
t2: Constant speed time
t3: Deceleration time (from max_speed to 0)
```

```c
void stepper_move_trapezoidal(stepper_motor_t *m, int32_t target, float max_sps, float accel)
{
    int32_t distance = target - m->current_steps;
    float accel_time = max_sps / accel;
    float accel_dist = 0.5f * accel * accel_time * accel_time;

    if (2.0f * accel_dist > fabsf((float)distance)) {
        float peak = sqrtf(fabsf((float)distance) * accel);
        accel_time = peak / accel;
        max_sps = peak;
    }

    m->target_steps = target;
    m->speed_steps_per_sec = max_sps;
    m->acceleration_steps_per_sec2 = accel;
    m->direction = (distance > 0) ? 0 : 1;
}
```

### Sensorless Homing with TMC2209/5160

```c
uint8_t stepper_home_tmc(stepper_motor_t *m)
{
    uint32_t stall_threshold = 100;
    while (1) {
        if (stepper_step_update(m, get_micros()) == 0) continue;
        uint32_t sg_result = tmc_read_stallguard();
        if (sg_result < stall_threshold) {
            m->current_steps = 0;
            return 1;
        }
    }
    return 0;
}
```

## Servo Motor Patterns

### PWM Mapping

```
Pulse Width | Servo Position
------------+---------------
  500 µs    |  0° (min)
 1000 µs    |  45°
 1500 µs    |  90° (center)
 2000 µs    |  135°
 2500 µs    |  180° (max)

Standard range: 1000-2000 µs (some servos can use 500-2500 µs)
PWM frequency: 50 Hz (20ms period) for standard analog servos
Digital servos can accept up to 333 Hz (3ms period)
```

### Servo Speed Control (Software Ramp)

```c
void servo_move_smooth(servo_t *servo, float target_deg)
{
    if (target_deg < servo->min_angle_deg) target_deg = servo->min_angle_deg;
    if (target_deg > servo->max_angle_deg) target_deg = servo->max_angle_deg;

    float step = servo->speed_dps * 0.02f;  /* 50Hz update */
    float diff = target_deg - servo->angle_deg;

    if (fabsf(diff) <= step) {
        servo->angle_deg = target_deg;
    } else {
        servo->angle_deg += (diff > 0) ? step : -step;
    }

    uint16_t pulse = servo_angle_to_pulse(servo, servo->angle_deg);
    pwm_set_pulse(servo->channel, pulse);
}
```

## S-Curve Trajectory Planning

An S-curve trajectory avoids infinite jerk (the derivative of acceleration), resulting in smoother motion and less mechanical stress.

### Mathematical Formulation

```
Position:  p(t) = p0 + (p1-p0) * s(t/T)
           where s(τ) = τ² * (3 - 2τ)  (smoothstep)

Velocity:  v(t) = (p1-p0) * 6τ(1-τ) / T

Acceleration: a(t) = (p1-p0) * 6(1-2τ) / T²

Jerk:      j(t) = (p1-p0) * (-12) / T³  (constant magnitude)

Where:
  τ = t / T (normalized time, 0 to 1)
  T = total trajectory duration
  p0 = start position, p1 = end position
```

### Comparison: Trapezoidal vs S-Curve

| Property | Trapezoidal | S-Curve |
|----------|-------------|---------|
| Jerk | Infinite (at transitions) | Finite, constant |
| Vibration | Higher | Lower |
| Settling time | Shorter (for same max accel) | Longer (for same max accel) |
| Positioning accuracy | Good | Excellent |
| Mechanical wear | Higher | Lower |
| Computational load | Low | Medium |

### Implementation Notes

```c
s_curve_trajectory_t traj;
s_curve_init(&traj, 0.0f, 100.0f, 50.0f, 200.0f, 1000.0f);

while (!s_curve_completed(&traj)) {
    float pos = s_curve_evaluate(&traj, dt);
    dc_motor_pos_control_with_target(&motor, pos);
    delay_ms(10);
}
```

## Advanced Patterns

### Cascade Control (Position + Velocity)

```
                     +-----------+
Position SP --------->| Pos PID  |---> Velocity SP
                     +-----+-----+
                           |
                     +-----v-----+
Velocity FB -------------->| Vel PID  |---> PWM Output
                     +-----+-----+
                           |
                     +-----v-----+
                     |   Motor   |
                     +-----------+
```

```c
void motor_cascade_control(dc_motor_t *motor, float target_pos, float max_vel)
{
    float pos_error = target_pos - motor->encoder.position_ticks;
    float vel_sp = clamp(pos_error * motor->pos_pid.kp, -max_vel, max_vel);
    pid_set_setpoint(&motor->vel_pid, vel_sp);
    dc_motor_vel_control(motor);
}
```

### Feedforward + PID

Adding feedforward improves tracking of known or predicted motion profiles.

```c
float motor_control_ff_pid(dc_motor_t *motor, float target_vel, float estimated_load)
{
    float ff_torque = estimated_load * motor->gear_ratio;
    float ff_pwm = ff_torque / motor->max_torque * DC_MOTOR_MAX_PWM;
    float pid_output = pid_compute(&motor->vel_pid, motor->encoder.speed_rpm);
    return ff_pwm + pid_output * DC_MOTOR_MAX_PWM;
}
```

### Multi-Axis Coordination

For CNC or robotic arm applications where multiple axes must move together:

```c
typedef struct {
    stepper_motor_t *axes[4];
    int32_t targets[4];
    float speeds[4];
    uint8_t num_axes;
} coordinated_move_t;

void coordinated_move_start(coordinated_move_t *cm)
{
    int32_t max_dist = 0;
    uint8_t i;
    for (i = 0; i < cm->num_axes; i++) {
        int32_t dist = cm->targets[i] - cm->axes[i]->current_steps;
        if (labs(dist) > max_dist) max_dist = labs(dist);
    }
    for (i = 0; i < cm->num_axes; i++) {
        float ratio = fabsf((float)labs(cm->targets[i] - cm->axes[i]->current_steps)) / (float)max_dist;
        stepper_set_target(cm->axes[i], cm->targets[i],
                          cm->speeds[i] * ratio,
                          cm->axes[i]->acceleration_steps_per_sec2 * ratio);
    }
}
```

## Safety Patterns

### Emergency Stop

```c
volatile uint8_t e_stop = 0;

void emergency_stop_handler(dc_motor_t *motors, uint8_t count)
{
    uint8_t i;
    for (i = 0; i < count; i++) {
        motors[i].pwm_duty = 0;
        motors[i].enabled = 0;
    }
    e_stop = 1;
}
```

### Watchdog Timer

```c
void motor_watchdog(dc_motor_t *motor, uint32_t timeout_ms)
{
    static uint32_t last_cmd_ms = 0;
    if (get_time_ms() - last_cmd_ms > timeout_ms) {
        dc_motor_stop(motor);
    }
}
```

### Soft Limits

```c
uint8_t motor_check_limits(stepper_motor_t *stepper, int32_t min_pos, int32_t max_pos)
{
    if (stepper->current_steps < min_pos || stepper->current_steps > max_pos) {
        stepper_disable(stepper);
        return 0;
    }
    return 1;
}
```

## Power Electronics Tips

### Motor Driver Selection

| Current | Type | Recommended Driver |
|---------|------|--------------------|
| <2A | Brushed DC | L298N, TB6612FNG |
| 2-5A | Brushed DC | BTS7960, Cytron MDD10A |
| 5-20A | Brushed DC | VNH5019, Sabertooth |
| 20A+ | Brushed DC | RoboClaw, ODrive (also BLDC) |

### Flyback Diodes

Always use flyback diodes across motor terminals when using H-bridges without built-in protection. Schottky diodes (1N5819) for low voltage, fast recovery for higher voltages.

### Bulk Capacitance

Add electrolytic capacitors near motor power input:
- 100-470 µF per 1A of motor current
- Low ESR ceramic (0.1 µF) in parallel for high-frequency noise

### EMI Mitigation

1. Twist motor power wires together
2. Use ferrite beads on signal lines near the motor
3. Separate power and signal grounds (single-point connection)
4. Keep PWM frequency above 20 kHz (outside audible range)
5. Use shielded encoder cables with shield grounded at one end only
