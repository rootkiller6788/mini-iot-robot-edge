#include "gpio_pwm_adc.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ─── Platform-Shim: pin state storage ──────────────────────────────── */

#define MHI_MAX_PINS    64
#define MHI_MAX_PWM_CH  16
#define MHI_MAX_ADC_CH  16

typedef struct {
    mhi_gpio_mode_t mode;
    mhi_gpio_level_t level;
    mhi_gpio_callback_t irq_cb;
    mhi_gpio_irq_t irq;
    void *irq_user;
    bool in_use;
} mhi_pin_state_t;

typedef struct {
    float duty_percent;
    float frequency_hz;
    mhi_pwm_resolution_t resolution;
    uint16_t period_ticks;
    uint16_t duty_ticks;
    bool running;
    bool in_use;
} mhi_pwm_state_t;

typedef struct {
    uint16_t pulse_min_us;
    uint16_t pulse_max_us;
    float    angle_min_deg;
    float    angle_max_deg;
    float    current_angle;
    bool     in_use;
} mhi_servo_state_t;

typedef struct {
    mhi_adc_resolution_t resolution;
    float    vref;
    uint32_t max_raw;
    mhi_adc_mode_t mode;
    mhi_adc_done_callback_t cb;
    void    *cb_user;
    uint32_t interval_ms;
    bool     continuous;
    bool     in_use;
} mhi_adc_state_t;

static mhi_pin_state_t  s_pins[MHI_MAX_PINS];
static mhi_pwm_state_t  s_pwm[MHI_MAX_PWM_CH];
static mhi_servo_state_t s_servo[MHI_MAX_PWM_CH];
static mhi_adc_state_t  s_adc[MHI_MAX_ADC_CH];
static bool s_initialized = false;

static void ensure_init(void) {
    if (s_initialized) return;
    memset(s_pins,  0, sizeof(s_pins));
    memset(s_pwm,   0, sizeof(s_pwm));
    memset(s_servo, 0, sizeof(s_servo));
    memset(s_adc,   0, sizeof(s_adc));
    s_initialized = true;
}

/* ─── GPIO Implementation ───────────────────────────────────────────── */

int mhi_gpio_init(uint8_t pin, mhi_gpio_mode_t mode) {
    ensure_init();
    if (pin >= MHI_MAX_PINS) return -1;
    s_pins[pin].mode  = mode;
    s_pins[pin].level = MHI_GPIO_LOW;
    s_pins[pin].in_use = true;
    return 0;
}

void mhi_gpio_deinit(uint8_t pin) {
    if (pin < MHI_MAX_PINS) {
        s_pins[pin].in_use = false;
        s_pins[pin].irq_cb = NULL;
    }
}

void mhi_gpio_write(uint8_t pin, mhi_gpio_level_t level) {
    if (pin < MHI_MAX_PINS && s_pins[pin].in_use)
        s_pins[pin].level = level;
}

mhi_gpio_level_t mhi_gpio_read(uint8_t pin) {
    if (pin < MHI_MAX_PINS && s_pins[pin].in_use)
        return s_pins[pin].level;
    return MHI_GPIO_LOW;
}

void mhi_gpio_toggle(uint8_t pin) {
    if (pin < MHI_MAX_PINS && s_pins[pin].in_use)
        s_pins[pin].level = (s_pins[pin].level == MHI_GPIO_LOW)
                          ? MHI_GPIO_HIGH : MHI_GPIO_LOW;
}

int mhi_gpio_set_irq(uint8_t pin, mhi_gpio_irq_t trigger,
                     mhi_gpio_callback_t callback, void *user_data) {
    if (pin >= MHI_MAX_PINS || !s_pins[pin].in_use) return -1;
    s_pins[pin].irq      = trigger;
    s_pins[pin].irq_cb   = callback;
    s_pins[pin].irq_user = user_data;
    return 0;
}

void mhi_gpio_clear_irq(uint8_t pin) {
    if (pin < MHI_MAX_PINS) {
        s_pins[pin].irq      = MHI_GPIO_IRQ_NONE;
        s_pins[pin].irq_cb   = NULL;
        s_pins[pin].irq_user = NULL;
    }
}

/* ─── PWM Implementation ────────────────────────────────────────────── */

int mhi_pwm_init(uint8_t channel, float frequency_hz,
                 mhi_pwm_resolution_t res) {
    ensure_init();
    if (channel >= MHI_MAX_PWM_CH || frequency_hz <= 0.0f) return -1;
    s_pwm[channel].resolution   = res;
    s_pwm[channel].frequency_hz = frequency_hz;
    s_pwm[channel].duty_percent = 0.0f;
    s_pwm[channel].period_ticks = (uint16_t)((1u << (uint8_t)res) - 1u);
    s_pwm[channel].duty_ticks   = 0;
    s_pwm[channel].running      = false;
    s_pwm[channel].in_use       = true;
    return 0;
}

void mhi_pwm_deinit(uint8_t channel) {
    if (channel < MHI_MAX_PWM_CH) {
        s_pwm[channel].in_use  = false;
        s_pwm[channel].running = false;
    }
}

int mhi_pwm_set_duty(uint8_t channel, float percent) {
    if (channel >= MHI_MAX_PWM_CH || !s_pwm[channel].in_use) return -1;
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 100.0f) percent = 100.0f;
    s_pwm[channel].duty_percent = percent;
    s_pwm[channel].duty_ticks = (uint16_t)(
        (percent / 100.0f) * (float)s_pwm[channel].period_ticks);
    return 0;
}

int mhi_pwm_set_duty_raw(uint8_t channel, uint16_t raw) {
    if (channel >= MHI_MAX_PWM_CH || !s_pwm[channel].in_use) return -1;
    if (raw > s_pwm[channel].period_ticks) raw = s_pwm[channel].period_ticks;
    s_pwm[channel].duty_ticks   = raw;
    s_pwm[channel].duty_percent = (float)raw
        / (float)s_pwm[channel].period_ticks * 100.0f;
    return 0;
}

float mhi_pwm_get_duty(uint8_t channel) {
    if (channel >= MHI_MAX_PWM_CH || !s_pwm[channel].in_use) return 0.0f;
    return s_pwm[channel].duty_percent;
}

int mhi_pwm_set_frequency(uint8_t channel, float frequency_hz) {
    if (channel >= MHI_MAX_PWM_CH || !s_pwm[channel].in_use
        || frequency_hz <= 0.0f) return -1;
    s_pwm[channel].frequency_hz = frequency_hz;
    return 0;
}

float mhi_pwm_get_frequency(uint8_t channel) {
    if (channel >= MHI_MAX_PWM_CH || !s_pwm[channel].in_use) return 0.0f;
    return s_pwm[channel].frequency_hz;
}

void mhi_pwm_start(uint8_t channel) {
    if (channel < MHI_MAX_PWM_CH && s_pwm[channel].in_use)
        s_pwm[channel].running = true;
}

void mhi_pwm_stop(uint8_t channel) {
    if (channel < MHI_MAX_PWM_CH && s_pwm[channel].in_use) {
        s_pwm[channel].running = false;
        s_pwm[channel].duty_ticks = 0;
        s_pwm[channel].duty_percent = 0.0f;
    }
}

bool mhi_pwm_is_running(uint8_t channel) {
    if (channel >= MHI_MAX_PWM_CH || !s_pwm[channel].in_use) return false;
    return s_pwm[channel].running;
}

/* ─── Servo Implementation ──────────────────────────────────────────── */

int mhi_servo_init(const mhi_servo_config_t *cfg) {
    if (!cfg) return -1;
    ensure_init();
    if (cfg->pwm_channel >= MHI_MAX_PWM_CH) return -1;
    s_servo[cfg->pwm_channel].pulse_min_us = cfg->pulse_min_us;
    s_servo[cfg->pwm_channel].pulse_max_us = cfg->pulse_max_us;
    s_servo[cfg->pwm_channel].angle_min_deg = cfg->angle_min_deg;
    s_servo[cfg->pwm_channel].angle_max_deg = cfg->angle_max_deg;
    s_servo[cfg->pwm_channel].current_angle = 0.0f;
    s_servo[cfg->pwm_channel].in_use = true;
    return mhi_pwm_init(cfg->pwm_channel,
                        1000000.0f / (float)cfg->period_us,
                        MHI_PWM_RES_16BIT);
}

void mhi_servo_deinit(uint8_t pwm_channel) {
    if (pwm_channel < MHI_MAX_PWM_CH) {
        mhi_pwm_deinit(pwm_channel);
        s_servo[pwm_channel].in_use = false;
    }
}

int mhi_servo_set_angle(uint8_t pwm_channel, float angle_deg) {
    if (pwm_channel >= MHI_MAX_PWM_CH || !s_servo[pwm_channel].in_use)
        return -1;
    mhi_servo_state_t *s = &s_servo[pwm_channel];
    if (angle_deg < s->angle_min_deg) angle_deg = s->angle_min_deg;
    if (angle_deg > s->angle_max_deg) angle_deg = s->angle_max_deg;

    float pulse_us = mhi_map_float(angle_deg,
        s->angle_min_deg, s->angle_max_deg,
        (float)s->pulse_min_us, (float)s->pulse_max_us);

    float duty = (pulse_us / (float)s_servo[pwm_channel].pulse_min_us)
                * (s_servo[pwm_channel].pulse_min_us < 20000.0f
                   ? pulse_us / 20000.0f : pulse_us / 20000.0f);
    duty = pulse_us / 200.0f;
    s->current_angle = angle_deg;
    return mhi_pwm_set_duty(pwm_channel, duty);
}

float mhi_servo_get_angle(uint8_t pwm_channel) {
    if (pwm_channel >= MHI_MAX_PWM_CH || !s_servo[pwm_channel].in_use)
        return 0.0f;
    return s_servo[pwm_channel].current_angle;
}

int mhi_servo_calibrate(uint8_t pwm_channel,
                        uint16_t pulse_min_us, uint16_t pulse_max_us,
                        float angle_min_deg, float angle_max_deg) {
    if (pwm_channel >= MHI_MAX_PWM_CH || !s_servo[pwm_channel].in_use)
        return -1;
    s_servo[pwm_channel].pulse_min_us = pulse_min_us;
    s_servo[pwm_channel].pulse_max_us = pulse_max_us;
    s_servo[pwm_channel].angle_min_deg = angle_min_deg;
    s_servo[pwm_channel].angle_max_deg = angle_max_deg;
    return 0;
}

/* ─── ADC Implementation ────────────────────────────────────────────── */

int mhi_adc_init(uint8_t channel, mhi_adc_resolution_t res, float vref) {
    ensure_init();
    if (channel >= MHI_MAX_ADC_CH || vref <= 0.0f) return -1;
    s_adc[channel].resolution = res;
    s_adc[channel].vref       = vref;
    s_adc[channel].max_raw    = (uint32_t)((1u << (uint8_t)res) - 1u);
    s_adc[channel].mode       = MHI_ADC_MODE_SINGLE;
    s_adc[channel].continuous = false;
    s_adc[channel].in_use     = true;
    return 0;
}

void mhi_adc_deinit(uint8_t channel) {
    if (channel < MHI_MAX_ADC_CH) {
        s_adc[channel].in_use     = false;
        s_adc[channel].continuous = false;
    }
}

int mhi_adc_read_single(uint8_t channel, uint32_t *raw, float *voltage) {
    if (channel >= MHI_MAX_ADC_CH || !s_adc[channel].in_use) return -1;
    if (raw) *raw = 0;
    if (voltage) *voltage = 0.0f;
    return 0;
}

int mhi_adc_read_continuous_start(uint8_t channel, uint32_t interval_ms,
                    mhi_adc_done_callback_t callback, void *user_data) {
    if (channel >= MHI_MAX_ADC_CH || !s_adc[channel].in_use) return -1;
    s_adc[channel].mode       = MHI_ADC_MODE_CONTINUOUS;
    s_adc[channel].continuous = true;
    s_adc[channel].interval_ms = interval_ms;
    s_adc[channel].cb          = callback;
    s_adc[channel].cb_user     = user_data;
    return 0;
}

void mhi_adc_read_continuous_stop(uint8_t channel) {
    if (channel < MHI_MAX_ADC_CH) {
        s_adc[channel].continuous = false;
        s_adc[channel].mode       = MHI_ADC_MODE_SINGLE;
    }
}

int mhi_adc_read_differential(uint8_t ch_positive, uint8_t ch_negative,
                              float gain, uint32_t *raw, float *voltage) {
    if (ch_positive >= MHI_MAX_ADC_CH || ch_negative >= MHI_MAX_ADC_CH
        || !s_adc[ch_positive].in_use || !s_adc[ch_negative].in_use)
        return -1;
    if (raw)    *raw = 0;
    if (voltage) *voltage = 0.0f;
    (void)gain;
    return 0;
}

float mhi_adc_raw_to_voltage(uint32_t raw, float vref, uint32_t max_raw) {
    if (max_raw == 0u) return 0.0f;
    return vref * (float)raw / (float)max_raw;
}

uint32_t mhi_adc_voltage_to_raw(float voltage, float vref, uint32_t max_raw) {
    if (vref <= 0.0f) return 0u;
    float ratio = voltage / vref;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    return (uint32_t)(ratio * (float)max_raw + 0.5f);
}
