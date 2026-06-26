#include "gpio_pwm_adc.h"
#include "sensor_polling.h"
#include "pid_controller.h"
#include "actuator_driver.h"
#include "signal_cond.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#define WHEEL_RADIUS_M      0.05f
#define WHEEL_BASE_M        0.20f
#define ENCODER_CPR         48u
#define GEAR_RATIO          30.0f
#define TICKS_PER_REV       (ENCODER_CPR * 2u)
#define MAX_SPEED_MPS       1.0f

/* ── Robot state ── */
static float s_robot_x    = 0.0f;
static float s_robot_y    = 0.0f;
static float s_robot_theta = 0.0f;

static float s_left_speed   = 0.0f;
static float s_right_speed  = 0.0f;
static int32_t s_left_encoder  = 0;
static int32_t s_right_encoder = 0;

static float s_distance_cm[3]   = { 150.0f, 150.0f, 150.0f };
static float s_battery_voltage  = 12.0f;
static float s_left_current_a   = 0.1f;
static float s_right_current_a  = 0.1f;

/* ── Navigation state machine ── */
typedef enum {
    NAV_IDLE = 0,
    NAV_FORWARD,
    NAV_TURN_LEFT,
    NAV_TURN_RIGHT,
    NAV_AVOID,
    NAV_STOP,
    NAV_ESTOP
} nav_state_t;

static nav_state_t s_nav_state = NAV_IDLE;
static float s_target_distance = 0.0f;
static float s_traveled_dist   = 0.0f;

/* ── Sensors ── */
static float s_dist_buf[5]   = {0.0f};
static float s_dist_sort[5]  = {0.0f};
static mhi_median_filter_t s_dist_mf;
static mhi_kalman_1d_t s_batt_kf;

/* ── PID controllers ── */
static mhi_pid_t s_left_speed_pid;
static mhi_pid_t s_right_speed_pid;

/* ── Motors ── */
static mhi_dc_motor_t s_motor_left;
static mhi_dc_motor_t s_motor_right;

/* ── Sensors ── */
static mhi_sensor_t s_sensors[3];
static mhi_sensor_manager_t s_sensor_mgr;
static float s_dist_raw[1]  = {0.0f};
static float s_dist_filt[1] = {0.0f};
static float s_batt_raw[1]  = {0.0f};
static float s_batt_filt[1] = {0.0f};
static float s_temp_raw[1]  = {0.0f};
static float s_temp_filt[1] = {0.0f};

static void read_distance(void *ctx, float *values, uint8_t *count)
{
    (void)ctx;
    s_distance_cm[1] += (float)((rand() % 20) - 10) * 0.5f;
    if (s_distance_cm[1] < 2.0f)  s_distance_cm[1] = 2.0f;
    if (s_distance_cm[1] > 400.0f) s_distance_cm[1] = 400.0f;
    values[0] = s_distance_cm[1];
    *count = 1u;
}

static void read_battery(void *ctx, float *values, uint8_t *count)
{
    (void)ctx;
    s_battery_voltage += (float)((rand() % 20) - 10) * 0.01f;
    if (s_battery_voltage < 10.0f) s_battery_voltage = 10.0f;
    if (s_battery_voltage > 13.0f) s_battery_voltage = 13.0f;
    values[0] = s_battery_voltage;
    *count = 1u;
}

static void read_motor_temp(void *ctx, float *values, uint8_t *count)
{
    (void)ctx;
    float temp = 25.0f +
        (fabsf(s_left_speed) + fabsf(s_right_speed)) * 15.0f +
        (float)((rand() % 30) - 15) * 0.1f;
    if (temp > 80.0f) temp = 80.0f;
    values[0] = temp;
    *count = 1u;
}

/* ── Odometry update ── */
static void update_odometry(float dt_s)
{
    float linear  = (s_left_speed + s_right_speed) * 0.5f;
    float angular = (s_right_speed - s_left_speed) / WHEEL_BASE_M;

    s_robot_x     += linear * cosf(s_robot_theta) * dt_s;
    s_robot_y     += linear * sinf(s_robot_theta) * dt_s;
    s_robot_theta += angular * dt_s;

    while (s_robot_theta >  (float)M_PI) s_robot_theta -= 2.0f * (float)M_PI;
    while (s_robot_theta < -(float)M_PI) s_robot_theta += 2.0f * (float)M_PI;

    s_traveled_dist += fabsf(linear) * dt_s;

    float wheel_circ = 2.0f * (float)M_PI * WHEEL_RADIUS_M;
    s_left_encoder  += (int32_t)(s_left_speed  * dt_s / wheel_circ
                                 * TICKS_PER_REV);
    s_right_encoder += (int32_t)(s_right_speed * dt_s / wheel_circ
                                 * TICKS_PER_REV);
}

/* ── Navigate ── */
static void do_navigate(float dt_s)
{
    float setpoint_l = 0.0f;
    float setpoint_r = 0.0f;

    float front_dist = s_distance_cm[1];

    switch (s_nav_state) {
    case NAV_ESTOP:
    case NAV_STOP:
        setpoint_l = 0.0f;
        setpoint_r = 0.0f;
        break;

    case NAV_FORWARD:
        if (front_dist < 20.0f) {
            s_nav_state = NAV_AVOID;
            s_target_distance = 0.3f + (float)(rand() % 20) * 0.01f;
            s_traveled_dist = 0.0f;
        } else {
            setpoint_l = MAX_SPEED_MPS * 0.5f;
            setpoint_r = MAX_SPEED_MPS * 0.5f;
        }
        break;

    case NAV_TURN_LEFT:
        if (s_traveled_dist >= s_target_distance) {
            s_nav_state = NAV_FORWARD;
        } else {
            setpoint_l = -MAX_SPEED_MPS * 0.3f;
            setpoint_r =  MAX_SPEED_MPS * 0.3f;
        }
        break;

    case NAV_AVOID:
        if (s_traveled_dist >= s_target_distance) {
            if (front_dist > 30.0f)
                s_nav_state = NAV_FORWARD;
            else
                s_nav_state = NAV_TURN_LEFT;
        } else {
            setpoint_l = -MAX_SPEED_MPS * 0.4f;
            setpoint_r = -MAX_SPEED_MPS * 0.4f;
        }
        break;

    default:
        break;
    }

    float meas_l = s_left_speed;
    float meas_r = s_right_speed;

    float cmd_l = mhi_pid_compute(&s_left_speed_pid,
                                  setpoint_l, meas_l, (uint32_t)(dt_s * 1000.0f));
    float cmd_r = mhi_pid_compute(&s_right_speed_pid,
                                  setpoint_r, meas_r, (uint32_t)(dt_s * 1000.0f));

    cmd_l = fmaxf(-1.0f, fminf(1.0f, cmd_l));
    cmd_r = fmaxf(-1.0f, fminf(1.0f, cmd_r));

    mhi_dc_motor_set_speed(&s_motor_left,  cmd_l);
    mhi_dc_motor_set_speed(&s_motor_right, cmd_r);

    s_left_speed  += 0.1f * (cmd_l * MAX_SPEED_MPS - s_left_speed);
    s_right_speed += 0.1f * (cmd_r * MAX_SPEED_MPS - s_right_speed);
}

/* ── Serial command parser ── */
static void parse_command(const char *cmd)
{
    float val = 0.0f;
    if (sscanf(cmd, "forward %f", &val) == 1) {
        s_nav_state = NAV_FORWARD;
        s_target_distance = val;
        s_traveled_dist = 0.0f;
        printf("[CMD] Forward %.2f m\n", (double)val);
    } else if (sscanf(cmd, "turn %f", &val) == 1) {
        if (val > 0.0f)
            s_nav_state = NAV_TURN_LEFT;
        else
            s_nav_state = NAV_TURN_RIGHT;
        s_target_distance = fabsf(val) * (float)M_PI * WHEEL_BASE_M / 4.0f;
        s_traveled_dist = 0.0f;
        printf("[CMD] Turn %.1f deg\n", (double)val);
    } else if (strcmp(cmd, "stop") == 0) {
        s_nav_state = NAV_STOP;
        printf("[CMD] Stop\n");
    } else if (strcmp(cmd, "estop") == 0) {
        mhi_emergency_stop_all();
        s_nav_state = NAV_ESTOP;
        printf("[CMD] EMERGENCY STOP\n");
    } else if (strcmp(cmd, "resume") == 0) {
        mhi_emergency_resume_all();
        s_nav_state = NAV_IDLE;
        printf("[CMD] Resume\n");
    }
}

/* ── Display telemetry ── */
static void print_telemetry(uint32_t t_ms)
{
    float batt_f = mhi_kalman_1d_update(&s_batt_kf, s_battery_voltage);

    printf("\n╔══ T=%.1fs ═══════════════════════════════════╗\n",
           t_ms * 0.001);
    printf("║ POS    x=% 6.2f  y=% 6.2f  θ=% 6.1f°      ║\n",
           (double)s_robot_x, (double)s_robot_y,
           (double)(s_robot_theta * 180.0f / M_PI));
    printf("║ ENC    L=%-6d    R=%-6d                 ║\n",
           s_left_encoder, s_right_encoder);
    printf("║ MOTOR  L=% 5.2f m/s  R=% 5.2f m/s          ║\n",
           (double)s_left_speed, (double)s_right_speed);
    printf("║ PID    L=% 5.3f  R=% 5.3f                     ║\n",
           (double)s_left_speed_pid.output,
           (double)s_right_speed_pid.output);
    printf("║ DIST   F=% 5.1f cm                            ║\n",
           (double)s_distance_cm[1]);
    printf("║ NAV    state=%d  target=%.2f  traveled=%.2f ║\n",
           s_nav_state, (double)s_target_distance,
           (double)s_traveled_dist);
    printf("║ BATT   %.2f V (est %.2f V)                  ║\n",
           (double)s_battery_voltage, (double)batt_f);
    printf("║ TEMP   motor=%.1f C                            ║\n",
           (double)s_temp_raw[0]);
    printf("╚═══════════════════════════════════════════════╝\n");
}

int main(void)
{
    printf("=== Demo: Differential Drive Robot Control System ===\n\n");

    /* ── Init GPIO/PWM for motors ── */
    printf("[INIT] GPIO and PWM for motors\n");
    mhi_gpio_init(4u, MHI_GPIO_MODE_OUTPUT);
    mhi_gpio_init(5u, MHI_GPIO_MODE_OUTPUT);
    mhi_pwm_init(0u, 20000.0f, MHI_PWM_RES_8BIT);
    mhi_gpio_init(6u, MHI_GPIO_MODE_OUTPUT);
    mhi_gpio_init(7u, MHI_GPIO_MODE_OUTPUT);
    mhi_pwm_init(1u, 20000.0f, MHI_PWM_RES_8BIT);

    /* ── Init DC motors ── */
    printf("[INIT] DC motors (H-bridge)\n");
    mhi_dc_motor_init(&s_motor_left,  0u, 4u, 5u);
    mhi_dc_motor_init(&s_motor_right, 1u, 6u, 7u);
    s_motor_left.soft_start_ramp_s  = 0.2f;
    s_motor_right.soft_start_ramp_s = 0.2f;

    /* ── Init PID controllers ── */
    printf("[INIT] PID velocity controllers\n");
    mhi_pid_config_t speed_cfg = {
        .kp = 3.0f, .ki = 1.5f, .kd = 0.1f,
        .setpoint = 0.0f,
        .sample_time_s = 0.02f,
        .output_min = -1.0f, .output_max = 1.0f,
        .form = MHI_PID_FORM_POSITIONAL,
        .anti_windup = MHI_PID_ANTI_WINDUP_BOTH,
        .direction = MHI_PID_DIRECT,
        .back_calc_gain = 0.3f,
        .derivative_filter_a = 0.15f
    };
    mhi_pid_init(&s_left_speed_pid,  &speed_cfg);
    mhi_pid_init(&s_right_speed_pid, &speed_cfg);

    /* ── Init sensors ── */
    printf("[INIT] Sensor array (distance, battery, temp)\n");
    s_sensors[0].id = 0u;
    s_sensors[0].type = MHI_SENSOR_DISTANCE;
    s_sensors[0].name = "VL53L0X-Front";
    s_sensors[0].read = read_distance;
    s_sensors[0].raw_values = s_dist_raw;
    s_sensors[0].filtered_values = s_dist_filt;
    s_sensors[0].value_count = 1u;
    s_sensors[0].poll_interval_ms = 50u;

    s_sensors[1].id = 1u;
    s_sensors[1].type = MHI_SENSOR_CUSTOM;
    s_sensors[1].name = "Battery-ADC";
    s_sensors[1].read = read_battery;
    s_sensors[1].raw_values = s_batt_raw;
    s_sensors[1].filtered_values = s_batt_filt;
    s_sensors[1].value_count = 1u;
    s_sensors[1].poll_interval_ms = 200u;

    s_sensors[2].id = 2u;
    s_sensors[2].type = MHI_SENSOR_TEMP;
    s_sensors[2].name = "Motor-Thermistor";
    s_sensors[2].read = read_motor_temp;
    s_sensors[2].raw_values = s_temp_raw;
    s_sensors[2].filtered_values = s_temp_filt;
    s_sensors[2].value_count = 1u;
    s_sensors[2].poll_interval_ms = 500u;

    mhi_sensor_manager_init(&s_sensor_mgr, s_sensors, 3u);

    /* ── Init signal conditioning ── */
    printf("[INIT] Signal conditioning\n");
    mhi_median_init(&s_dist_mf, s_dist_buf, s_dist_sort, 5u);

    mhi_kalman_1d_init(&s_batt_kf, 12.0f, 0.001f, 0.05f);

    /* ── ADC for battery ── */
    mhi_adc_init(0u, MHI_ADC_RES_12BIT, 3.3f);

    /* ── Simulation loop ── */
    printf("\n[RUN] Starting 10-second simulation...\n");
    printf("[CMD] Available: forward <m>, turn <deg>, stop, estop, resume\n\n");

    parse_command("forward 3.0");

    uint32_t ms;
    for (ms = 0u; ms <= 10000u; ms += 100u) {
        float dt_s = 0.1f;

        mhi_sensor_manager_service(&s_sensor_mgr, ms);

        s_distance_cm[1] = mhi_median_update(&s_dist_mf, s_dist_raw[0]);

        if (ms > 3000u && ms < 3100u) {
            s_distance_cm[1] = 5.0f;
            printf("[SIM] Obstacle detected! distance=%.0f cm\n",
                   s_distance_cm[1]);
        }

        if (ms == 7000u) {
            parse_command("turn 90");
        }

        if (ms == 9000u) {
            parse_command("stop");
        }

        do_navigate(dt_s);
        update_odometry(dt_s);

        float batt_raw = 0u;
        float batt_volt = 0.0f;
        mhi_adc_read_single(0u, (uint32_t *)&batt_raw, &batt_volt);

        if (ms % 2000u == 0u) {
            print_telemetry(ms);
        }
    }

    printf("[RUN] Simulation complete\n");

    /* ── Shutdown ── */
    printf("\n[SHUTDOWN] Stopping motors\n");
    mhi_emergency_stop_all();
    mhi_dc_motor_deinit(&s_motor_left);
    mhi_dc_motor_deinit(&s_motor_right);
    mhi_pwm_deinit(0u);
    mhi_pwm_deinit(1u);
    mhi_adc_deinit(0u);

    printf("\n=== Demo: Robot Control complete ===\n");
    return 0;
}
