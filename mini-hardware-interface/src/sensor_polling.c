#include "sensor_polling.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ─── Bus Abstraction ───────────────────────────────────────────────── */

static mhi_bus_config_t s_bus;

int mhi_bus_init(const mhi_bus_config_t *cfg) {
    if (!cfg) return -1;
    s_bus = *cfg;
    return 0;
}

void mhi_bus_deinit(void) {
    memset(&s_bus, 0, sizeof(s_bus));
}

int mhi_bus_write_reg(uint8_t reg, const uint8_t *data, size_t len) {
    (void)reg; (void)data; (void)len;
    return 0;
}

int mhi_bus_read_reg(uint8_t reg, uint8_t *data, size_t len) {
    (void)reg;
    if (data && len > 0u) memset(data, 0, len);
    return 0;
}

int mhi_bus_write_read(const uint8_t *tx, size_t tx_len,
                       uint8_t *rx, size_t rx_len) {
    (void)tx; (void)tx_len;
    if (rx && rx_len > 0u) memset(rx, 0, rx_len);
    return 0;
}

/* ─── Polling Timer ─────────────────────────────────────────────────── */

int mhi_poll_timer_init(mhi_poll_timer_t *timer, uint32_t interval_ms,
                        mhi_timer_callback_t cb, void *user_data,
                        bool auto_reload) {
    if (!timer || interval_ms == 0u) return -1;
    timer->interval_ms   = interval_ms;
    timer->callback      = cb;
    timer->user_data     = user_data;
    timer->auto_reload   = auto_reload;
    timer->running       = false;
    timer->last_tick_ms  = 0u;
    return 0;
}

void mhi_poll_timer_start(mhi_poll_timer_t *timer) {
    if (timer) timer->running = true;
}

void mhi_poll_timer_stop(mhi_poll_timer_t *timer) {
    if (timer) timer->running = false;
}

void mhi_poll_timer_tick(mhi_poll_timer_t *timer, uint32_t now_ms) {
    if (!timer || !timer->running || !timer->callback) return;
    if (now_ms - timer->last_tick_ms >= timer->interval_ms) {
        timer->last_tick_ms = now_ms;
        timer->callback(timer->user_data);
        if (!timer->auto_reload) timer->running = false;
    }
}

/* ─── Sensor IRQ (GPIO Interrupt) ───────────────────────────────────── */

int mhi_sensor_irq_init(mhi_sensor_irq_t *irq, uint8_t pin,
                        void (*cb)(uint8_t pin, void *user_data),
                        void *user_data, uint32_t debounce_ms) {
    if (!irq || !cb) return -1;
    irq->pin          = pin;
    irq->callback     = cb;
    irq->user_data    = user_data;
    irq->debounce_ms  = debounce_ms;
    irq->last_fire_ms = 0u;
    irq->enabled      = false;
    return 0;
}

void mhi_sensor_irq_enable(mhi_sensor_irq_t *irq) {
    if (irq) irq->enabled = true;
}

void mhi_sensor_irq_disable(mhi_sensor_irq_t *irq) {
    if (irq) irq->enabled = false;
}

bool mhi_sensor_irq_fire(mhi_sensor_irq_t *irq, uint32_t now_ms) {
    if (!irq || !irq->enabled || !irq->callback) return false;
    if (now_ms - irq->last_fire_ms < irq->debounce_ms) return false;
    irq->last_fire_ms = now_ms;
    irq->callback(irq->pin, irq->user_data);
    return true;
}

/* ─── Moving Average ────────────────────────────────────────────────── */

void mhi_ma_init(mhi_moving_average_t *ma, float *buffer, uint8_t size) {
    if (!ma || !buffer || size == 0u) return;
    ma->buffer = buffer;
    ma->size   = size;
    ma->index  = 0u;
    ma->count  = 0u;
    ma->sum    = 0.0f;
    memset(buffer, 0, sizeof(float) * size);
}

float mhi_ma_update(mhi_moving_average_t *ma, float value) {
    if (!ma || ma->size == 0u) return value;
    if (ma->count < ma->size) {
        ma->count++;
    } else {
        ma->sum -= ma->buffer[ma->index];
    }
    ma->buffer[ma->index] = value;
    ma->sum += value;
    ma->index = (uint8_t)((ma->index + 1u) % ma->size);
    return ma->sum / (float)ma->count;
}

void mhi_ma_reset(mhi_moving_average_t *ma) {
    if (!ma) return;
    ma->index = 0u;
    ma->count = 0u;
    ma->sum   = 0.0f;
    if (ma->buffer) memset(ma->buffer, 0, sizeof(float) * ma->size);
}

/* ─── Median Filter ─────────────────────────────────────────────────── */

static int float_cmp(const void *a, const void *b) {
    float fa = *(const float *)a;
    float fb = *(const float *)b;
    if (fa < fb) return -1;
    if (fa > fb) return  1;
    return 0;
}

void mhi_median_init(mhi_median_filter_t *mf, float *buffer,
                     float *sorted, uint8_t window) {
    if (!mf || !buffer || !sorted || window == 0u) return;
    mf->buffer = buffer;
    mf->sorted = sorted;
    mf->window = window;
    mf->index  = 0u;
    mf->count  = 0u;
    memset(buffer, 0, sizeof(float) * window);
    memset(sorted, 0, sizeof(float) * window);
}

float mhi_median_update(mhi_median_filter_t *mf, float value) {
    if (!mf || mf->window == 0u) return value;

    mf->buffer[mf->index] = value;
    mf->index = (uint8_t)((mf->index + 1u) % mf->window);
    if (mf->count < mf->window) mf->count++;

    memcpy(mf->sorted, mf->buffer, sizeof(float) * mf->window);
    qsort(mf->sorted, mf->window, sizeof(float), float_cmp);

    if (mf->count % 2u == 1u) {
        return mf->sorted[mf->count / 2u];
    } else {
        uint8_t mid = mf->count / 2u;
        return (mf->sorted[mid - 1u] + mf->sorted[mid]) * 0.5f;
    }
}

void mhi_median_reset(mhi_median_filter_t *mf) {
    if (!mf) return;
    mf->index = 0u;
    mf->count = 0u;
    if (mf->buffer) memset(mf->buffer, 0, sizeof(float) * mf->window);
    if (mf->sorted) memset(mf->sorted, 0, sizeof(float) * mf->window);
}

/* ─── Kalman 1D ─────────────────────────────────────────────────────── */

void mhi_kalman_1d_init(mhi_kalman_1d_t *kf, float init_value,
                        float process_noise, float measurement_noise) {
    if (!kf) return;
    kf->x = init_value;
    kf->p = 1.0f;
    kf->q = process_noise;
    kf->r = measurement_noise;
    kf->k = 0.0f;
}

float mhi_kalman_1d_update(mhi_kalman_1d_t *kf, float measurement) {
    if (!kf) return measurement;
    kf->p = kf->p + kf->q;
    kf->k = kf->p / (kf->p + kf->r);
    kf->x = kf->x + kf->k * (measurement - kf->x);
    kf->p = (1.0f - kf->k) * kf->p;
    return kf->x;
}

void mhi_kalman_1d_reset(mhi_kalman_1d_t *kf, float value) {
    if (!kf) return;
    kf->x = value;
    kf->p = 1.0f;
    kf->k = 0.0f;
}

/* ─── Calibration ───────────────────────────────────────────────────── */

void mhi_calibration_init(mhi_calibration_t *cal) {
    if (!cal) return;
    cal->zero     = 0.0f;
    cal->span     = 1.0f;
    cal->ref_zero = 0.0f;
    cal->ref_span = 1.0f;
}

void mhi_calibration_set_zero(mhi_calibration_t *cal,
                              float raw_val, float ref_val) {
    if (!cal) return;
    cal->zero     = raw_val;
    cal->ref_zero = ref_val;
    cal->span = (cal->ref_span - cal->ref_zero)
              / (cal->span != 0.0f ? cal->span : 1.0f);
}

void mhi_calibration_set_span(mhi_calibration_t *cal,
                              float raw_val, float ref_val) {
    if (!cal) return;
    float delta_raw = raw_val - cal->zero;
    float delta_ref = ref_val - cal->ref_zero;
    cal->span = (delta_raw != 0.0f) ? delta_ref / delta_raw : 1.0f;
    cal->ref_span = ref_val;
}

float mhi_calibration_apply(const mhi_calibration_t *cal, float raw) {
    if (!cal) return raw;
    return cal->ref_zero + cal->span * (raw - cal->zero);
}

/* ─── Sensor Manager (Round-Robin) ──────────────────────────────────── */

void mhi_sensor_manager_init(mhi_sensor_manager_t *mgr,
                             mhi_sensor_t *sensors, uint8_t count) {
    if (!mgr) return;
    mgr->sensors          = sensors;
    mgr->count            = count;
    mgr->current_index    = 0u;
    mgr->last_service_ms  = 0u;
    mgr->min_interval_ms  = 10u;
}

void mhi_sensor_manager_service(mhi_sensor_manager_t *mgr, uint32_t now_ms) {
    if (!mgr || mgr->count == 0u || !mgr->sensors) return;
    uint8_t tries = 0u;

    while (tries < mgr->count) {
        mhi_sensor_t *s = &mgr->sensors[mgr->current_index];
        if (s->read && (now_ms - s->last_poll_ms >= s->poll_interval_ms)) {
            s->read(s->ctx, s->raw_values, &s->value_count);
            s->last_poll_ms = now_ms;
            if (s->filtered_values && s->value_count != 0u) {
                memcpy(s->filtered_values, s->raw_values,
                       sizeof(float) * s->value_count);
            }
            break;
        }
        mgr->current_index = (uint8_t)((mgr->current_index + 1u)
                                        % mgr->count);
        tries++;
    }
    if (mgr->current_index < mgr->count)
        mgr->current_index = (uint8_t)((mgr->current_index + 1u)
                                        % mgr->count);
}

void mhi_sensor_manager_read_current(mhi_sensor_manager_t *mgr) {
    if (!mgr || mgr->count == 0u || !mgr->sensors) return;
    mhi_sensor_t *s = &mgr->sensors[mgr->current_index];
    if (s->read) {
        s->read(s->ctx, s->raw_values, &s->value_count);
        if (s->filtered_values && s->value_count != 0u) {
            memcpy(s->filtered_values, s->raw_values,
                   sizeof(float) * s->value_count);
        }
    }
}
