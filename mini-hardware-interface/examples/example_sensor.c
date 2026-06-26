#include "sensor_polling.h"
#include "signal_cond.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

/* ── simulated BME280 read function ── */
static float s_fake_temp = 25.0f;
static float s_fake_humidity = 55.0f;
static float s_fake_pressure = 1013.25f;

static void read_temp_sensor(void *ctx, float *values, uint8_t *count)
{
    (void)ctx;
    s_fake_temp += (float)((rand() % 100) - 50) * 0.01f;
    if (s_fake_temp < -10.0f) s_fake_temp = -10.0f;
    if (s_fake_temp > 60.0f)  s_fake_temp = 60.0f;
    values[0] = s_fake_temp;
    *count = 1u;
}

static void read_humidity_sensor(void *ctx, float *values, uint8_t *count)
{
    (void)ctx;
    s_fake_humidity += (float)((rand() % 60) - 30) * 0.1f;
    if (s_fake_humidity < 0.0f)   s_fake_humidity = 0.0f;
    if (s_fake_humidity > 100.0f) s_fake_humidity = 100.0f;
    values[0] = s_fake_humidity;
    *count = 1u;
}

static void read_pressure_sensor(void *ctx, float *values, uint8_t *count)
{
    (void)ctx;
    s_fake_pressure += (float)((rand() % 30) - 15) * 0.1f;
    if (s_fake_pressure < 900.0f)  s_fake_pressure = 900.0f;
    if (s_fake_pressure > 1100.0f) s_fake_pressure = 1100.0f;
    values[0] = s_fake_pressure;
    *count = 1u;
}

/* ── polling callback ── */
static void on_poll_tick(void *user_data)
{
    float *counter = (float *)user_data;
    *counter += 1.0f;
    printf("  [TIMER] tick %.0f\n", *counter);
}

/* ── GPIO IRQ callback ── */
static void on_data_ready(uint8_t pin, void *user_data)
{
    const char *sensor_name = (const char *)user_data;
    printf("  [IRQ] Data ready from '%s' on pin %u\n", sensor_name, pin);
}

int main(void)
{
    printf("=== Example: Sensor Reading, Smoothing & Calibration ===\n\n");

    /* ── I2C bus init ── */
    printf("[BUS] Init I2C: addr=0x76, speed=400kHz\n");
    mhi_bus_config_t bus_cfg = {
        .type    = MHI_BUS_I2C,
        .address = 0x76u,
        .speed_hz = 400000u,
        .spi_mode = 0u,
        .spi_lsb_first = false
    };
    mhi_bus_init(&bus_cfg);

    printf("[BUS] Write to register 0xF4: 0x27\n");
    uint8_t cmd = 0x27u;
    mhi_bus_write_reg(0xF4u, &cmd, 1u);

    printf("[BUS] Read from register 0xFA (3 bytes)\n");
    uint8_t buf[3] = {0};
    mhi_bus_read_reg(0xFAu, buf, 3u);
    printf("  Result: 0x%02X 0x%02X 0x%02X\n", buf[0], buf[1], buf[2]);

    /* ── Sensor descriptor setup ── */
    float temp_raw[1]      = {0.0f};
    float temp_filt[1]     = {0.0f};
    float hum_raw[1]       = {0.0f};
    float hum_filt[1]      = {0.0f};
    float press_raw[1]     = {0.0f};
    float press_filt[1]    = {0.0f};

    mhi_sensor_t sensors[3];
    memset(sensors, 0, sizeof(sensors));

    sensors[0].id = 0u;
    sensors[0].type = MHI_SENSOR_TEMP;
    sensors[0].name = "BME280-Temp";
    sensors[0].read = read_temp_sensor;
    sensors[0].raw_values = temp_raw;
    sensors[0].filtered_values = temp_filt;
    sensors[0].value_count = 1u;
    sensors[0].poll_interval_ms = 200u;

    sensors[1].id = 1u;
    sensors[1].type = MHI_SENSOR_HUMIDITY;
    sensors[1].name = "BME280-Hum";
    sensors[1].read = read_humidity_sensor;
    sensors[1].raw_values = hum_raw;
    sensors[1].filtered_values = hum_filt;
    sensors[1].value_count = 1u;
    sensors[1].poll_interval_ms = 300u;

    sensors[2].id = 2u;
    sensors[2].type = MHI_SENSOR_PRESSURE;
    sensors[2].name = "BME280-Press";
    sensors[2].read = read_pressure_sensor;
    sensors[2].raw_values = press_raw;
    sensors[2].filtered_values = press_filt;
    sensors[2].value_count = 1u;
    sensors[2].poll_interval_ms = 500u;

    /* ── Sensor manager (round-robin) ── */
    printf("\n[SENSOR MGR] Init round-robin with 3 sensors\n");
    mhi_sensor_manager_t mgr;
    mhi_sensor_manager_init(&mgr, sensors, 3u);

    printf("[SENSOR MGR] Service for 1.5 seconds:\n");
    uint32_t tick;
    for (tick = 0u; tick < 1500u; tick += 100u) {
        mhi_sensor_manager_service(&mgr, tick);
        printf("  t=%ums  T=%.2f  H=%.2f  P=%.2f\n",
               tick, temp_raw[0], hum_raw[0], press_raw[0]);
    }

    /* ── Moving Average ── */
    printf("\n[MOVING AVG] Window=5, 10 samples with noise:\n");
    float ma_buf[5] = {0.0f};
    mhi_moving_average_t ma;
    mhi_ma_init(&ma, ma_buf, 5u);

    float noisy[] = { 25.0f, 25.3f, 24.8f, 25.5f, 25.1f,
                      24.9f, 26.0f, 25.2f, 25.7f, 24.6f };
    int j;
    for (j = 0; j < 10; j++) {
        float smoothed = mhi_ma_update(&ma, noisy[j]);
        printf("  raw=%.2f  filtered=%.2f\n", noisy[j], smoothed);
    }

    /* ── Median Filter ── */
    printf("\n[MEDIAN] Window=5, spike rejection test:\n");
    float med_buf[5]  = {0.0f};
    float med_sort[5] = {0.0f};
    mhi_median_filter_t mf;
    mhi_median_init(&mf, med_buf, med_sort, 5u);

    float spike_data[] = { 10.0f, 10.1f, 99.9f, 10.2f, 10.0f };
    for (j = 0; j < 5; j++) {
        float med_val = mhi_median_update(&mf, spike_data[j]);
        printf("  raw=%.1f  median=%.1f\n", spike_data[j], med_val);
    }

    /* ── Kalman 1D ── */
    printf("\n[KALMAN 1D] Filter temperature measurement noise:\n");
    mhi_kalman_1d_t kf;
    mhi_kalman_1d_init(&kf, 25.0f, 0.01f, 0.1f);

    float measurements[] = { 25.0f, 25.2f, 24.9f, 25.1f, 25.3f, 25.0f };
    for (j = 0; j < 6; j++) {
        float est = mhi_kalman_1d_update(&kf, measurements[j]);
        printf("  z=%.2f  x_hat=%.3f  P=%.4f\n",
               measurements[j], est, kf.p);
    }

    /* ── Calibration ── */
    printf("\n[CALIBRATION] Zero+Span for a gas sensor:\n");
    mhi_calibration_t cal;
    mhi_calibration_init(&cal);
    mhi_calibration_set_zero(&cal, 0.5f, 0.0f);   /* 0.5V → 0 ppm */
    mhi_calibration_set_span(&cal, 3.0f, 100.0f);  /* 3.0V → 100 ppm */
    printf("  zero: raw=0.5V -> ref=0ppm, span: raw=3.0V -> ref=100ppm\n");

    float test_readings[] = { 0.5f, 1.75f, 3.0f };
    for (j = 0; j < 3; j++) {
        float calibrated = mhi_calibration_apply(&cal, test_readings[j]);
        printf("  raw=%.2fV -> calibrated=%.1f ppm\n",
               test_readings[j], calibrated);
    }

    /* ── Polling timer ── */
    printf("\n[TIMER] 200ms periodic timer (3 ticks):\n");
    float timer_count = 0.0f;
    mhi_poll_timer_t poll_timer;
    mhi_poll_timer_init(&poll_timer, 200u, on_poll_tick, &timer_count, true);
    mhi_poll_timer_start(&poll_timer);

    for (tick = 0u; tick < 800u; tick += 100u) {
        mhi_poll_timer_tick(&poll_timer, tick);
        usleep(50000u);
    }
    mhi_poll_timer_stop(&poll_timer);

    /* ── GPIO interrupt for sensor data-ready ── */
    printf("\n[IRQ] GPIO interrupt on pin 5 (data-ready):\n");
    mhi_sensor_irq_t irq;
    mhi_sensor_irq_init(&irq, 5u, on_data_ready, "BME280", 10u);
    mhi_sensor_irq_enable(&irq);
    mhi_sensor_irq_fire(&irq, 100u);
    mhi_sensor_irq_fire(&irq, 105u);
    mhi_sensor_irq_fire(&irq, 200u);
    mhi_sensor_irq_disable(&irq);

    /* ── Signal conditioning: exponential filter ── */
    printf("\n[NOISE] Exponential filter (alpha=0.3):\n");
    float prev_val = 25.0f;
    for (j = 0; j < 5; j++) {
        float new_val = 25.0f + (float)((rand() % 100) - 50) * 0.05f;
        float filtered = mhi_noise_exponential(new_val, prev_val, 0.3f);
        printf("  raw=%.3f  filtered=%.3f\n", new_val, filtered);
        prev_val = filtered;
    }

    printf("\n=== Example complete ===\n");
    return 0;
}
