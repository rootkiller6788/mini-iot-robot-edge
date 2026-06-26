#ifndef MHI_ACTUATOR_DRIVER_H
#define MHI_ACTUATOR_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Brushed DC Motor ──────────────────────────────────────────────── */

typedef enum {
    MHI_DC_DIR_CW    = 0,   /* clockwise */
    MHI_DC_DIR_CCW   = 1,   /* counter-clockwise */
    MHI_DC_DIR_BRAKE = 2,   /* short-brake */
    MHI_DC_DIR_COAST = 3    /* free-wheeling */
} mhi_dc_direction_t;

typedef struct {
    uint8_t pwm_channel;     /* PWM output pin */
    uint8_t in1_pin;         /* H-bridge IN1 */
    uint8_t in2_pin;         /* H-bridge IN2 */
    uint8_t enable_pin;      /* optional enable (255 = none) */
    float   max_current_a;
    float   current_limit_a;
    float   soft_start_ramp_s;   /* time to reach full speed */
    float   current_speed;       /* -1.0 to 1.0 */
    mhi_dc_direction_t direction;
    uint32_t ramp_start_ms;
    bool    emergency_stop;
    bool    enabled;
} mhi_dc_motor_t;

int  mhi_dc_motor_init(mhi_dc_motor_t *motor, uint8_t pwm_ch,
                       uint8_t in1, uint8_t in2);
void mhi_dc_motor_deinit(mhi_dc_motor_t *motor);
int  mhi_dc_motor_set_speed(mhi_dc_motor_t *motor, float speed);
int  mhi_dc_motor_set_direction(mhi_dc_motor_t *motor, mhi_dc_direction_t dir);
int  mhi_dc_motor_brake(mhi_dc_motor_t *motor);
int  mhi_dc_motor_coast(mhi_dc_motor_t *motor);
void mhi_dc_motor_emergency_stop(mhi_dc_motor_t *motor);
void mhi_dc_motor_resume(mhi_dc_motor_t *motor);
void mhi_dc_motor_tick(mhi_dc_motor_t *motor, uint32_t now_ms);

/* ─── Brushless DC Motor (3-phase + hall sensors) ───────────────────── */

typedef struct {
    uint8_t hall_a;    /* hall sensor A input pin */
    uint8_t hall_b;    /* hall sensor B input pin */
    uint8_t hall_c;    /* hall sensor C input pin */
    uint8_t pwm_uh;    /* phase U high-side */
    uint8_t pwm_ul;    /* phase U low-side */
    uint8_t pwm_vh;    /* phase V high-side */
    uint8_t pwm_vl;    /* phase V low-side */
    uint8_t pwm_wh;    /* phase W high-side */
    uint8_t pwm_wl;    /* phase W low-side */
} mhi_bldc_pins_t;

typedef struct {
    mhi_bldc_pins_t pins;
    float  target_speed;        /* -1.0 to 1.0 */
    float  current_speed;
    float  duty_cycle;          /* PWM duty 0–1.0 */
    float  max_current_a;
    uint8_t hall_state;
    uint8_t commutation_step;
    bool   enabled;
    bool   emergency_stop;
    uint32_t soft_start_ms;
    uint32_t ramp_start_ms;
} mhi_bldc_motor_t;

int  mhi_bldc_motor_init(mhi_bldc_motor_t *motor, const mhi_bldc_pins_t *pins);
void mhi_bldc_motor_deinit(mhi_bldc_motor_t *motor);
int  mhi_bldc_motor_set_speed(mhi_bldc_motor_t *motor, float speed);
int  mhi_bldc_motor_set_duty(mhi_bldc_motor_t *motor, float duty);
void mhi_bldc_motor_commutate(mhi_bldc_motor_t *motor);
void mhi_bldc_motor_emergency_stop(mhi_bldc_motor_t *motor);
void mhi_bldc_motor_tick(mhi_bldc_motor_t *motor, uint32_t now_ms);

/* ─── Stepper Motor ─────────────────────────────────────────────────── */

typedef enum {
    MHI_STEP_MODE_FULL      = 0,
    MHI_STEP_MODE_HALF      = 1,
    MHI_STEP_MODE_MICRO_4   = 2,
    MHI_STEP_MODE_MICRO_8   = 3,
    MHI_STEP_MODE_MICRO_16  = 4,
    MHI_STEP_MODE_MICRO_32  = 5
} mhi_step_mode_t;

typedef struct {
    uint8_t coil_a1;
    uint8_t coil_a2;
    uint8_t coil_b1;
    uint8_t coil_b2;
} mhi_stepper_pins_t;

typedef struct {
    mhi_stepper_pins_t pins;
    mhi_step_mode_t    mode;
    int32_t   position;         /* current step position */
    int32_t   target_position;
    float     speed_steps_per_s;
    uint32_t  step_interval_us; /* microseconds per step */
    uint8_t   micro_step_index;
    uint8_t   steps_per_revolution; /* 200 for 1.8° stepper */
    bool      enabled;
    bool      emergency_stop;
    uint32_t  last_step_us;
} mhi_stepper_motor_t;

int  mhi_stepper_motor_init(mhi_stepper_motor_t *motor,
                            const mhi_stepper_pins_t *pins,
                            uint16_t steps_per_rev);
void mhi_stepper_motor_deinit(mhi_stepper_motor_t *motor);
int  mhi_stepper_motor_set_mode(mhi_stepper_motor_t *motor, mhi_step_mode_t mode);
int  mhi_stepper_motor_move_to(mhi_stepper_motor_t *motor, int32_t position);
int  mhi_stepper_motor_move_steps(mhi_stepper_motor_t *motor, int32_t steps);
int  mhi_stepper_motor_set_speed(mhi_stepper_motor_t *motor, float steps_per_s);
void mhi_stepper_motor_emergency_stop(mhi_stepper_motor_t *motor);
void mhi_stepper_motor_tick(mhi_stepper_motor_t *motor, uint32_t now_us);

/* ─── Servo Motor ───────────────────────────────────────────────────── */

typedef struct {
    uint8_t  pwm_channel;
    uint16_t pulse_min_us;
    uint16_t pulse_max_us;
    float    angle_min_deg;
    float    angle_max_deg;
    float    current_angle;
    bool     enabled;
} mhi_servo_motor_t;

int  mhi_servo_motor_init(mhi_servo_motor_t *servo, uint8_t pwm_ch);
int  mhi_servo_motor_set_angle(mhi_servo_motor_t *servo, float angle_deg);
float mhi_servo_motor_get_angle(const mhi_servo_motor_t *servo);
int  mhi_servo_motor_calibrate(mhi_servo_motor_t *servo,
                               uint16_t min_us, uint16_t max_us,
                               float min_deg, float max_deg);
void mhi_servo_motor_disable(mhi_servo_motor_t *servo);

/* ─── Global Emergency Stop ─────────────────────────────────────────── */

void mhi_emergency_stop_all(void);
void mhi_emergency_resume_all(void);

#ifdef __cplusplus
}
#endif

#endif /* MHI_ACTUATOR_DRIVER_H */
