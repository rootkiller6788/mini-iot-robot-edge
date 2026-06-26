#include "pid_controller.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static float sign_float(float x) {
    return (x > 0.0f) ? 1.0f : ((x < 0.0f) ? -1.0f : 0.0f);
}

static float clamp_float(float val, float min, float max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

/* ─── Single PID ────────────────────────────────────────────────────── */

void mhi_pid_init(mhi_pid_t *pid, const mhi_pid_config_t *config) {
    if (!pid || !config) return;
    pid->config = *config;
    mhi_pid_reset(pid);
}

void mhi_pid_reset(mhi_pid_t *pid) {
    if (!pid) return;
    pid->integral           = 0.0f;
    pid->prev_error         = 0.0f;
    pid->prev_measurement   = 0.0f;
    pid->derivative_filtered = 0.0f;
    pid->output             = 0.0f;
    pid->feed_forward       = 0.0f;
    pid->p_term             = 0.0f;
    pid->i_term             = 0.0f;
    pid->d_term             = 0.0f;
    pid->ff_term            = 0.0f;
    pid->saturated          = false;
    pid->last_time_ms       = 0u;
}

float mhi_pid_compute(mhi_pid_t *pid, float setpoint,
                      float measurement, uint32_t now_ms) {
    if (!pid) return 0.0f;

    float dt = 0.0f;
    if (pid->last_time_ms != 0u && now_ms > pid->last_time_ms) {
        dt = (float)(now_ms - pid->last_time_ms) * 0.001f;
    }
    pid->last_time_ms = now_ms;

    if (dt <= 0.0f) return pid->output;

    float error = setpoint - measurement;

    if (pid->config.direction == MHI_PID_REVERSE)
        error = -error;

    /* ── Proportional term ── */
    pid->p_term = pid->config.kp * error;

    /* ── Integral term (with anti-windup) ── */
    float new_integral = pid->integral;
    new_integral += pid->config.ki * error * dt;

    pid->i_term = new_integral;
    pid->integral = new_integral;

    /* ── Derivative term (derivative on measurement to avoid kick) ── */
    float deriv_input = -measurement;
    float raw_deriv = 0.0f;
    if (dt > 1e-6f) {
        raw_deriv = (deriv_input - pid->prev_measurement) / dt;
    }
    pid->prev_measurement = measurement;

    /* D-term low-pass filter */
    float a = pid->config.derivative_filter_a;
    pid->derivative_filtered = a * raw_deriv
                              + (1.0f - a) * pid->derivative_filtered;
    pid->d_term = pid->config.kd * pid->derivative_filtered;

    /* ── Feed-forward ── */
    pid->ff_term = pid->feed_forward;

    /* ── Output summation ── */
    float output = pid->p_term + pid->i_term + pid->d_term + pid->ff_term;

    /* ── Anti-windup ── */
    bool sat = false;
    if (output > pid->config.output_max || output < pid->config.output_min) {
        sat = true;
    }

    float pre_clamp = output;
    output = clamp_float(output, pid->config.output_min,
                         pid->config.output_max);
    pid->output = output;
    pid->saturated = sat;

    /* Back-calculation anti-windup */
    if (sat && (pid->config.anti_windup == MHI_PID_ANTI_WINDUP_BACK_CALC
              || pid->config.anti_windup == MHI_PID_ANTI_WINDUP_BOTH)) {
        float out_diff = pre_clamp - output;
        pid->integral += pid->config.back_calc_gain * out_diff * dt;
        pid->integral = clamp_float(pid->integral,
            pid->config.output_min, pid->config.output_max);
        pid->i_term = pid->integral;
    }

    /* Clamping anti-windup */
    if (pid->config.anti_windup == MHI_PID_ANTI_WINDUP_CLAMPING
        || pid->config.anti_windup == MHI_PID_ANTI_WINDUP_BOTH) {
        pid->integral = clamp_float(pid->integral,
            pid->config.output_min, pid->config.output_max);
        pid->i_term = pid->integral;
    }

    pid->prev_error = error;
    return output;
}

void mhi_pid_set_gains(mhi_pid_t *pid, float kp, float ki, float kd) {
    if (!pid) return;
    pid->config.kp = kp;
    pid->config.ki = ki;
    pid->config.kd = kd;
}

void mhi_pid_set_output_limits(mhi_pid_t *pid, float min, float max) {
    if (!pid) return;
    pid->config.output_min = min;
    pid->config.output_max = max;
}

void mhi_pid_set_feed_forward(mhi_pid_t *pid, float ff) {
    if (!pid) return;
    pid->feed_forward = ff;
}

float mhi_pid_get_output(const mhi_pid_t *pid) {
    return pid ? pid->output : 0.0f;
}

float mhi_pid_get_p_term(const mhi_pid_t *pid) {
    return pid ? pid->p_term : 0.0f;
}

float mhi_pid_get_i_term(const mhi_pid_t *pid) {
    return pid ? pid->i_term : 0.0f;
}

float mhi_pid_get_d_term(const mhi_pid_t *pid) {
    return pid ? pid->d_term : 0.0f;
}

bool mhi_pid_is_saturated(const mhi_pid_t *pid) {
    return pid ? pid->saturated : false;
}

/* ─── Ziegler-Nichols Auto-Tuning ───────────────────────────────────── */

void mhi_zn_tune(mhi_zn_type_t type, float ku, float tu,
                 mhi_zn_result_t *result) {
    if (!result || ku <= 0.0f || tu <= 0.0f) return;

    switch (type) {
    case MHI_ZN_TYPE_P:
        result->kp = 0.5f * ku;
        result->ki = 0.0f;
        result->kd = 0.0f;
        break;
    case MHI_ZN_TYPE_PI:
        result->kp = 0.45f * ku;
        result->ki = 0.54f * ku / tu;
        result->kd = 0.0f;
        break;
    case MHI_ZN_TYPE_PID:
    default:
        result->kp = 0.6f  * ku;
        result->ki = 1.2f  * ku / tu;
        result->kd = 0.075f * ku * tu;
        break;
    }
    result->ultimate_gain   = ku;
    result->ultimate_period_s = tu;
}

/* ─── Cascaded PID ──────────────────────────────────────────────────── */

void mhi_cascaded_pid_init(mhi_cascaded_pid_t *cpid,
                           const mhi_pid_config_t *outer_cfg,
                           const mhi_pid_config_t *inner_cfg) {
    if (!cpid) return;
    if (outer_cfg) mhi_pid_init(&cpid->outer, outer_cfg);
    if (inner_cfg) mhi_pid_init(&cpid->inner, inner_cfg);
    cpid->inner_setpoint = 0.0f;
    cpid->enabled        = true;
}

void mhi_cascaded_pid_reset(mhi_cascaded_pid_t *cpid) {
    if (!cpid) return;
    mhi_pid_reset(&cpid->outer);
    mhi_pid_reset(&cpid->inner);
    cpid->inner_setpoint = 0.0f;
}

float mhi_cascaded_pid_compute(mhi_cascaded_pid_t *cpid,
                               float outer_setpoint,
                               float outer_measurement,
                               float inner_measurement,
                               uint32_t now_ms) {
    if (!cpid || !cpid->enabled) return 0.0f;

    float outer_out = mhi_pid_compute(&cpid->outer, outer_setpoint,
                                      outer_measurement, now_ms);

    cpid->inner_setpoint = outer_out;

    float inner_out = mhi_pid_compute(&cpid->inner, cpid->inner_setpoint,
                                      inner_measurement, now_ms);

    cpid->inner.output = inner_out;
    return inner_out;
}

void mhi_cascaded_pid_set_output_limits(mhi_cascaded_pid_t *cpid,
                                        float min, float max) {
    if (!cpid) return;
    mhi_pid_set_output_limits(&cpid->outer, min, max);
    mhi_pid_set_output_limits(&cpid->inner, min, max);
}
