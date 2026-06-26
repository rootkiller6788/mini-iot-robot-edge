#ifndef MHI_SIGNAL_COND_H
#define MHI_SIGNAL_COND_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Op-Amp Simulation ─────────────────────────────────────────────── */

typedef enum {
    MHI_OPAMP_INVERTING      = 0,
    MHI_OPAMP_NON_INVERTING  = 1,
    MHI_OPAMP_DIFFERENTIAL   = 2,
    MHI_OPAMP_INSTRUMENTATION = 3
} mhi_opamp_topology_t;

typedef struct {
    mhi_opamp_topology_t topology;
    float r1;          /* input resistor */
    float r2;          /* feedback resistor */
    float r3;          /* (differential: R3=R1, R4=R2) */
    float r4;
    float rg;          /* instrumentation amp gain resistor */
} mhi_opamp_config_t;

typedef struct {
    mhi_opamp_config_t config;
    float input_offset_v;
    float vcc;         /* positive supply */
    float vee;         /* negative supply (or 0 for single-supply) */
    float saturation_min;
    float saturation_max;
} mhi_opamp_t;

void mhi_opamp_init(mhi_opamp_t *opamp, const mhi_opamp_config_t *cfg,
                    float vcc, float vee);

float mhi_opamp_inverting(const mhi_opamp_t *opamp, float vin);
float mhi_opamp_non_inverting(const mhi_opamp_t *opamp, float vin);
float mhi_opamp_differential(const mhi_opamp_t *opamp,
                             float vp, float vn);
float mhi_opamp_instrumentation(const mhi_opamp_t *opamp,
                                float vp, float vn);
float mhi_opamp_compute(const mhi_opamp_t *opamp, float vp, float vn);
float mhi_opamp_clamp(const mhi_opamp_t *opamp, float vout);

/* ─── Filters ───────────────────────────────────────────────────────── */

typedef enum {
    MHI_FILTER_LOWPASS  = 0,
    MHI_FILTER_HIGHPASS = 1
} mhi_filter_type_t;

typedef enum {
    MHI_FILTER_ORDER_1ST = 1,
    MHI_FILTER_ORDER_2ND = 2
} mhi_filter_order_t;

/* RC passive filter (discrete-time IIR) */
typedef struct {
    mhi_filter_type_t  type;
    float rc;         /* time constant = R * C (seconds) */
    float sample_time_s;
    float alpha;      /* smoothing factor 0–1 */
    float prev_out;
    float prev_in;
} mhi_rc_filter_t;

void mhi_rc_filter_init(mhi_rc_filter_t *f, mhi_filter_type_t type,
                        float r_ohm, float c_farad, float sample_time_s);
float mhi_rc_filter_update(mhi_rc_filter_t *f, float input);
void mhi_rc_filter_reset(mhi_rc_filter_t *f);
float mhi_rc_filter_cutoff_freq(const mhi_rc_filter_t *f);
float mhi_rc_design_c(float cutoff_hz, float r_ohm);
float mhi_rc_design_r(float cutoff_hz, float c_farad);

/* 2nd-order active filter (Sallen-Key topology, discrete-time) */
typedef struct {
    mhi_filter_type_t  type;
    float q_factor;          /* quality factor: 0.707 for Butterworth */
    float cutoff_hz;
    float sample_time_s;
    float a1, a2;            /* denominator coefficients */
    float b0, b1, b2;        /* numerator coefficients */
    float x1, x2;            /* input history */
    float y1, y2;            /* output history */
} mhi_filter_2nd_t;

void mhi_filter_2nd_init(mhi_filter_2nd_t *f, mhi_filter_type_t type,
                         float cutoff_hz, float q_factor, float sample_hz);
float mhi_filter_2nd_update(mhi_filter_2nd_t *f, float input);
void mhi_filter_2nd_reset(mhi_filter_2nd_t *f);

/* ─── Noise Filtering ───────────────────────────────────────────────── */

float mhi_noise_simple_average(const float *buffer, uint8_t len);
float mhi_noise_exponential(float current, float prev, float alpha);
float mhi_noise_spike_removal(float value, float prev, float max_delta);
float mhi_noise_threshold_deadband(float value, float prev, float threshold);

/* ─── Voltage Level Shifting ────────────────────────────────────────── */

typedef struct {
    float r1;          /* resistor from input to output */
    float r2;          /* resistor from output to Vref (divider) */
    float vref;
} mhi_level_shift_t;

void mhi_level_shift_init(mhi_level_shift_t *ls, float r1, float r2,
                          float vref);
float mhi_level_shift_apply(const mhi_level_shift_t *ls, float vin);
void mhi_level_shift_design(mhi_level_shift_t *ls,
                            float vin_min, float vin_max,
                            float vout_min, float vout_max,
                            float vref);

/* ─── Wheatstone Bridge (Strain Gauge) ──────────────────────────────── */

typedef enum {
    MHI_WHEATSTONE_QUARTER  = 0,
    MHI_WHEATSTONE_HALF     = 1,
    MHI_WHEATSTONE_FULL     = 2
} mhi_wheatstone_config_t;

typedef struct {
    mhi_wheatstone_config_t config;
    float excitation_v;
    float gauge_factor;    /* GF ≈ 2.0 for metal foil */
    float nominal_resistance; /* e.g. 120, 350, 1000 ohms */
    float bridge_r1, bridge_r2, bridge_r3; /* completion resistors */
} mhi_wheatstone_t;

void mhi_wheatstone_init(mhi_wheatstone_t *wb,
                         mhi_wheatstone_config_t config,
                         float v_excitation, float gauge_factor,
                         float nominal_r);
float mhi_wheatstone_output_voltage(const mhi_wheatstone_t *wb, float strain);
float mhi_wheatstone_strain_from_voltage(const mhi_wheatstone_t *wb,
                                         float v_out);
float mhi_wheatstone_resistance_change(const mhi_wheatstone_t *wb,
                                       float strain);

/* ─── Optocoupler Isolation ─────────────────────────────────────────── */

typedef struct {
    float ctr;              /* current transfer ratio (e.g. 0.5–6.0) */
    float led_vf;           /* LED forward voltage */ 
    float led_if_max_ma;    /* max LED forward current */
    float output_vcc;
    float output_r_pullup;
} mhi_optocoupler_t;

void mhi_optocoupler_init(mhi_optocoupler_t *oc,
                          float ctr, float led_vf,
                          float led_if_max_ma, float output_vcc,
                          float r_pullup);
float mhi_optocoupler_led_resistor(float vin, float desired_if_ma,
                                   float led_vf);
float mhi_optocoupler_output_voltage(const mhi_optocoupler_t *oc,
                                     float led_if_ma);
bool mhi_optocoupler_output_logic(const mhi_optocoupler_t *oc,
                                  float led_if_ma, float vih_threshold);

/* ─── ADC Anti-Aliasing Filter Design ───────────────────────────────── */

typedef struct {
    float sample_rate_hz;      /* ADC sampling rate */
    float cutoff_hz;           /* target cutoff (≤ sample_rate / 2) */
    float stopband_hz;
    float passband_ripple_db;
    float stopband_atten_db;
    int   order;
    float r;                   /* computed R */
    float c;                   /* computed C */
} mhi_anti_alias_t;

void mhi_anti_alias_design(mhi_anti_alias_t *aa, float sample_rate_hz,
                           float cutoff_hz, int order);
float mhi_anti_alias_attenuation(const mhi_anti_alias_t *aa, float freq_hz);
void mhi_anti_alias_design_rc(mhi_anti_alias_t *aa, float r_ohm);

#ifdef __cplusplus
}
#endif

#endif /* MHI_SIGNAL_COND_H */
