#ifndef MHI_GPIO_PWM_ADC_H
#define MHI_GPIO_PWM_ADC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── GPIO ──────────────────────────────────────────────────────────── */

typedef enum {
    MHI_GPIO_MODE_INPUT         = 0,
    MHI_GPIO_MODE_OUTPUT        = 1,
    MHI_GPIO_MODE_INPUT_PULLUP  = 2,
    MHI_GPIO_MODE_INPUT_PULLDOWN = 3,
    MHI_GPIO_MODE_OPEN_DRAIN    = 4
} mhi_gpio_mode_t;

typedef enum {
    MHI_GPIO_LOW  = 0,
    MHI_GPIO_HIGH = 1
} mhi_gpio_level_t;

typedef enum {
    MHI_GPIO_IRQ_NONE    = 0,
    MHI_GPIO_IRQ_RISING  = 1,
    MHI_GPIO_IRQ_FALLING = 2,
    MHI_GPIO_IRQ_BOTH    = 3
} mhi_gpio_irq_t;

typedef void (*mhi_gpio_callback_t)(uint8_t pin, void *user_data);

int  mhi_gpio_init(uint8_t pin, mhi_gpio_mode_t mode);
void mhi_gpio_deinit(uint8_t pin);
void mhi_gpio_write(uint8_t pin, mhi_gpio_level_t level);
mhi_gpio_level_t mhi_gpio_read(uint8_t pin);
void mhi_gpio_toggle(uint8_t pin);
int  mhi_gpio_set_irq(uint8_t pin, mhi_gpio_irq_t trigger,
                      mhi_gpio_callback_t callback, void *user_data);
void mhi_gpio_clear_irq(uint8_t pin);

/* ─── PWM ───────────────────────────────────────────────────────────── */

typedef enum {
    MHI_PWM_RES_8BIT  = 8,
    MHI_PWM_RES_16BIT = 16
} mhi_pwm_resolution_t;

typedef struct {
    uint8_t  channel;
    uint16_t period;        /* timer ticks per period */
    uint16_t duty;          /* timer ticks high */
    float    duty_percent;  /* 0.0f – 100.0f */
    float    frequency_hz;
    mhi_pwm_resolution_t resolution;
} mhi_pwm_config_t;

int  mhi_pwm_init(uint8_t channel, float frequency_hz,
                  mhi_pwm_resolution_t res);
void mhi_pwm_deinit(uint8_t channel);
int  mhi_pwm_set_duty(uint8_t channel, float percent);   /* 0.0 – 100.0 */
int  mhi_pwm_set_duty_raw(uint8_t channel, uint16_t raw);
float mhi_pwm_get_duty(uint8_t channel);
int  mhi_pwm_set_frequency(uint8_t channel, float frequency_hz);
float mhi_pwm_get_frequency(uint8_t channel);
void mhi_pwm_start(uint8_t channel);
void mhi_pwm_stop(uint8_t channel);
bool mhi_pwm_is_running(uint8_t channel);

/* ─── Servo ─────────────────────────────────────────────────────────── */

typedef struct {
    uint8_t  pwm_channel;
    uint16_t pulse_min_us;   /* default 500us → 0° */
    uint16_t pulse_max_us;   /* default 2500us → 180° */
    float    angle_min_deg;  /* default 0.0° */
    float    angle_max_deg;  /* default 180.0° */
    uint16_t period_us;      /* default 20000us = 50Hz */
} mhi_servo_config_t;

int  mhi_servo_init(const mhi_servo_config_t *cfg);
void mhi_servo_deinit(uint8_t pwm_channel);
int  mhi_servo_set_angle(uint8_t pwm_channel, float angle_deg);
float mhi_servo_get_angle(uint8_t pwm_channel);
int  mhi_servo_calibrate(uint8_t pwm_channel,
                         uint16_t pulse_min_us, uint16_t pulse_max_us,
                         float angle_min_deg, float angle_max_deg);

/* ─── ADC ───────────────────────────────────────────────────────────── */

typedef enum {
    MHI_ADC_RES_10BIT = 10,
    MHI_ADC_RES_12BIT = 12,
    MHI_ADC_RES_16BIT = 16
} mhi_adc_resolution_t;

typedef enum {
    MHI_ADC_MODE_SINGLE      = 0,
    MHI_ADC_MODE_CONTINUOUS  = 1,
    MHI_ADC_MODE_DIFFERENTIAL = 2
} mhi_adc_mode_t;

typedef struct {
    uint8_t  channel;
    mhi_adc_resolution_t resolution;
    float    vref;         /* reference voltage */
    uint32_t max_raw;      /* (1 << resolution) - 1 */
} mhi_adc_config_t;

typedef void (*mhi_adc_done_callback_t)(uint8_t channel, uint32_t raw,
                                        float voltage, void *user_data);

int  mhi_adc_init(uint8_t channel, mhi_adc_resolution_t res, float vref);
void mhi_adc_deinit(uint8_t channel);
int  mhi_adc_read_single(uint8_t channel, uint32_t *raw, float *voltage);
int  mhi_adc_read_continuous_start(uint8_t channel, uint32_t interval_ms,
                     mhi_adc_done_callback_t callback, void *user_data);
void mhi_adc_read_continuous_stop(uint8_t channel);
int  mhi_adc_read_differential(uint8_t ch_positive, uint8_t ch_negative,
                               float gain, uint32_t *raw, float *voltage);
float mhi_adc_raw_to_voltage(uint32_t raw, float vref, uint32_t max_raw);
uint32_t mhi_adc_voltage_to_raw(float voltage, float vref, uint32_t max_raw);

/* ─── Utility ───────────────────────────────────────────────────────── */

static inline float mhi_map_float(float x, float in_min, float in_max,
                                  float out_min, float out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

#ifdef __cplusplus
}
#endif

#endif /* MHI_GPIO_PWM_ADC_H */
