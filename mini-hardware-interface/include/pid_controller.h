#ifndef MHI_PID_CONTROLLER_H
#define MHI_PID_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── PID Modes ─────────────────────────────────────────────────────── */

typedef enum {
    MHI_PID_FORM_POSITIONAL = 0,
    MHI_PID_FORM_VELOCITY   = 1
} mhi_pid_form_t;

typedef enum {
    MHI_PID_ANTI_WINDUP_NONE          = 0,
    MHI_PID_ANTI_WINDUP_CLAMPING      = 1,
    MHI_PID_ANTI_WINDUP_BACK_CALC     = 2,
    MHI_PID_ANTI_WINDUP_BOTH          = 3
} mhi_pid_anti_windup_t;

typedef enum {
    MHI_PID_DIRECT   = 0,   /* output increases with error */
    MHI_PID_REVERSE  = 1    /* output decreases with error */
} mhi_pid_direction_t;

/* ─── PID Configuration ─────────────────────────────────────────────── */

typedef struct {
    float kp;                  /* proportional gain */
    float ki;                  /* integral gain (= Kp / Ti) */
    float kd;                  /* derivative gain (= Kp * Td) */
    float setpoint;
    float sample_time_s;       /* control loop period in seconds */
    float output_min;
    float output_max;
    mhi_pid_form_t         form;
    mhi_pid_anti_windup_t  anti_windup;
    mhi_pid_direction_t    direction;
    float back_calc_gain;       /* Kb for back-calculation anti-windup */
    float derivative_filter_a;  /* D-term low-pass filter coefficient (0–1) */
} mhi_pid_config_t;

/* ─── PID State ─────────────────────────────────────────────────────── */

typedef struct {
    mhi_pid_config_t config;

    /* internal state */
    float integral;
    float prev_error;
    float prev_measurement;
    float derivative_filtered;
    float output;
    float feed_forward;

    /* intermediate terms (for diagnostics) */
    float p_term;
    float i_term;
    float d_term;
    float ff_term;

    bool  saturated;
    uint32_t last_time_ms;
} mhi_pid_t;

void mhi_pid_init(mhi_pid_t *pid, const mhi_pid_config_t *config);
void mhi_pid_reset(mhi_pid_t *pid);
float mhi_pid_compute(mhi_pid_t *pid, float setpoint,
                      float measurement, uint32_t now_ms);
void mhi_pid_set_gains(mhi_pid_t *pid, float kp, float ki, float kd);
void mhi_pid_set_output_limits(mhi_pid_t *pid, float min, float max);
void mhi_pid_set_feed_forward(mhi_pid_t *pid, float ff);
float mhi_pid_get_output(const mhi_pid_t *pid);
float mhi_pid_get_p_term(const mhi_pid_t *pid);
float mhi_pid_get_i_term(const mhi_pid_t *pid);
float mhi_pid_get_d_term(const mhi_pid_t *pid);
bool mhi_pid_is_saturated(const mhi_pid_t *pid);

/* ─── Ziegler-Nichols Auto-Tuning ───────────────────────────────────── */

typedef enum {
    MHI_ZN_TYPE_P   = 0,
    MHI_ZN_TYPE_PI  = 1,
    MHI_ZN_TYPE_PID = 2
} mhi_zn_type_t;

typedef struct {
    float ultimate_gain;      /* Ku */
    float ultimate_period_s;  /* Tu */
    float kp;
    float ki;
    float kd;
} mhi_zn_result_t;

void mhi_zn_tune(mhi_zn_type_t type, float ku, float tu,
                 mhi_zn_result_t *result);

/* ─── Cascaded PID ──────────────────────────────────────────────────── */

typedef struct {
    mhi_pid_t outer;   /* position / angle loop */
    mhi_pid_t inner;   /* velocity / rate loop */
    float inner_setpoint;
    bool    enabled;
} mhi_cascaded_pid_t;

void mhi_cascaded_pid_init(mhi_cascaded_pid_t *cpid,
                           const mhi_pid_config_t *outer_cfg,
                           const mhi_pid_config_t *inner_cfg);
void mhi_cascaded_pid_reset(mhi_cascaded_pid_t *cpid);
float mhi_cascaded_pid_compute(mhi_cascaded_pid_t *cpid,
                               float outer_setpoint,
                               float outer_measurement,
                               float inner_measurement,
                               uint32_t now_ms);
void mhi_cascaded_pid_set_output_limits(mhi_cascaded_pid_t *cpid,
                                        float min, float max);

#ifdef __cplusplus
}
#endif

#endif /* MHI_PID_CONTROLLER_H */
