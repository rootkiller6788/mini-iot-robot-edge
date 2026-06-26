#include "gpio_pwm_adc.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    printf("=== Example: GPIO + PWM + Servo Control ===\n\n");

    /* ── GPIO: LED blink on pin 13 ── */
    printf("[GPIO] Init pin 13 as OUTPUT\n");
    mhi_gpio_init(13u, MHI_GPIO_MODE_OUTPUT);

    printf("[GPIO] Blink LED 3 times:\n");
    int i;
    for (i = 0; i < 3; i++) {
        printf("  LED ON\n");
        mhi_gpio_write(13u, MHI_GPIO_HIGH);
        usleep(500000);

        printf("  LED OFF\n");
        mhi_gpio_write(13u, MHI_GPIO_LOW);
        usleep(500000);
    }

    /* ── GPIO: button input with pull-up ── */
    printf("\n[GPIO] Init pin 2 as INPUT_PULLUP\n");
    mhi_gpio_init(2u, MHI_GPIO_MODE_INPUT_PULLUP);
    mhi_gpio_level_t btn = mhi_gpio_read(2u);
    printf("[GPIO] Button state: %s\n",
           btn == MHI_GPIO_HIGH ? "RELEASED" : "PRESSED");

    /* ── PWM: LED brightness fade ── */
    printf("\n[PWM] Init channel 0 at 1kHz, 8-bit resolution\n");
    mhi_pwm_init(0u, 1000.0f, MHI_PWM_RES_8BIT);
    mhi_pwm_start(0u);

    printf("[PWM] Fade LED from 0%% to 100%%:\n");
    float duty;
    for (duty = 0.0f; duty <= 100.0f; duty += 10.0f) {
        mhi_pwm_set_duty(0u, duty);
        printf("  duty = %.0f%%\n", mhi_pwm_get_duty(0u));
        usleep(200000);
    }
    mhi_pwm_stop(0u);

    /* ── PWM: 16-bit high-resolution ── */
    printf("\n[PWM] Init channel 1 at 50Hz, 16-bit (for servo)\n");
    mhi_pwm_init(1u, 50.0f, MHI_PWM_RES_16BIT);
    uint16_t raw = (uint16_t)(65535u * 7.5f / 100.0f);
    printf("[PWM] Raw duty = %u / 65535 (7.5%%)\n", raw);
    mhi_pwm_set_duty_raw(1u, raw);
    mhi_pwm_start(1u);
    usleep(500000);
    mhi_pwm_stop(1u);

    /* ── Servo: sweep 0° to 180° ── */
    printf("\n[Servo] Init servo on PWM channel 1 (50Hz, 500-2500us)\n");
    mhi_servo_config_t servo_cfg = {
        .pwm_channel  = 1u,
        .pulse_min_us = 500u,
        .pulse_max_us = 2500u,
        .angle_min_deg = 0.0f,
        .angle_max_deg = 180.0f,
        .period_us    = 20000u
    };
    mhi_servo_init(&servo_cfg);

    printf("[Servo] Sweep 0 -> 90 -> 180 degrees:\n");
    float angles[] = { 0.0f, 45.0f, 90.0f, 135.0f, 180.0f };
    for (i = 0; i < 5; i++) {
        mhi_servo_set_angle(1u, angles[i]);
        printf("  angle = %.0f deg, pulse ~ %.0f us\n",
               mhi_servo_get_angle(1u),
               mhi_map_float(angles[i], 0.0f, 180.0f, 500.0f, 2500.0f));
        usleep(300000);
    }

    /* ── Servo: recalibrate ── */
    printf("\n[Servo] Recalibrate: 600us-2400us, -90° to +90°\n");
    mhi_servo_calibrate(1u, 600u, 2400u, -90.0f, 90.0f);
    mhi_servo_set_angle(1u, 0.0f);
    printf("  center at 0°, pulse = %u us\n",
           mhi_servo_get_angle(1u) == 0.0f ? 1500u : 1500u);

    /* ── ADC: read and convert ── */
    printf("\n[ADC] Init channel 0: 12-bit, Vref = 3.3V\n");
    mhi_adc_init(0u, MHI_ADC_RES_12BIT, 3.3f);

    uint32_t raw_val = 2048u;
    float voltage = mhi_adc_raw_to_voltage(raw_val, 3.3f, 4095u);
    printf("[ADC] Raw=%lu -> Voltage=%.3fV (mid-scale)\n",
           (unsigned long)raw_val, (double)voltage);

    raw_val = mhi_adc_voltage_to_raw(2.5f, 3.3f, 4095u);
    printf("[ADC] 2.5V -> Raw=%lu\n", (unsigned long)raw_val);

    printf("\n[ADC] Init differential: ch0-ch1 with 16-bit res, Vref=5.0V\n");
    mhi_adc_init(0u, MHI_ADC_RES_16BIT, 5.0f);
    mhi_adc_init(1u, MHI_ADC_RES_16BIT, 5.0f);
    mhi_adc_read_differential(0u, 1u, 1.0f, NULL, NULL);

    /* ── Continuous ADC (demo start/stop) ── */
    printf("\n[ADC] Start continuous read on channel 0 (interval=100ms)\n");
    mhi_adc_read_continuous_start(0u, 100u, NULL, NULL);
    usleep(300000);
    mhi_adc_read_continuous_stop(0u);
    printf("[ADC] Continuous read stopped\n");

    /* ── Cleanup ── */
    mhi_gpio_deinit(13u);
    mhi_gpio_deinit(2u);
    mhi_pwm_deinit(0u);
    mhi_pwm_deinit(1u);
    mhi_servo_deinit(1u);
    mhi_adc_deinit(0u);
    mhi_adc_deinit(1u);

    printf("\n=== Example complete ===\n");
    return 0;
}
