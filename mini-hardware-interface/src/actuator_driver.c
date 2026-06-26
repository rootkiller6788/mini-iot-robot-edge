#include "actuator_driver.h"
#include "gpio_pwm_adc.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ─── Global Emergency State ────────────────────────────────────────── */

static bool s_global_estop = false;

void mhi_emergency_stop_all(void)  { s_global_estop = true; }
void mhi_emergency_resume_all(void) { s_global_estop = false; }

/* ─── Brushed DC Motor ──────────────────────────────────────────────── */

int mhi_dc_motor_init(mhi_dc_motor_t *motor, uint8_t pwm_ch,
                      uint8_t in1, uint8_t in2) {
    if (!motor) return -1;
    motor->pwm_channel    = pwm_ch;
    motor->in1_pin        = in1;
    motor->in2_pin        = in2;
    motor->enable_pin     = 255u;
    motor->max_current_a  = 5.0f;
    motor->current_limit_a = 2.0f;
    motor->current_speed  = 0.0f;
    motor->direction      = MHI_DC_DIR_COAST;
    motor->soft_start_ramp_s = 0.5f;
    motor->ramp_start_ms  = 0u;
    motor->emergency_stop = false;
    motor->enabled        = false;

    mhi_gpio_init(in1, MHI_GPIO_MODE_OUTPUT);
    mhi_gpio_init(in2, MHI_GPIO_MODE_OUTPUT);
    mhi_pwm_init(pwm_ch, 1000.0f, MHI_PWM_RES_8BIT);
    return 0;
}

void mhi_dc_motor_deinit(mhi_dc_motor_t *motor) {
    if (!motor) return;
    mhi_pwm_deinit(motor->pwm_channel);
    mhi_gpio_deinit(motor->in1_pin);
    mhi_gpio_deinit(motor->in2_pin);
}

int mhi_dc_motor_set_speed(mhi_dc_motor_t *motor, float speed) {
    if (!motor || motor->emergency_stop || s_global_estop) return -1;
    if (speed < -1.0f) speed = -1.0f;
    if (speed >  1.0f) speed =  1.0f;

    motor->current_speed = speed;
    motor->enabled = (fabsf(speed) > 0.001f);
    if (motor->enabled) motor->ramp_start_ms = 0u;

    if (speed >= 0.0f)
        motor->direction = MHI_DC_DIR_CW;
    else
        motor->direction = MHI_DC_DIR_CCW;

    return 0;
}

int mhi_dc_motor_set_direction(mhi_dc_motor_t *motor, mhi_dc_direction_t dir) {
    if (!motor) return -1;
    motor->direction = dir;
    return 0;
}

int mhi_dc_motor_brake(mhi_dc_motor_t *motor) {
    if (!motor) return -1;
    motor->direction = MHI_DC_DIR_BRAKE;
    mhi_gpio_write(motor->in1_pin, MHI_GPIO_HIGH);
    mhi_gpio_write(motor->in2_pin, MHI_GPIO_HIGH);
    mhi_pwm_stop(motor->pwm_channel);
    motor->current_speed = 0.0f;
    motor->enabled = false;
    return 0;
}

int mhi_dc_motor_coast(mhi_dc_motor_t *motor) {
    if (!motor) return -1;
    motor->direction = MHI_DC_DIR_COAST;
    mhi_gpio_write(motor->in1_pin, MHI_GPIO_LOW);
    mhi_gpio_write(motor->in2_pin, MHI_GPIO_LOW);
    mhi_pwm_stop(motor->pwm_channel);
    motor->current_speed = 0.0f;
    motor->enabled = false;
    return 0;
}

void mhi_dc_motor_emergency_stop(mhi_dc_motor_t *motor) {
    if (!motor) return;
    motor->emergency_stop = true;
    mhi_gpio_write(motor->in1_pin, MHI_GPIO_LOW);
    mhi_gpio_write(motor->in2_pin, MHI_GPIO_LOW);
    mhi_pwm_stop(motor->pwm_channel);
    motor->current_speed = 0.0f;
    motor->enabled = false;
}

void mhi_dc_motor_resume(mhi_dc_motor_t *motor) {
    if (!motor) return;
    motor->emergency_stop = false;
    motor->ramp_start_ms = 0u;
}

void mhi_dc_motor_tick(mhi_dc_motor_t *motor, uint32_t now_ms) {
    if (!motor || !motor->enabled
        || motor->emergency_stop || s_global_estop) return;

    if (motor->ramp_start_ms == 0u)
        motor->ramp_start_ms = now_ms;

    float elapsed_s = (float)(now_ms - motor->ramp_start_ms) * 0.001f;
    float ramp_factor = 1.0f;
    if (motor->soft_start_ramp_s > 0.0f) {
        ramp_factor = elapsed_s / motor->soft_start_ramp_s;
        if (ramp_factor > 1.0f) ramp_factor = 1.0f;
    }

    float abs_speed = fabsf(motor->current_speed) * ramp_factor;
    float duty = abs_speed * 100.0f;

    if (motor->direction == MHI_DC_DIR_CW) {
        mhi_gpio_write(motor->in1_pin, MHI_GPIO_HIGH);
        mhi_gpio_write(motor->in2_pin, MHI_GPIO_LOW);
    } else if (motor->direction == MHI_DC_DIR_CCW) {
        mhi_gpio_write(motor->in1_pin, MHI_GPIO_LOW);
        mhi_gpio_write(motor->in2_pin, MHI_GPIO_HIGH);
    }

    mhi_pwm_set_duty(motor->pwm_channel, duty);
    if (duty > 0.0f)
        mhi_pwm_start(motor->pwm_channel);
    else
        mhi_pwm_stop(motor->pwm_channel);
}

/* ─── BLDC: 6-step commutation table (hall → phase output) ──────────── */

typedef struct {
    uint8_t uh; uint8_t ul;
    uint8_t vh; uint8_t vl;
    uint8_t wh; uint8_t wl;
} mhi_bldc_step_t;

static const mhi_bldc_step_t s_bldc_table[8] = {
    { 0,0, 0,0, 0,0 }, /* 000: invalid */
    { 1,0, 0,0, 0,1 }, /* 001: UH, WL */
    { 0,0, 1,0, 0,1 }, /* 010: VH, WL */
    { 0,1, 1,0, 0,0 }, /* 011: VH, UL */
    { 0,0, 0,0, 1,0 }, /* 100: WH, VL (placeholder, real table varies) */
    { 0,1, 0,0, 1,0 }, /* 101: WH, UL */
    { 1,0, 0,1, 0,0 }, /* 110: UH, VL */
    { 0,0, 0,0, 0,0 }, /* 111: invalid */
};

int mhi_bldc_motor_init(mhi_bldc_motor_t *motor, const mhi_bldc_pins_t *pins) {
    if (!motor || !pins) return -1;
    motor->pins           = *pins;
    motor->target_speed   = 0.0f;
    motor->current_speed  = 0.0f;
    motor->duty_cycle     = 0.0f;
    motor->max_current_a  = 10.0f;
    motor->hall_state     = 0u;
    motor->commutation_step = 0u;
    motor->enabled        = false;
    motor->emergency_stop = false;
    motor->soft_start_ms  = 500u;
    motor->ramp_start_ms  = 0u;
    return 0;
}

void mhi_bldc_motor_deinit(mhi_bldc_motor_t *motor) {
    if (!motor) return;
    motor->enabled = false;
}

int mhi_bldc_motor_set_speed(mhi_bldc_motor_t *motor, float speed) {
    if (!motor || motor->emergency_stop || s_global_estop) return -1;
    if (speed < -1.0f) speed = -1.0f;
    if (speed >  1.0f) speed =  1.0f;
    motor->target_speed  = speed;
    motor->duty_cycle    = fabsf(speed);
    motor->enabled       = (motor->duty_cycle > 0.001f);
    if (motor->enabled) motor->ramp_start_ms = 0u;
    return 0;
}

int mhi_bldc_motor_set_duty(mhi_bldc_motor_t *motor, float duty) {
    if (!motor) return -1;
    if (duty < 0.0f) duty = 0.0f;
    if (duty > 1.0f) duty = 1.0f;
    motor->duty_cycle = duty;
    motor->enabled = (duty > 0.001f);
    return 0;
}

void mhi_bldc_motor_commutate(mhi_bldc_motor_t *motor) {
    if (!motor || !motor->enabled
        || motor->emergency_stop || s_global_estop) return;

    uint8_t hall = motor->hall_state & 0x07u;
    const mhi_bldc_step_t *step = &s_bldc_table[hall];

    mhi_gpio_write(motor->pins.pwm_uh, (mhi_gpio_level_t)step->uh);
    mhi_gpio_write(motor->pins.pwm_ul, (mhi_gpio_level_t)step->ul);
    mhi_gpio_write(motor->pins.pwm_vh, (mhi_gpio_level_t)step->vh);
    mhi_gpio_write(motor->pins.pwm_vl, (mhi_gpio_level_t)step->vl);
    mhi_gpio_write(motor->pins.pwm_wh, (mhi_gpio_level_t)step->wh);
    mhi_gpio_write(motor->pins.pwm_wl, (mhi_gpio_level_t)step->wl);
}

void mhi_bldc_motor_emergency_stop(mhi_bldc_motor_t *motor) {
    if (!motor) return;
    motor->emergency_stop = true;
    motor->enabled = false;
    mhi_gpio_write(motor->pins.pwm_uh, MHI_GPIO_LOW);
    mhi_gpio_write(motor->pins.pwm_ul, MHI_GPIO_LOW);
    mhi_gpio_write(motor->pins.pwm_vh, MHI_GPIO_LOW);
    mhi_gpio_write(motor->pins.pwm_vl, MHI_GPIO_LOW);
    mhi_gpio_write(motor->pins.pwm_wh, MHI_GPIO_LOW);
    mhi_gpio_write(motor->pins.pwm_wl, MHI_GPIO_LOW);
}

void mhi_bldc_motor_tick(mhi_bldc_motor_t *motor, uint32_t now_ms) {
    if (!motor || !motor->enabled
        || motor->emergency_stop || s_global_estop) return;

    if (motor->ramp_start_ms == 0u)
        motor->ramp_start_ms = now_ms;

    float elapsed_s = (float)(now_ms - motor->ramp_start_ms) * 0.001f;
    float ramp_s = (float)motor->soft_start_ms * 0.001f;
    float ramp_factor = 1.0f;
    if (ramp_s > 0.0f) {
        ramp_factor = elapsed_s / ramp_s;
        if (ramp_factor > 1.0f) ramp_factor = 1.0f;
    }
    motor->current_speed = motor->target_speed * ramp_factor;

    /* hall sensors read would happen here on real hardware */
    uint8_t ha = mhi_gpio_read(motor->pins.hall_a);
    uint8_t hb = mhi_gpio_read(motor->pins.hall_b);
    uint8_t hc = mhi_gpio_read(motor->pins.hall_c);
    motor->hall_state = (uint8_t)(ha | (uint8_t)(hb << 1u)
                                    | (uint8_t)(hc << 2u));

    mhi_bldc_motor_commutate(motor);
}

/* ─── Stepper Motor ─────────────────────────────────────────────────── */

static const uint8_t s_step_full[8][4] = {
    {1,0,1,0}, {1,0,0,1}, {0,1,0,1}, {0,1,1,0},
    {0,1,1,0}, {0,1,0,1}, {1,0,0,1}, {1,0,1,0}
};

static const uint8_t s_step_half[8][4] = {
    {1,0,0,0}, {1,0,1,0}, {0,0,1,0}, {0,1,1,0},
    {0,1,0,0}, {0,1,0,1}, {0,0,0,1}, {1,0,0,1}
};

int mhi_stepper_motor_init(mhi_stepper_motor_t *motor,
                           const mhi_stepper_pins_t *pins,
                           uint16_t steps_per_rev) {
    if (!motor || !pins) return -1;
    motor->pins                = *pins;
    motor->mode                = MHI_STEP_MODE_FULL;
    motor->position            = 0;
    motor->target_position     = 0;
    motor->speed_steps_per_s   = 100.0f;
    motor->step_interval_us    = 10000u;
    motor->micro_step_index    = 0u;
    motor->steps_per_revolution = steps_per_rev;
    motor->enabled             = false;
    motor->emergency_stop      = false;
    motor->last_step_us        = 0u;
    return 0;
}

void mhi_stepper_motor_deinit(mhi_stepper_motor_t *motor) {
    if (!motor) return;
    motor->enabled = false;
}

int mhi_stepper_motor_set_mode(mhi_stepper_motor_t *motor, mhi_step_mode_t mode) {
    if (!motor) return -1;
    motor->mode = mode;
    return 0;
}

int mhi_stepper_motor_move_to(mhi_stepper_motor_t *motor, int32_t position) {
    if (!motor) return -1;
    motor->target_position = position;
    motor->enabled = true;
    return 0;
}

int mhi_stepper_motor_move_steps(mhi_stepper_motor_t *motor, int32_t steps) {
    if (!motor) return -1;
    motor->target_position = motor->position + steps;
    motor->enabled = true;
    return 0;
}

int mhi_stepper_motor_set_speed(mhi_stepper_motor_t *motor, float steps_per_s) {
    if (!motor || steps_per_s <= 0.0f) return -1;
    motor->speed_steps_per_s = steps_per_s;
    motor->step_interval_us = (uint32_t)(1000000.0f / steps_per_s);
    return 0;
}

void mhi_stepper_motor_emergency_stop(mhi_stepper_motor_t *motor) {
    if (!motor) return;
    motor->emergency_stop = true;
    motor->enabled = false;
}

void mhi_stepper_motor_tick(mhi_stepper_motor_t *motor, uint32_t now_us) {
    if (!motor || !motor->enabled
        || motor->emergency_stop || s_global_estop) return;
    if (motor->position == motor->target_position) {
        motor->enabled = false;
        return;
    }

    if (now_us - motor->last_step_us < motor->step_interval_us) return;
    motor->last_step_us = now_us;

    int32_t direction = (motor->target_position > motor->position) ? 1 : -1;
    motor->position += direction;

    uint8_t step_idx = (uint8_t)(motor->position & 0x07u);
    const uint8_t (*table)[4];
    if (motor->mode == MHI_STEP_MODE_FULL)
        table = s_step_full;
    else
        table = s_step_half;

    mhi_gpio_write(motor->pins.coil_a1, (mhi_gpio_level_t)table[step_idx][0]);
    mhi_gpio_write(motor->pins.coil_a2, (mhi_gpio_level_t)table[step_idx][1]);
    mhi_gpio_write(motor->pins.coil_b1, (mhi_gpio_level_t)table[step_idx][2]);
    mhi_gpio_write(motor->pins.coil_b2, (mhi_gpio_level_t)table[step_idx][3]);
}

/* ─── Servo Motor ───────────────────────────────────────────────────── */

int mhi_servo_motor_init(mhi_servo_motor_t *servo, uint8_t pwm_ch) {
    if (!servo) return -1;
    servo->pwm_channel   = pwm_ch;
    servo->pulse_min_us  = 500u;
    servo->pulse_max_us  = 2500u;
    servo->angle_min_deg = 0.0f;
    servo->angle_max_deg = 180.0f;
    servo->current_angle = 90.0f;
    servo->enabled       = true;
    return 0;
}

int mhi_servo_motor_set_angle(mhi_servo_motor_t *servo, float angle_deg) {
    if (!servo || !servo->enabled) return -1;
    if (angle_deg < servo->angle_min_deg) angle_deg = servo->angle_min_deg;
    if (angle_deg > servo->angle_max_deg) angle_deg = servo->angle_max_deg;

    float pulse_us = servo->pulse_min_us +
        (angle_deg - servo->angle_min_deg) /
        (servo->angle_max_deg - servo->angle_min_deg) *
        (float)(servo->pulse_max_us - servo->pulse_min_us);

    mhi_servo_config_t cfg = {
        .pwm_channel  = servo->pwm_channel,
        .pulse_min_us = servo->pulse_min_us,
        .pulse_max_us = servo->pulse_max_us,
        .angle_min_deg = servo->angle_min_deg,
        .angle_max_deg = servo->angle_max_deg,
        .period_us    = 20000u
    };
    mhi_servo_init(&cfg);
    servo->current_angle = angle_deg;
    return mhi_servo_set_angle(servo->pwm_channel, angle_deg);
}

float mhi_servo_motor_get_angle(const mhi_servo_motor_t *servo) {
    return servo ? servo->current_angle : 0.0f;
}

int mhi_servo_motor_calibrate(mhi_servo_motor_t *servo,
                              uint16_t min_us, uint16_t max_us,
                              float min_deg, float max_deg) {
    if (!servo) return -1;
    servo->pulse_min_us  = min_us;
    servo->pulse_max_us  = max_us;
    servo->angle_min_deg = min_deg;
    servo->angle_max_deg = max_deg;
    return 0;
}

void mhi_servo_motor_disable(mhi_servo_motor_t *servo) {
    if (servo) {
        servo->enabled = false;
        mhi_pwm_stop(servo->pwm_channel);
    }
}
