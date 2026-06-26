#ifndef MHI_SENSOR_POLLING_H
#define MHI_SENSOR_POLLING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Sensor Bus Abstraction ────────────────────────────────────────── */

typedef enum {
    MHI_BUS_I2C = 0,
    MHI_BUS_SPI = 1
} mhi_bus_type_t;

typedef struct {
    mhi_bus_type_t type;
    uint8_t  address;       /* I2C: 7-bit addr; SPI: CS pin */
    uint32_t speed_hz;      /* I2C: 100k/400k; SPI: 1M–20M */
    uint8_t  spi_mode;      /* 0–3: CPOL/CPHA */
    bool     spi_lsb_first;
} mhi_bus_config_t;

int  mhi_bus_init(const mhi_bus_config_t *cfg);
void mhi_bus_deinit(void);
int  mhi_bus_write_reg(uint8_t reg, const uint8_t *data, size_t len);
int  mhi_bus_read_reg(uint8_t reg, uint8_t *data, size_t len);
int  mhi_bus_write_read(const uint8_t *tx, size_t tx_len,
                        uint8_t *rx, size_t rx_len);

/* ─── Sensor Descriptor ─────────────────────────────────────────────── */

typedef enum {
    MHI_SENSOR_TEMP      = 0,
    MHI_SENSOR_HUMIDITY  = 1,
    MHI_SENSOR_PRESSURE  = 2,
    MHI_SENSOR_ACCEL     = 3,
    MHI_SENSOR_GYRO      = 4,
    MHI_SENSOR_MAG       = 5,
    MHI_SENSOR_LIGHT     = 6,
    MHI_SENSOR_DISTANCE  = 7,
    MHI_SENSOR_GAS       = 8,
    MHI_SENSOR_CUSTOM    = 9
} mhi_sensor_type_t;

typedef void (*mhi_sensor_read_fn)(void *sensor_ctx, float *values,
                                   uint8_t *count);
typedef void (*mhi_sensor_init_fn)(void *sensor_ctx);

typedef struct {
    uint8_t            id;
    mhi_sensor_type_t  type;
    const char        *name;
    mhi_bus_config_t   bus;
    void              *ctx;
    mhi_sensor_init_fn init;
    mhi_sensor_read_fn read;
    uint32_t           poll_interval_ms;  /* period for round-robin */
    uint32_t           last_poll_ms;      /* internal timestamp */
    uint8_t            value_count;
    float             *raw_values;        /* latest raw readings */
    float             *filtered_values;   /* after smoothing */
} mhi_sensor_t;

/* ─── Polling / Timer ───────────────────────────────────────────────── */

typedef void (*mhi_timer_callback_t)(void *user_data);

typedef struct {
    uint32_t            interval_ms;
    mhi_timer_callback_t callback;
    void               *user_data;
    bool                 auto_reload;
    bool                 running;
    uint32_t            last_tick_ms;
} mhi_poll_timer_t;

int  mhi_poll_timer_init(mhi_poll_timer_t *timer, uint32_t interval_ms,
                         mhi_timer_callback_t cb, void *user_data,
                         bool auto_reload);
void mhi_poll_timer_start(mhi_poll_timer_t *timer);
void mhi_poll_timer_stop(mhi_poll_timer_t *timer);
void mhi_poll_timer_tick(mhi_poll_timer_t *timer, uint32_t now_ms);

/* ─── GPIO Interrupt for Sensor Data-Ready ──────────────────────────── */

typedef struct {
    uint8_t               pin;
    mhi_gpio_callback_t  callback;  /* from gpio_pwm_adc.h */
    void                 *user_data;
    bool                  enabled;
    uint32_t              debounce_ms;
    uint32_t              last_fire_ms;
} mhi_sensor_irq_t;

int  mhi_sensor_irq_init(mhi_sensor_irq_t *irq, uint8_t pin,
                         void (*cb)(uint8_t pin, void *user_data),
                         void *user_data, uint32_t debounce_ms);
void mhi_sensor_irq_enable(mhi_sensor_irq_t *irq);
void mhi_sensor_irq_disable(mhi_sensor_irq_t *irq);
bool mhi_sensor_irq_fire(mhi_sensor_irq_t *irq, uint32_t now_ms);

/* ─── Data Smoothing ────────────────────────────────────────────────── */

typedef struct {
    float   *buffer;
    uint8_t  size;
    uint8_t  index;
    uint8_t  count;
    float    sum;
} mhi_moving_average_t;

void mhi_ma_init(mhi_moving_average_t *ma, float *buffer, uint8_t size);
float mhi_ma_update(mhi_moving_average_t *ma, float value);
void mhi_ma_reset(mhi_moving_average_t *ma);

/* Median filter: sliding window, returns median of last `window` values */
typedef struct {
    float   *buffer;
    float   *sorted;
    uint8_t  window;
    uint8_t  index;
    uint8_t  count;
} mhi_median_filter_t;

void mhi_median_init(mhi_median_filter_t *mf, float *buffer,
                     float *sorted, uint8_t window);
float mhi_median_update(mhi_median_filter_t *mf, float value);
void mhi_median_reset(mhi_median_filter_t *mf);

/* 1-D Kalman filter */
typedef struct {
    float x;       /* state estimate */
    float p;       /* error covariance */
    float q;       /* process noise covariance */
    float r;       /* measurement noise covariance */
    float k;       /* Kalman gain (internal) */
} mhi_kalman_1d_t;

void mhi_kalman_1d_init(mhi_kalman_1d_t *kf, float init_value,
                        float process_noise, float measurement_noise);
float mhi_kalman_1d_update(mhi_kalman_1d_t *kf, float measurement);
void mhi_kalman_1d_reset(mhi_kalman_1d_t *kf, float value);

/* ─── Sensor Calibration ────────────────────────────────────────────── */

typedef struct {
    float zero;    /* offset (raw value at zero reference) */
    float span;    /* scale factor */
    float ref_zero;  /* reference value when raw = zero_offset */
    float ref_span;  /* reference value when raw = span_point */
} mhi_calibration_t;

void mhi_calibration_init(mhi_calibration_t *cal);
void mhi_calibration_set_zero(mhi_calibration_t *cal,
                              float raw_val, float ref_val);
void mhi_calibration_set_span(mhi_calibration_t *cal,
                              float raw_val, float ref_val);
float mhi_calibration_apply(const mhi_calibration_t *cal, float raw);

/* ─── Round-Robin Sensor Manager ────────────────────────────────────── */

typedef struct {
    mhi_sensor_t  *sensors;
    uint8_t        count;
    uint8_t        current_index;
    uint32_t       last_service_ms;
    uint32_t       min_interval_ms;
} mhi_sensor_manager_t;

void mhi_sensor_manager_init(mhi_sensor_manager_t *mgr,
                             mhi_sensor_t *sensors, uint8_t count);
void mhi_sensor_manager_service(mhi_sensor_manager_t *mgr, uint32_t now_ms);
void mhi_sensor_manager_read_current(mhi_sensor_manager_t *mgr);

#ifdef __cplusplus
}
#endif

#endif /* MHI_SENSOR_POLLING_H */
