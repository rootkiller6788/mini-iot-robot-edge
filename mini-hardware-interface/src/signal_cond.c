#include "signal_cond.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* ─── Op-Amp ────────────────────────────────────────────────────────── */

void mhi_opamp_init(mhi_opamp_t *opamp, const mhi_opamp_config_t *cfg,
                    float vcc, float vee) {
    if (!opamp || !cfg) return;
    opamp->config          = *cfg;
    opamp->input_offset_v  = 0.0f;
    opamp->vcc             = vcc;
    opamp->vee             = vee;
    opamp->saturation_min  = vee + 0.1f;
    opamp->saturation_max  = vcc - 0.1f;
}

float mhi_opamp_inverting(const mhi_opamp_t *opamp, float vin) {
    if (!opamp || opamp->config.r1 <= 0.0f) return 0.0f;
    float gain = -opamp->config.r2 / opamp->config.r1;
    return mhi_opamp_clamp(opamp, gain * vin + opamp->input_offset_v);
}

float mhi_opamp_non_inverting(const mhi_opamp_t *opamp, float vin) {
    if (!opamp || opamp->config.r1 <= 0.0f) return 0.0f;
    float gain = 1.0f + opamp->config.r2 / opamp->config.r1;
    return mhi_opamp_clamp(opamp, gain * vin + opamp->input_offset_v);
}

float mhi_opamp_differential(const mhi_opamp_t *opamp,
                             float vp, float vn) {
    if (!opamp || opamp->config.r1 <= 0.0f) return 0.0f;
    float gain = opamp->config.r2 / opamp->config.r1;
    return mhi_opamp_clamp(opamp, gain * (vp - vn) + opamp->input_offset_v);
}

float mhi_opamp_instrumentation(const mhi_opamp_t *opamp,
                                float vp, float vn) {
    if (!opamp || opamp->config.rg <= 0.0f || opamp->config.r1 <= 0.0f)
        return 0.0f;
    float gain = 1.0f + (2.0f * opamp->config.r1) / opamp->config.rg;
    if (opamp->config.r3 > 0.0f && opamp->config.r4 > 0.0f) {
        gain *= opamp->config.r3 / opamp->config.r4;
    }
    return mhi_opamp_clamp(opamp, gain * (vp - vn) + opamp->input_offset_v);
}

float mhi_opamp_compute(const mhi_opamp_t *opamp, float vp, float vn) {
    if (!opamp) return 0.0f;
    switch (opamp->config.topology) {
    case MHI_OPAMP_INVERTING:
        return mhi_opamp_inverting(opamp, vn);
    case MHI_OPAMP_NON_INVERTING:
        return mhi_opamp_non_inverting(opamp, vp);
    case MHI_OPAMP_DIFFERENTIAL:
        return mhi_opamp_differential(opamp, vp, vn);
    case MHI_OPAMP_INSTRUMENTATION:
        return mhi_opamp_instrumentation(opamp, vp, vn);
    default:
        return 0.0f;
    }
}

float mhi_opamp_clamp(const mhi_opamp_t *opamp, float vout) {
    if (!opamp) return vout;
    if (vout > opamp->saturation_max) return opamp->saturation_max;
    if (vout < opamp->saturation_min) return opamp->saturation_min;
    return vout;
}

/* ─── RC Filter ─────────────────────────────────────────────────────── */

void mhi_rc_filter_init(mhi_rc_filter_t *f, mhi_filter_type_t type,
                        float r_ohm, float c_farad, float sample_time_s) {
    if (!f) return;
    f->type          = type;
    f->rc            = r_ohm * c_farad;
    f->sample_time_s = sample_time_s;
    f->alpha         = f->rc / (f->rc + sample_time_s);
    if (f->alpha > 1.0f) f->alpha = 1.0f;
    if (f->alpha < 0.0f) f->alpha = 0.0f;
    f->prev_out = 0.0f;
    f->prev_in  = 0.0f;
}

float mhi_rc_filter_update(mhi_rc_filter_t *f, float input) {
    if (!f) return input;
    float output;
    if (f->type == MHI_FILTER_LOWPASS) {
        output = f->prev_out + f->alpha * (input - f->prev_out);
    } else {
        output = f->alpha * (f->prev_out + input - f->prev_in);
    }
    f->prev_out = output;
    f->prev_in  = input;
    return output;
}

void mhi_rc_filter_reset(mhi_rc_filter_t *f) {
    if (!f) return;
    f->prev_out = 0.0f;
    f->prev_in  = 0.0f;
}

float mhi_rc_filter_cutoff_freq(const mhi_rc_filter_t *f) {
    if (!f || f->rc <= 0.0f) return 0.0f;
    return 1.0f / (2.0f * (float)M_PI * f->rc);
}

float mhi_rc_design_c(float cutoff_hz, float r_ohm) {
    if (cutoff_hz <= 0.0f || r_ohm <= 0.0f) return 0.0f;
    return 1.0f / (2.0f * (float)M_PI * r_ohm * cutoff_hz);
}

float mhi_rc_design_r(float cutoff_hz, float c_farad) {
    if (cutoff_hz <= 0.0f || c_farad <= 0.0f) return 0.0f;
    return 1.0f / (2.0f * (float)M_PI * c_farad * cutoff_hz);
}

/* ─── 2nd Order Active Filter (bilinear transform, Sallen-Key) ─────── */

static float sallen_key_lowpass_a1(float q)  { return 1.0f / q; }
static float sallen_key_lowpass_a2(float q)  { (void)q; return 1.0f; }
static float sallen_key_highpass_a1(float q) { return 1.0f / q; }
static float sallen_key_highpass_a2(float q) { (void)q; return 1.0f; }

void mhi_filter_2nd_init(mhi_filter_2nd_t *f, mhi_filter_type_t type,
                         float cutoff_hz, float q_factor, float sample_hz) {
    if (!f) return;
    memset(f, 0, sizeof(*f));
    f->type          = type;
    f->cutoff_hz     = cutoff_hz;
    f->q_factor      = q_factor;
    f->sample_time_s = 1.0f / sample_hz;

    float w0 = 2.0f * (float)M_PI * cutoff_hz;
    float T  = f->sample_time_s;

    /* pre-warp */
    float w0_warped = (2.0f / T) * tanf(w0 * T * 0.5f);

    float wT = w0_warped * T;
    float wT2 = wT * wT;

    float den = 4.0f + 2.0f * sallen_key_lowpass_a1(q_factor) * wT + wT2;
    if (den == 0.0f) den = 1e-9f;

    if (type == MHI_FILTER_LOWPASS) {
        f->b0 = wT2 / den;
        f->b1 = 2.0f * wT2 / den;
        f->b2 = wT2 / den;
    } else {
        f->b0 = 4.0f / den;
        f->b1 = -8.0f / den;
        f->b2 = 4.0f / den;
    }

    f->a1 = (2.0f * wT2 - 8.0f) / den;
    f->a2 = (4.0f - 2.0f * sallen_key_lowpass_a1(q_factor) * wT + wT2) / den;
}

float mhi_filter_2nd_update(mhi_filter_2nd_t *f, float input) {
    if (!f) return input;

    float output = f->b0 * input + f->b1 * f->x1 + f->b2 * f->x2
                  - f->a1 * f->y1 - f->a2 * f->y2;

    f->x2 = f->x1;
    f->x1 = input;
    f->y2 = f->y1;
    f->y1 = output;

    return output;
}

void mhi_filter_2nd_reset(mhi_filter_2nd_t *f) {
    if (!f) return;
    f->x1 = f->x2 = 0.0f;
    f->y1 = f->y2 = 0.0f;
}

/* ─── Noise Filtering ───────────────────────────────────────────────── */

float mhi_noise_simple_average(const float *buffer, uint8_t len) {
    if (!buffer || len == 0u) return 0.0f;
    float sum = 0.0f;
    uint8_t i;
    for (i = 0u; i < len; i++) {
        sum += buffer[i];
    }
    return sum / (float)len;
}

float mhi_noise_exponential(float current, float prev, float alpha) {
    return alpha * current + (1.0f - alpha) * prev;
}

float mhi_noise_spike_removal(float value, float prev, float max_delta) {
    if (fabsf(value - prev) > max_delta)
        return prev;
    return value;
}

float mhi_noise_threshold_deadband(float value, float prev, float threshold) {
    if (fabsf(value - prev) < threshold)
        return prev;
    return value;
}

/* ─── Voltage Level Shifting ────────────────────────────────────────── */

void mhi_level_shift_init(mhi_level_shift_t *ls, float r1, float r2,
                          float vref) {
    if (!ls) return;
    ls->r1   = r1;
    ls->r2   = r2;
    ls->vref = vref;
}

float mhi_level_shift_apply(const mhi_level_shift_t *ls, float vin) {
    if (!ls || ls->r1 + ls->r2 <= 0.0f) return 0.0f;
    /* Vout = Vin * R2/(R1+R2) + Vref * R1/(R1+R2) */
    float den = ls->r1 + ls->r2;
    return vin * (ls->r2 / den) + ls->vref * (ls->r1 / den);
}

void mhi_level_shift_design(mhi_level_shift_t *ls,
                            float vin_min, float vin_max,
                            float vout_min, float vout_max,
                            float vref) {
    if (!ls) return;
    /* R1/R2 = (vin_max - vin_min) / (vout_max - vout_min) */
    float ratio = (vin_max - vin_min) / (vout_max - vout_min);
    ls->r2 = 10000.0f; /* choose R2 = 10k */
    ls->r1 = ratio * ls->r2;
    ls->vref = vref;
}

/* ─── Wheatstone Bridge ─────────────────────────────────────────────── */

void mhi_wheatstone_init(mhi_wheatstone_t *wb,
                         mhi_wheatstone_config_t config,
                         float v_excitation, float gauge_factor,
                         float nominal_r) {
    if (!wb) return;
    wb->config            = config;
    wb->excitation_v      = v_excitation;
    wb->gauge_factor      = gauge_factor;
    wb->nominal_resistance = nominal_r;
    wb->bridge_r1 = nominal_r;
    wb->bridge_r2 = nominal_r;
    wb->bridge_r3 = nominal_r;
}

float mhi_wheatstone_output_voltage(const mhi_wheatstone_t *wb, float strain) {
    if (!wb) return 0.0f;

    float delta_r = strain * wb->gauge_factor * wb->nominal_resistance;

    if (wb->config == MHI_WHEATSTONE_QUARTER) {
        float vout = wb->excitation_v *
            (delta_r / (4.0f * wb->nominal_resistance + 2.0f * delta_r));
        return vout;
    } else if (wb->config == MHI_WHEATSTONE_HALF) {
        return wb->excitation_v * delta_r / (2.0f * wb->nominal_resistance);
    } else {
        return wb->excitation_v * delta_r / wb->nominal_resistance;
    }
}

float mhi_wheatstone_strain_from_voltage(const mhi_wheatstone_t *wb,
                                         float v_out) {
    if (!wb || wb->excitation_v <= 0.0f) return 0.0f;
    float v_ratio = v_out / wb->excitation_v;

    if (wb->config == MHI_WHEATSTONE_QUARTER) {
        return 4.0f * v_ratio / (wb->gauge_factor * (1.0f - 2.0f * v_ratio));
    } else if (wb->config == MHI_WHEATSTONE_HALF) {
        return 2.0f * v_ratio / wb->gauge_factor;
    } else {
        return v_ratio / wb->gauge_factor;
    }
}

float mhi_wheatstone_resistance_change(const mhi_wheatstone_t *wb,
                                       float strain) {
    if (!wb) return 0.0f;
    return strain * wb->gauge_factor * wb->nominal_resistance;
}

/* ─── Optocoupler ───────────────────────────────────────────────────── */

void mhi_optocoupler_init(mhi_optocoupler_t *oc,
                          float ctr, float led_vf,
                          float led_if_max_ma, float output_vcc,
                          float r_pullup) {
    if (!oc) return;
    oc->ctr             = ctr;
    oc->led_vf          = led_vf;
    oc->led_if_max_ma   = led_if_max_ma;
    oc->output_vcc      = output_vcc;
    oc->output_r_pullup = r_pullup;
}

float mhi_optocoupler_led_resistor(float vin, float desired_if_ma,
                                   float led_vf) {
    if (desired_if_ma <= 0.0f) return 1e9f;
    return (vin - led_vf) / (desired_if_ma * 0.001f);
}

float mhi_optocoupler_output_voltage(const mhi_optocoupler_t *oc,
                                     float led_if_ma) {
    if (!oc) return 0.0f;
    float ic = led_if_ma * 0.001f * oc->ctr; /* Ic = If * CTR */
    float v_drop = ic * oc->output_r_pullup;
    float vout = oc->output_vcc - v_drop;
    if (vout < 0.0f) vout = 0.0f;
    return vout;
}

bool mhi_optocoupler_output_logic(const mhi_optocoupler_t *oc,
                                  float led_if_ma, float vih_threshold) {
    float vout = mhi_optocoupler_output_voltage(oc, led_if_ma);
    return vout > vih_threshold;
}

/* ─── Anti-Aliasing Filter Design ───────────────────────────────────── */

void mhi_anti_alias_design(mhi_anti_alias_t *aa, float sample_rate_hz,
                           float cutoff_hz, int order) {
    if (!aa) return;
    aa->sample_rate_hz = sample_rate_hz;
    aa->cutoff_hz      = cutoff_hz;
    aa->order          = order;
    aa->stopband_hz    = sample_rate_hz - cutoff_hz;
    aa->passband_ripple_db = 0.1f;
    aa->stopband_atten_db  = 40.0f;

    /* default: choose C = 0.1uF, compute R */
    aa->c = 1e-7f; /* 0.1µF */
    aa->r = 1.0f / (2.0f * (float)M_PI * cutoff_hz * aa->c);
}

float mhi_anti_alias_attenuation(const mhi_anti_alias_t *aa, float freq_hz) {
    if (!aa || aa->cutoff_hz <= 0.0f) return 0.0f;
    float ratio = freq_hz / aa->cutoff_hz;
    /* single-pole RC roll-off: -20dB/dec × order */
    float att_db = -20.0f * (float)aa->order * log10f(ratio);
    if (att_db > 0.0f) att_db = 0.0f;
    return att_db;
}

void mhi_anti_alias_design_rc(mhi_anti_alias_t *aa, float r_ohm) {
    if (!aa || r_ohm <= 0.0f) return;
    aa->r = r_ohm;
    aa->c = 1.0f / (2.0f * (float)M_PI * aa->cutoff_hz * r_ohm);
}
