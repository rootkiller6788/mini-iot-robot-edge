#include "gpio_pwm_adc.h"
#include "sensor_polling.h"
#include "actuator_driver.h"
#include "signal_cond.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

/* ── Sensor values ── */
static float s_temperature_c   = 22.0f;
static float s_humidity_pct    = 50.0f;
static float s_pressure_hpa    = 1013.0f;
static float s_light_lux       = 500.0f;
static float s_co2_ppm         = 420.0f;
static float s_soil_moisture   = 65.0f;
static float s_wind_speed_ms   = 0.0f;
static uint32_t s_rain_count   = 0u;

/* ── Sensor descriptors ── */
static mhi_sensor_t s_sensors[6];
static float s_temp_raw[1]   = {0.0f}, s_temp_filt[1]   = {0.0f};
static float s_hum_raw[1]    = {0.0f}, s_hum_filt[1]    = {0.0f};
static float s_press_raw[1]  = {0.0f}, s_press_filt[1]  = {0.0f};
static float s_light_raw[1]  = {0.0f}, s_light_filt[1]  = {0.0f};
static float s_co2_raw[1]    = {0.0f}, s_co2_filt[1]    = {0.0f};
static float s_soil_raw[1]   = {0.0f}, s_soil_filt[1]   = {0.0f};

static mhi_sensor_manager_t s_sensor_mgr;

/* ── Signal conditioning ── */
static mhi_moving_average_t s_temp_ma, s_hum_ma, s_press_ma;
static mhi_moving_average_t s_light_ma, s_co2_ma, s_soil_ma;
static mhi_kalman_1d_t s_temp_kf, s_hum_kf, s_press_kf;

static float s_ma_temp_buf[10], s_ma_hum_buf[10], s_ma_press_buf[10];
static float s_ma_light_buf[10], s_ma_co2_buf[10], s_ma_soil_buf[10];

static mhi_rc_filter_t s_temp_lp;
static mhi_rc_filter_t s_hum_lp;

/* ── Alarms ── */
static bool s_alarm_high_temp  = false;
static bool s_alarm_low_temp   = false;
static bool s_alarm_high_hum   = false;
static bool s_alarm_low_hum    = false;
static bool s_alarm_co2        = false;

/* ── Actuator control ── */
static mhi_dc_motor_t s_fan;
static mhi_dc_motor_t s_heater;
static bool s_fan_on     = false;
static bool s_heater_on  = false;

/* ── GPIO IRQ for rain gauge and anemometer ── */
static mhi_sensor_irq_t s_rain_irq;
static mhi_sensor_irq_t s_wind_irq;

/* ── Polling timer ── */
static mhi_poll_timer_t s_sensor_timer;
static mhi_poll_timer_t s_alarm_timer;
static mhi_poll_timer_t s_display_timer;

/* ── Read functions ── */
static void read_temp(void *ctx, float *values, uint8_t *count)
{
    (void)ctx;
    s_temperature_c += (float)((rand() % 80) - 40) * 0.02f;
    if (s_temperature_c < -5.0f)  s_temperature_c = -5.0f;
    if (s_temperature_c > 45.0f)  s_temperature_c = 45.0f;
    values[0] = s_temperature_c;
    *count = 1u;
}

static void read_humidity(void *ctx, float *values, uint8_t *count)
{
    (void)ctx;
    s_humidity_pct += (float)((rand() % 50) - 25) * 0.1f;
    if (s_humidity_pct < 10.0f)  s_humidity_pct = 10.0f;
    if (s_humidity_pct > 95.0f)  s_humidity_pct = 95.0f;
    values[0] = s_humidity_pct;
    *count = 1u;
}

static void read_pressure(void *ctx, float *values, uint8_t *count)
{
    (void)ctx;
    s_pressure_hpa += (float)((rand() % 40) - 20) * 0.05f;
    if (s_pressure_hpa < 980.0f)  s_pressure_hpa = 980.0f;
    if (s_pressure_hpa > 1050.0f) s_pressure_hpa = 1050.0f;
    values[0] = s_pressure_hpa;
    *count = 1u;
}

static void read_light(void *ctx, float *values, uint8_t *count)
{
    (void)ctx;
    s_light_lux += (float)((rand() % 100) - 50) * 1.0f;
    if (s_light_lux < 0.0f)    s_light_lux = 0.0f;
    if (s_light_lux > 10000.0f) s_light_lux = 10000.0f;
    values[0] = s_light_lux;
    *count = 1u;
}

static void read_co2(void *ctx, float *values, uint8_t *count)
{
    (void)ctx;
    s_co2_ppm += (float)((rand() % 30) - 15) * 0.5f;
    if (s_co2_ppm < 300.0f)  s_co2_ppm = 300.0f;
    if (s_co2_ppm > 5000.0f) s_co2_ppm = 5000.0f;
    values[0] = s_co2_ppm;
    *count = 1u;
}

static void read_soil(void *ctx, float *values, uint8_t *count)
{
    (void)ctx;
    s_soil_moisture += (float)((rand() % 20) - 10) * 0.1f;
    if (s_soil_moisture < 0.0f)   s_soil_moisture = 0.0f;
    if (s_soil_moisture > 100.0f) s_soil_moisture = 100.0f;
    values[0] = s_soil_moisture;
    *count = 1u;
}

/* ── IRQ callbacks ── */
static void on_rain_tip(uint8_t pin, void *user_data)
{
    (void)pin;
    (void)user_data;
    s_rain_count++;
}

static void on_wind_pulse(uint8_t pin, void *user_data)
{
    (void)pin;
    (void)user_data;
    s_wind_speed_ms += 0.5f;
    if (s_wind_speed_ms > 50.0f) s_wind_speed_ms = 50.0f;
}

/* ── Timer callbacks ── */
static void on_sensor_tick(void *user_data)
{
    (void)user_data;
    mhi_sensor_manager_read_current(&s_sensor_mgr);
}

static void on_alarm_tick(void *user_data)
{
    (void)user_data;

    float t = mhi_kalman_1d_update(&s_temp_kf,
               mhi_ma_update(&s_temp_ma, s_temp_raw[0]));
    float h = mhi_kalman_1d_update(&s_hum_kf,
               mhi_ma_update(&s_hum_ma, s_hum_raw[0]));
    float p = mhi_kalman_1d_update(&s_press_kf,
               mhi_ma_update(&s_press_ma, s_press_raw[0]));

    s_temp_filt[0]   = mhi_rc_filter_update(&s_temp_lp, t);
    s_hum_filt[0]    = mhi_rc_filter_update(&s_hum_lp, h);
    s_press_filt[0]  = p;
    s_light_filt[0]  = mhi_ma_update(&s_light_ma, s_light_raw[0]);
    s_co2_filt[0]    = mhi_ma_update(&s_co2_ma, s_co2_raw[0]);
    s_soil_filt[0]   = mhi_ma_update(&s_soil_ma, s_soil_raw[0]);

    s_alarm_high_temp = (s_temp_filt[0] > 35.0f);
    s_alarm_low_temp  = (s_temp_filt[0] < 5.0f);
    s_alarm_high_hum  = (s_hum_filt[0] > 80.0f);
    s_alarm_low_hum   = (s_hum_filt[0] < 20.0f);
    s_alarm_co2       = (s_co2_filt[0] > 1500.0f);

    if (s_alarm_high_temp || s_alarm_co2) {
        if (!s_fan_on) {
            mhi_dc_motor_set_speed(&s_fan, 0.8f);
            s_fan_on = true;
        }
    } else if (s_fan_on && !s_alarm_high_temp && !s_alarm_co2) {
        mhi_dc_motor_set_speed(&s_fan, 0.0f);
        s_fan_on = false;
    }

    if (s_alarm_low_temp) {
        if (!s_heater_on) {
            mhi_dc_motor_set_speed(&s_heater, 0.5f);
            s_heater_on = true;
        }
    } else if (s_heater_on && !s_alarm_low_temp) {
        mhi_dc_motor_set_speed(&s_heater, 0.0f);
        s_heater_on = false;
    }
}

static void on_display_tick(void *user_data)
{
    (void)user_data;

    printf("\n╔══ ENVIRONMENTAL MONITOR ═══════════════════════════╗\n");
    printf("║ TEMP     % 7.2f °C   %s %s  ║\n",
           (double)s_temp_filt[0],
           s_alarm_high_temp ? "[HIGH!]" : "       ",
           s_alarm_low_temp  ? "[LOW!]"  : "       ");
    printf("║ HUM      % 7.2f %%    %s %s  ║\n",
           (double)s_hum_filt[0],
           s_alarm_high_hum ? "[HIGH!]" : "       ",
           s_alarm_low_hum  ? "[LOW!]"  : "       ");
    printf("║ PRESS    % 7.2f hPa                      ║\n",
           (double)s_press_filt[0]);
    printf("║ LIGHT    % 7.1f lux                      ║\n",
           (double)s_light_filt[0]);
    printf("║ CO2      % 7.1f ppm  %s               ║\n",
           (double)s_co2_filt[0],
           s_alarm_co2 ? "[DANGER!]" : "         ");
    printf("║ SOIL     % 7.2f %%                         ║\n",
           (double)s_soil_filt[0]);
    printf("║ WIND     % 7.2f m/s  pulses              ║\n",
           (double)s_wind_speed_ms);
    printf("║ RAIN     % 7u tips                       ║\n",
           s_rain_count);
    printf("║ FAN      %s  HEATER  %s                    ║\n",
           s_fan_on ? "ON " : "OFF", s_heater_on ? "ON " : "OFF");
    printf("╚═══════════════════════════════════════════════════╝\n");
}

/* ── Initialize sensors ── */
static void init_sensor_array(void)
{
    s_sensors[0].id = 0u;
    s_sensors[0].type = MHI_SENSOR_TEMP;
    s_sensors[0].name = "BME280-Temp";
    s_sensors[0].read = read_temp;
    s_sensors[0].raw_values = s_temp_raw;
    s_sensors[0].filtered_values = s_temp_filt;
    s_sensors[0].value_count = 1u;
    s_sensors[0].poll_interval_ms = 200u;

    s_sensors[1].id = 1u;
    s_sensors[1].type = MHI_SENSOR_HUMIDITY;
    s_sensors[1].name = "BME280-Humidity";
    s_sensors[1].read = read_humidity;
    s_sensors[1].raw_values = s_hum_raw;
    s_sensors[1].filtered_values = s_hum_filt;
    s_sensors[1].value_count = 1u;
    s_sensors[1].poll_interval_ms = 200u;

    s_sensors[2].id = 2u;
    s_sensors[2].type = MHI_SENSOR_PRESSURE;
    s_sensors[2].name = "BME280-Pressure";
    s_sensors[2].read = read_pressure;
    s_sensors[2].raw_values = s_press_raw;
    s_sensors[2].filtered_values = s_press_filt;
    s_sensors[2].value_count = 1u;
    s_sensors[2].poll_interval_ms = 300u;

    s_sensors[3].id = 3u;
    s_sensors[3].type = MHI_SENSOR_LIGHT;
    s_sensors[3].name = "BH1750-Light";
    s_sensors[3].read = read_light;
    s_sensors[3].raw_values = s_light_raw;
    s_sensors[3].filtered_values = s_light_filt;
    s_sensors[3].value_count = 1u;
    s_sensors[3].poll_interval_ms = 500u;

    s_sensors[4].id = 4u;
    s_sensors[4].type = MHI_SENSOR_GAS;
    s_sensors[4].name = "MH-Z19-CO2";
    s_sensors[4].read = read_co2;
    s_sensors[4].raw_values = s_co2_raw;
    s_sensors[4].filtered_values = s_co2_filt;
    s_sensors[4].value_count = 1u;
    s_sensors[4].poll_interval_ms = 1000u;

    s_sensors[5].id = 5u;
    s_sensors[5].type = MHI_SENSOR_CUSTOM;
    s_sensors[5].name = "Soil-Moisture";
    s_sensors[5].read = read_soil;
    s_sensors[5].raw_values = s_soil_raw;
    s_sensors[5].filtered_values = s_soil_filt;
    s_sensors[5].value_count = 1u;
    s_sensors[5].poll_interval_ms = 2000u;
}

int main(void)
{
    printf("=== Demo: Multi-Sensor Environmental Monitoring Station ===\n\n");

    /* ── Init sensors ── */
    printf("[INIT] Configuring 6 environmental sensors\n");
    init_sensor_array();
    mhi_sensor_manager_init(&s_sensor_mgr, s_sensors, 6u);

    /* ── Init signal conditioning ── */
    printf("[INIT] Data smoothing (moving avg, Kalman, RC filter)\n");
    mhi_ma_init(&s_temp_ma, s_ma_temp_buf, 10u);
    mhi_ma_init(&s_hum_ma, s_ma_hum_buf, 10u);
    mhi_ma_init(&s_press_ma, s_ma_press_buf, 10u);
    mhi_ma_init(&s_light_ma, s_ma_light_buf, 10u);
    mhi_ma_init(&s_co2_ma, s_ma_co2_buf, 10u);
    mhi_ma_init(&s_soil_ma, s_ma_soil_buf, 10u);

    mhi_kalman_1d_init(&s_temp_kf, 22.0f, 0.005f, 0.5f);
    mhi_kalman_1d_init(&s_hum_kf, 50.0f, 0.01f, 1.0f);
    mhi_kalman_1d_init(&s_press_kf, 1013.0f, 0.05f, 2.0f);

    mhi_rc_filter_init(&s_temp_lp, MHI_FILTER_LOWPASS, 1000.0f, 1e-6f, 0.2f);
    mhi_rc_filter_init(&s_hum_lp, MHI_FILTER_LOWPASS, 1000.0f, 1e-6f, 0.2f);

    /* ── Init GPIO for rain gauge / anemometer interrupts ── */
    printf("[INIT] Rain gauge IRQ on pin 10\n");
    mhi_sensor_irq_init(&s_rain_irq, 10u, on_rain_tip, NULL, 5u);
    mhi_sensor_irq_enable(&s_rain_irq);

    printf("[INIT] Anemometer IRQ on pin 11\n");
    mhi_sensor_irq_init(&s_wind_irq, 11u, on_wind_pulse, NULL, 5u);
    mhi_sensor_irq_enable(&s_wind_irq);

    /* ── Init actuators ── */
    printf("[INIT] Fan (PWM ch3, IN1=pin8, IN2=pin9)\n");
    mhi_gpio_init(8u, MHI_GPIO_MODE_OUTPUT);
    mhi_gpio_init(9u, MHI_GPIO_MODE_OUTPUT);
    mhi_pwm_init(3u, 1000.0f, MHI_PWM_RES_8BIT);
    mhi_dc_motor_init(&s_fan, 3u, 8u, 9u);
    s_fan.soft_start_ramp_s = 0.3f;

    printf("[INIT] Heater (PWM ch4, IN1=pin12, IN2=pin13)\n");
    mhi_gpio_init(12u, MHI_GPIO_MODE_OUTPUT);
    mhi_gpio_init(13u, MHI_GPIO_MODE_OUTPUT);
    mhi_pwm_init(4u, 1000.0f, MHI_PWM_RES_8BIT);
    mhi_dc_motor_init(&s_heater, 4u, 12u, 13u);
    s_heater.soft_start_ramp_s = 1.0f;

    /* ── Init ADC for analog sensors ── */
    printf("[INIT] ADC channels\n");
    mhi_adc_init(0u, MHI_ADC_RES_12BIT, 3.3f);
    mhi_adc_init(1u, MHI_ADC_RES_12BIT, 3.3f);

    /* ── Init timers ── */
    printf("[INIT] Timers\n");
    mhi_poll_timer_init(&s_sensor_timer, 100u, on_sensor_tick, NULL, true);
    mhi_poll_timer_init(&s_alarm_timer,  500u, on_alarm_tick,  NULL, true);
    mhi_poll_timer_init(&s_display_timer, 2000u, on_display_tick, NULL, true);
    mhi_poll_timer_start(&s_sensor_timer);
    mhi_poll_timer_start(&s_alarm_timer);
    mhi_poll_timer_start(&s_display_timer);

    /* ── Main loop ── */
    printf("\n[RUN] Monitoring environment (15-second run, display every 2s)\n\n");

    uint32_t ms;
    for (ms = 0u; ms <= 15000u; ms += 100u) {
        mhi_poll_timer_tick(&s_sensor_timer, ms);

        /* simulate wind pulses */
        if (ms % 3000u == 0u) {
            mhi_sensor_irq_fire(&s_wind_irq, ms);
            s_wind_speed_ms *= 0.9f;
        }

        /* simulate rain tips */
        if (ms == 5000u || ms == 10000u) {
            mhi_sensor_irq_fire(&s_rain_irq, ms);
        }

        mhi_poll_timer_tick(&s_alarm_timer, ms);

        /* tick motors for soft-start */
        mhi_dc_motor_tick(&s_fan, ms);
        mhi_dc_motor_tick(&s_heater, ms);

        mhi_poll_timer_tick(&s_display_timer, ms);
    }

    /* ── Shutdown ── */
    printf("\n[SHUTDOWN] Stopping all actuators\n");
    mhi_emergency_stop_all();
    mhi_dc_motor_deinit(&s_fan);
    mhi_dc_motor_deinit(&s_heater);
    mhi_pwm_deinit(3u);
    mhi_pwm_deinit(4u);
    mhi_adc_deinit(0u);
    mhi_adc_deinit(1u);

    printf("\n=== Demo: Environmental Monitor complete ===\n");
    return 0;
}
