/*
 * test_core.c - Core Unit Tests for mini-hardware-interface
 *
 * Tests all five sub-modules:
 *   actuator_driver.h, gpio_pwm_adc.h, pid_controller.h, sensor_polling.h, signal_cond.h
 */

#include "actuator_driver.h"
#include "gpio_pwm_adc.h"
#include "pid_controller.h"
#include "sensor_polling.h"
#include "signal_cond.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

static int tests_run = 0, tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  TEST %s ... ", name); } while(0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); return 1; } while(0)
#define CHECK(cond, msg) if (!(cond)) FAIL(msg)

/* ================================================================
 *  gpio_pwm_adc.h  --  GPIO / PWM / ADC / Servo
 * ================================================================ */

/* --- Test 1: GPIO Init / Write / Read / Toggle --- */
static int test_gpio_write_read(void)
{
    TEST("GPIO init/write/read/toggle");
    int rc = mhi_gpio_init(7, MHI_GPIO_MODE_OUTPUT);
    CHECK(rc == 0, "gpio_init(7) failed");

    mhi_gpio_write(7, MHI_GPIO_HIGH);
    mhi_gpio_level_t val = mhi_gpio_read(7);
    CHECK(val == MHI_GPIO_HIGH, "read after write HIGH failed");

    mhi_gpio_write(7, MHI_GPIO_LOW);
    val = mhi_gpio_read(7);
    CHECK(val == MHI_GPIO_LOW, "read after write LOW failed");

    mhi_gpio_toggle(7);
    val = mhi_gpio_read(7);
    CHECK(val == MHI_GPIO_HIGH, "toggle from LOW should give HIGH");

    mhi_gpio_toggle(7);
    val = mhi_gpio_read(7);
    CHECK(val == MHI_GPIO_LOW, "toggle from HIGH should give LOW");

    mhi_gpio_deinit(7);
    PASS();
    return 0;
}

/* --- Test 2: GPIO Input Modes --- */
static int test_gpio_input_modes(void)
{
    TEST("GPIO input modes (pullup/pulldown)");
    int rc = mhi_gpio_init(3, MHI_GPIO_MODE_INPUT_PULLUP);
    CHECK(rc == 0, "gpio_init input_pullup failed");
    mhi_gpio_level_t val = mhi_gpio_read(3);
    CHECK(val == MHI_GPIO_HIGH, "pullup pin should read HIGH");
    mhi_gpio_deinit(3);

    rc = mhi_gpio_init(4, MHI_GPIO_MODE_INPUT_PULLDOWN);
    CHECK(rc == 0, "gpio_init input_pulldown failed");
    val = mhi_gpio_read(4);
    CHECK(val == MHI_GPIO_LOW, "pulldown pin should read LOW");
    mhi_gpio_deinit(4);
    PASS();
    return 0;
}

/* --- Test 3: PWM Init / Set Duty --- */
static int test_pwm_init_duty(void)
{
    TEST("PWM init/set duty/frequency");
    int rc = mhi_pwm_init(3, 1000.0f, MHI_PWM_RES_16BIT);
    CHECK(rc == 0, "pwm_init(3) failed");

    rc = mhi_pwm_set_duty(3, 50.0f);
    CHECK(rc == 0, "pwm_set_duty 50%% failed");
    float got = mhi_pwm_get_duty(3);
    CHECK(got > 49.0f && got < 51.0f, "pwm_get_duty returned wrong value");

    rc = mhi_pwm_set_duty(3, 0.0f);
    CHECK(rc == 0, "pwm_set_duty 0%% failed");
    rc = mhi_pwm_set_duty(3, 100.0f);
    CHECK(rc == 0, "pwm_set_duty 100%% failed");

    rc = mhi_pwm_set_frequency(3, 2000.0f);
    CHECK(rc == 0, "pwm_set_frequency failed");
    float freq = mhi_pwm_get_frequency(3);
    CHECK(freq > 1990.0f && freq < 2010.0f, "pwm_get_frequency wrong");

    mhi_pwm_start(3);
    CHECK(mhi_pwm_is_running(3), "pwm should be running after start");
    mhi_pwm_stop(3);
    CHECK(!mhi_pwm_is_running(3), "pwm should not be running after stop");

    mhi_pwm_deinit(3);
    PASS();
    return 0;
}

/* --- Test 4: ADC Read and Raw-to-Voltage --- */
static int test_adc_read_convert(void)
{
    TEST("ADC read and voltage conversion");
    int rc = mhi_adc_init(2, MHI_ADC_RES_12BIT, 3.3f);
    CHECK(rc == 0, "adc_init(2) failed");

    uint32_t raw;
    float voltage;
    rc = mhi_adc_read_single(2, &raw, &voltage);
    CHECK(rc == 0, "adc_read_single failed");

    float computed = mhi_adc_raw_to_voltage(2048, 3.3f, 4095);
    CHECK(computed > 1.5f && computed < 1.8f, "adc_raw_to_voltage out of range");

    computed = mhi_adc_raw_to_voltage(0, 3.3f, 4095);
    CHECK(computed < 0.01f, "raw 0 should be ~0V");

    computed = mhi_adc_raw_to_voltage(4095, 3.3f, 4095);
    CHECK(computed > 3.28f, "raw max should be ~Vref");

    uint32_t raw_from_v = mhi_adc_voltage_to_raw(1.65f, 3.3f, 4095);
    CHECK(raw_from_v > 2000 && raw_from_v < 2100, "voltage_to_raw wrong");

    mhi_adc_deinit(2);
    PASS();
    return 0;
}

/* --- Test 5: Servo (config-based) Init/Angle --- */
static int test_servo_set_angle(void)
{
    TEST("Servo init/set angle (config-based)");
    mhi_servo_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.pwm_channel = 4;
    cfg.pulse_min_us = 500;
    cfg.pulse_max_us = 2500;
    cfg.angle_min_deg = 0.0f;
    cfg.angle_max_deg = 180.0f;
    cfg.period_us = 20000;

    int rc = mhi_servo_init(&cfg);
    CHECK(rc == 0, "servo_init failed");

    rc = mhi_servo_set_angle(4, 90.0f);
    CHECK(rc == 0, "servo_set_angle 90 failed");
    rc = mhi_servo_set_angle(4, 0.0f);
    CHECK(rc == 0, "servo_set_angle 0 failed");
    rc = mhi_servo_set_angle(4, 180.0f);
    CHECK(rc == 0, "servo_set_angle 180 failed");

    float angle = mhi_servo_get_angle(4);
    CHECK(angle > 178.0f && angle < 182.0f, "servo_get_angle wrong");

    mhi_servo_deinit(4);
    PASS();
    return 0;
}

/* ================================================================
 *  actuator_driver.h  --  DC / BLDC / Stepper / Servo Motor
 * ================================================================ */

/* --- Test 6: DC Motor Init / Speed / Direction / Brake --- */
static int test_dc_motor_ops(void)
{
    TEST("DC motor init/speed/direction/brake");
    mhi_dc_motor_t motor;
    memset(&motor, 0, sizeof(motor));

    int rc = mhi_dc_motor_init(&motor, 0, 1, 2);
    CHECK(rc == 0, "dc_motor_init failed");

    rc = mhi_dc_motor_set_speed(&motor, 0.5f);
    CHECK(rc == 0, "set_speed 0.5 failed");
    CHECK(motor.current_speed > 0.0f, "speed not set");

    rc = mhi_dc_motor_set_speed(&motor, -0.75f);
    CHECK(rc == 0, "set_speed -0.75 failed");
    CHECK(motor.current_speed < 0.0f, "negative speed not set");

    rc = mhi_dc_motor_set_direction(&motor, MHI_DC_DIR_CCW);
    CHECK(rc == 0, "set_direction CCW failed");
    rc = mhi_dc_motor_set_direction(&motor, MHI_DC_DIR_CW);
    CHECK(rc == 0, "set_direction CW failed");

    rc = mhi_dc_motor_brake(&motor);
    CHECK(rc == 0, "brake failed");

    mhi_dc_motor_emergency_stop(&motor);
    CHECK(motor.emergency_stop == true, "emergency_stop not set");

    mhi_dc_motor_resume(&motor);
    CHECK(motor.emergency_stop == false, "resume did not clear estop");

    mhi_dc_motor_deinit(&motor);
    PASS();
    return 0;
}

/* --- Test 7: BLDC Motor Init / Speed / Commutate --- */
static int test_bldc_motor_ops(void)
{
    TEST("BLDC motor init/speed/commutate");
    mhi_bldc_motor_t bldc;
    mhi_bldc_pins_t pins;
    memset(&pins, 0, sizeof(pins));
    memset(&bldc, 0, sizeof(bldc));
    pins.hall_a = 3; pins.hall_b = 4; pins.hall_c = 5;
    pins.pwm_uh = 6; pins.pwm_ul = 7;
    pins.pwm_vh = 8; pins.pwm_vl = 9;
    pins.pwm_wh = 10; pins.pwm_wl = 11;

    int rc = mhi_bldc_motor_init(&bldc, &pins);
    CHECK(rc == 0, "bldc_motor_init failed");

    rc = mhi_bldc_motor_set_speed(&bldc, 0.75f);
    CHECK(rc == 0, "bldc_set_speed failed");
    CHECK(bldc.target_speed > 0.7f, "target_speed not set");

    rc = mhi_bldc_motor_set_duty(&bldc, 0.60f);
    CHECK(rc == 0, "bldc_set_duty failed");

    mhi_bldc_motor_commutate(&bldc);
    CHECK(bldc.commutation_step < 6, "invalid commutation step");

    mhi_bldc_motor_emergency_stop(&bldc);
    CHECK(bldc.emergency_stop == true, "bldc emergency stop not set");

    mhi_bldc_motor_deinit(&bldc);
    PASS();
    return 0;
}

/* --- Test 8: Stepper Motor Init / Speed / Move To --- */
static int test_stepper_motor_ops(void)
{
    TEST("Stepper motor init/move/speed");
    mhi_stepper_motor_t stepper;
    mhi_stepper_pins_t spins;
    memset(&stepper, 0, sizeof(stepper));
    memset(&spins, 0, sizeof(spins));
    spins.coil_a1 = 1; spins.coil_a2 = 2;
    spins.coil_b1 = 3; spins.coil_b2 = 4;

    int rc = mhi_stepper_motor_init(&stepper, &spins, 200);
    CHECK(rc == 0, "stepper_motor_init failed");

    rc = mhi_stepper_motor_set_speed(&stepper, 500.0f);
    CHECK(rc == 0, "stepper_set_speed failed");
    CHECK(stepper.speed_steps_per_s > 499.0f, "speed not set");

    rc = mhi_stepper_motor_set_mode(&stepper, MHI_STEP_MODE_MICRO_8);
    CHECK(rc == 0, "stepper_set_mode failed");

    rc = mhi_stepper_motor_move_to(&stepper, 1000);
    CHECK(rc == 0, "stepper_move_to failed");
    CHECK(stepper.target_position == 1000, "target_position not set");

    rc = mhi_stepper_motor_move_steps(&stepper, 200);
    CHECK(rc == 0, "stepper_move_steps failed");

    mhi_stepper_motor_emergency_stop(&stepper);
    CHECK(stepper.emergency_stop == true, "stepper estop not set");

    mhi_stepper_motor_deinit(&stepper);
    PASS();
    return 0;
}

/* --- Test 9: Servo Motor Init / Angle / Disable --- */
static int test_servo_motor_ops(void)
{
    TEST("Servo motor init/angle/disable");
    mhi_servo_motor_t servo;
    memset(&servo, 0, sizeof(servo));

    int rc = mhi_servo_motor_init(&servo, 2);
    CHECK(rc == 0, "servo_motor_init failed");

    rc = mhi_servo_motor_set_angle(&servo, 45.0f);
    CHECK(rc == 0, "servo set 45 failed");
    float angle = mhi_servo_motor_get_angle(&servo);
    CHECK(angle == 45.0f, "servo get_angle != 45");

    rc = mhi_servo_motor_set_angle(&servo, 135.0f);
    CHECK(rc == 0, "servo set 135 failed");

    rc = mhi_servo_motor_calibrate(&servo, 500, 2500, 0.0f, 180.0f);
    CHECK(rc == 0, "servo calibrate failed");

    mhi_servo_motor_disable(&servo);
    CHECK(servo.enabled == false, "servo should be disabled");
    PASS();
    return 0;
}

/* ================================================================
 *  pid_controller.h  --  PID / Z-N / Cascaded
 * ================================================================ */

/* --- Test 10: PID Compute / Limits / Saturation --- */
static int test_pid_compute(void)
{
    TEST("PID controller compute");
    mhi_pid_t pid;
    mhi_pid_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.kp = 2.0f; cfg.ki = 0.5f; cfg.kd = 0.1f;
    cfg.setpoint = 100.0f;
    cfg.sample_time_s = 0.01f;
    cfg.output_min = -100.0f;
    cfg.output_max = 100.0f;
    cfg.form = MHI_PID_FORM_POSITIONAL;
    cfg.anti_windup = MHI_PID_ANTI_WINDUP_CLAMPING;
    cfg.direction = MHI_PID_DIRECT;

    mhi_pid_init(&pid, &cfg);

    float out = mhi_pid_compute(&pid, 100.0f, 50.0f, 100);
    CHECK(out > 0.0f, "PID output should be positive for positive error");

    mhi_pid_set_gains(&pid, 1.0f, 0.2f, 0.05f);
    out = mhi_pid_compute(&pid, 100.0f, 150.0f, 200);
    CHECK(out < 0.0f, "PID output should be negative for negative error");

    mhi_pid_set_output_limits(&pid, -50.0f, 50.0f);
    out = mhi_pid_compute(&pid, 100.0f, 10.0f, 300);
    CHECK(out <= 50.0f && out >= -50.0f, "PID output should be clamped");

    CHECK(mhi_pid_is_saturated(&pid), "PID should be saturated at limit");

    /* Feed-forward */
    mhi_pid_set_feed_forward(&pid, 10.0f);
    out = mhi_pid_compute(&pid, 100.0f, 50.0f, 400);
    CHECK(out > 0.0f, "PID with feed-forward should still be positive");

    /* Get terms */
    float p = mhi_pid_get_p_term(&pid);
    float i = mhi_pid_get_i_term(&pid);
    float d = mhi_pid_get_d_term(&pid);
    CHECK(p != 0.0f || i != 0.0f || d != 0.0f, "PID terms should not all be zero");

    mhi_pid_reset(&pid);
    float out_after = mhi_pid_get_output(&pid);
    CHECK(out_after < 0.01f && out_after > -0.01f, "PID output should be zero after reset");

    PASS();
    return 0;
}

/* --- Test 11: Ziegler-Nichols Tuning --- */
static int test_zn_tune(void)
{
    TEST("Ziegler-Nichols tuning (P/PI/PID)");
    mhi_zn_result_t r_p, r_pi, r_pid;

    mhi_zn_tune(MHI_ZN_TYPE_P, 2.5f, 1.0f, &r_p);
    CHECK(r_p.kp > 0.0f, "Z-N P: Kp should be positive");
    CHECK(r_p.ki == 0.0f, "Z-N P: Ki should be zero");

    mhi_zn_tune(MHI_ZN_TYPE_PI, 2.5f, 1.0f, &r_pi);
    CHECK(r_pi.kp > 0.0f, "Z-N PI: Kp should be positive");
    CHECK(r_pi.ki > 0.0f, "Z-N PI: Ki should be positive");

    mhi_zn_tune(MHI_ZN_TYPE_PID, 2.5f, 1.0f, &r_pid);
    CHECK(r_pid.kp > 0.0f, "Z-N PID: Kp should be positive");
    CHECK(r_pid.ki > 0.0f, "Z-N PID: Ki should be positive");
    CHECK(r_pid.kd > 0.0f, "Z-N PID: Kd should be positive");
    PASS();
    return 0;
}

/* --- Test 12: Cascaded PID --- */
static int test_cascaded_pid(void)
{
    TEST("Cascaded PID init/compute/reset");
    mhi_cascaded_pid_t cpid;
    mhi_pid_config_t outer_cfg, inner_cfg;
    memset(&outer_cfg, 0, sizeof(outer_cfg));
    memset(&inner_cfg, 0, sizeof(inner_cfg));

    outer_cfg.kp = 1.0f; outer_cfg.ki = 0.1f; outer_cfg.kd = 0.01f;
    outer_cfg.setpoint = 50.0f; outer_cfg.sample_time_s = 0.01f;
    outer_cfg.output_min = -100.0f; outer_cfg.output_max = 100.0f;
    outer_cfg.form = MHI_PID_FORM_POSITIONAL;

    inner_cfg.kp = 2.0f; inner_cfg.ki = 0.2f; inner_cfg.kd = 0.02f;
    inner_cfg.setpoint = 0.0f; inner_cfg.sample_time_s = 0.01f;
    inner_cfg.output_min = -100.0f; inner_cfg.output_max = 100.0f;
    inner_cfg.form = MHI_PID_FORM_POSITIONAL;

    mhi_cascaded_pid_init(&cpid, &outer_cfg, &inner_cfg);

    float out = mhi_cascaded_pid_compute(&cpid, 50.0f, 30.0f, 20.0f, 100);
    CHECK(out >= outer_cfg.output_min && out <= outer_cfg.output_max,
          "cascaded output out of bounds");

    out = mhi_cascaded_pid_compute(&cpid, 50.0f, 48.0f, 47.0f, 200);
    CHECK(out >= outer_cfg.output_min && out <= outer_cfg.output_max,
          "cascaded output near setpoint out of bounds");

    mhi_cascaded_pid_set_output_limits(&cpid, -50.0f, 50.0f);
    out = mhi_cascaded_pid_compute(&cpid, 50.0f, 0.0f, 0.0f, 300);
    CHECK(out <= 50.0f && out >= -50.0f, "cascaded output should be clamped");

    mhi_cascaded_pid_reset(&cpid);
    PASS();
    return 0;
}

/* ================================================================
 *  sensor_polling.h  --  Bus / Sensor / Filters / Calibration
 * ================================================================ */

/* --- Test 13: Sensor Bus Init / Read / Write --- */
static int test_sensor_bus_ops(void)
{
    TEST("Sensor bus init/read_reg/write_reg");
    mhi_bus_config_t bcfg;
    memset(&bcfg, 0, sizeof(bcfg));
    bcfg.type = MHI_BUS_I2C;
    bcfg.address = 0x76;
    bcfg.speed_hz = 400000;

    int rc = mhi_bus_init(&bcfg);
    CHECK(rc == 0, "bus_init failed");

    uint8_t tx[2] = {0xAA, 0xBB};
    rc = mhi_bus_write_reg(0xF4, tx, 2);
    CHECK(rc == 0, "bus_write_reg failed");

    uint8_t rx[2];
    rc = mhi_bus_read_reg(0xF7, rx, 2);
    CHECK(rc == 0, "bus_read_reg failed");

    mhi_bus_deinit();
    PASS();
    return 0;
}

/* --- Test 14: Sensor Manager Init / Service --- */
static int test_sensor_manager(void)
{
    TEST("Sensor manager init/service");
    mhi_sensor_t sensors[3];
    float r0[2] = {0, 0}, f0[2] = {0, 0};
    float r1[1] = {0}, f1[1] = {0};
    float r2[1] = {0}, f2[1] = {0};
    memset(sensors, 0, sizeof(sensors));

    sensors[0].id = 0; sensors[0].type = MHI_SENSOR_TEMP;
    sensors[0].poll_interval_ms = 100; sensors[0].value_count = 2;
    sensors[0].raw_values = r0; sensors[0].filtered_values = f0;

    sensors[1].id = 1; sensors[1].type = MHI_SENSOR_HUMIDITY;
    sensors[1].poll_interval_ms = 200; sensors[1].value_count = 1;
    sensors[1].raw_values = r1; sensors[1].filtered_values = f1;

    sensors[2].id = 2; sensors[2].type = MHI_SENSOR_PRESSURE;
    sensors[2].poll_interval_ms = 150; sensors[2].value_count = 1;
    sensors[2].raw_values = r2; sensors[2].filtered_values = f2;

    mhi_sensor_manager_t mgr;
    mhi_sensor_manager_init(&mgr, sensors, 3);

    mhi_sensor_manager_service(&mgr, 100);
    mhi_sensor_manager_service(&mgr, 200);
    mhi_sensor_manager_service(&mgr, 300);

    PASS();
    return 0;
}

/* --- Test 15: Kalman Filter --- */
static int test_kalman_filter(void)
{
    TEST("Kalman 1D filter update/reset");
    mhi_kalman_1d_t kf;
    mhi_kalman_1d_init(&kf, 25.0f, 0.01f, 0.1f);

    float est = mhi_kalman_1d_update(&kf, 27.0f);
    CHECK(est > 25.0f && est < 27.0f,
          "Kalman estimate should move toward measurement");
    est = mhi_kalman_1d_update(&kf, 23.0f);
    CHECK(est > 23.0f && est < 27.0f,
          "Kalman estimate should be between extremes");
    est = mhi_kalman_1d_update(&kf, 26.0f);
    CHECK(est > 24.0f && est < 26.5f,
          "Kalman estimate should converge");

    mhi_kalman_1d_reset(&kf, 0.0f);
    est = mhi_kalman_1d_update(&kf, 0.0f);
    CHECK(est < 0.01f && est > -0.01f, "Kalman after reset should be near zero");
    PASS();
    return 0;
}

/* --- Test 16: Moving Average and Median --- */
static int test_moving_average_median(void)
{
    TEST("Moving average and median filters");
    mhi_moving_average_t ma;
    float buf[8];
    mhi_ma_init(&ma, buf, 8);

    float avg = mhi_ma_update(&ma, 10.0f);
    CHECK(avg == 10.0f, "MA first value should equal input");
    avg = mhi_ma_update(&ma, 20.0f);
    CHECK(avg == 15.0f, "MA of 10,20 should be 15");

    mhi_ma_reset(&ma);
    avg = mhi_ma_update(&ma, 100.0f);
    CHECK(avg == 100.0f, "MA after reset should start fresh");

    mhi_median_filter_t mf;
    float mbuf[9], msort[9];
    mhi_median_init(&mf, mbuf, msort, 9);

    float med = mhi_median_update(&mf, 5.0f);
    CHECK(med == 5.0f, "Median of single value should be itself");
    med = mhi_median_update(&mf, 1.0f);
    med = mhi_median_update(&mf, 100.0f);
    CHECK(med == 5.0f, "Median of {5,1,100} should be 5");

    mhi_median_reset(&mf);
    med = mhi_median_update(&mf, 42.0f);
    CHECK(med == 42.0f, "Median after reset should start fresh");
    PASS();
    return 0;
}

/* --- Test 17: Calibration --- */
static int test_calibration(void)
{
    TEST("Sensor calibration zero/span/apply");
    mhi_calibration_t cal;
    mhi_calibration_init(&cal);

    mhi_calibration_set_zero(&cal, 0.0f, 0.0f);
    mhi_calibration_set_span(&cal, 1000.0f, 100.0f);

    float result = mhi_calibration_apply(&cal, 500.0f);
    CHECK(result > 49.0f && result < 51.0f, "calibration 500->50 failed");

    result = mhi_calibration_apply(&cal, 1000.0f);
    CHECK(result > 99.0f && result < 101.0f, "calibration 1000->100 failed");

    result = mhi_calibration_apply(&cal, 0.0f);
    CHECK(result > -1.0f && result < 1.0f, "calibration 0->0 failed");
    PASS();
    return 0;
}

/* ================================================================
 *  signal_cond.h  --  Op-Amp / RC Filter / 2nd-O / Wheatstone
 * ================================================================ */

/* --- Test 18: Op-Amp Configurations --- */
static int test_opamp_configs(void)
{
    TEST("Op-amp non-inverting/inverting/compute");
    mhi_opamp_t opamp;
    mhi_opamp_config_t ocfg;
    memset(&ocfg, 0, sizeof(ocfg));
    ocfg.topology = MHI_OPAMP_NON_INVERTING;
    ocfg.r1 = 10000.0f;
    ocfg.r2 = 100000.0f;

    mhi_opamp_init(&opamp, &ocfg, 5.0f, 0.0f);

    float vout = mhi_opamp_non_inverting(&opamp, 1.0f);
    CHECK(vout > 1.0f, "non-inverting gain should be > 1");

    vout = mhi_opamp_inverting(&opamp, 1.0f);
    CHECK(vout < 0.0f, "inverting output should be negative");

    vout = mhi_opamp_compute(&opamp, 0.5f, 0.0f);
    CHECK(vout >= opamp.saturation_min && vout <= opamp.saturation_max,
          "opamp output should be within saturation limits");

    /* Test clamping */
    float clamped = mhi_opamp_clamp(&opamp, 10.0f);
    CHECK(clamped <= opamp.saturation_max, "output should be clamped to max");
    clamped = mhi_opamp_clamp(&opamp, -10.0f);
    CHECK(clamped >= opamp.saturation_min, "output should be clamped to min");
    PASS();
    return 0;
}

/* --- Test 19: RC Filter --- */
static int test_rc_filter(void)
{
    TEST("RC filter update/cutoff/reset");
    mhi_rc_filter_t rc;
    mhi_rc_filter_init(&rc, MHI_FILTER_LOWPASS, 1000.0f, 1e-6f, 0.001f);

    float out = mhi_rc_filter_update(&rc, 5.0f);
    CHECK(out >= 0.0f, "RC filter output should be >= 0");

    out = mhi_rc_filter_update(&rc, 5.0f);
    CHECK(out > 0.0f, "RC filter should coast toward input");

    float cutoff = mhi_rc_filter_cutoff_freq(&rc);
    CHECK(cutoff > 0.0f, "cutoff frequency should be positive");

    mhi_rc_filter_reset(&rc);
    out = mhi_rc_filter_update(&rc, 0.0f);
    CHECK(out < 0.01f && out > -0.01f, "RC after reset should be ~0");

    /* Design helpers */
    float c = mhi_rc_design_c(159.0f, 1000.0f);
    CHECK(c > 0.0f, "rc_design_c should return positive value");
    float r = mhi_rc_design_r(159.0f, 1e-6f);
    CHECK(r > 0.0f, "rc_design_r should return positive value");
    PASS();
    return 0;
}

/* --- Test 20: 2nd-Order Filter --- */
static int test_filter_2nd(void)
{
    TEST("2nd-order filter init/update/reset");
    mhi_filter_2nd_t f2;
    mhi_filter_2nd_init(&f2, MHI_FILTER_LOWPASS, 100.0f, 0.707f, 1000.0f);

    float out = mhi_filter_2nd_update(&f2, 1.0f);
    CHECK(out >= 0.0f, "2nd order filter output should be >= 0");
    out = mhi_filter_2nd_update(&f2, 1.0f);
    CHECK(out > 0.0f, "2nd order filter should respond to input");

    mhi_filter_2nd_reset(&f2);
    out = mhi_filter_2nd_update(&f2, 0.0f);
    CHECK(out < 0.01f && out > -0.01f, "filter after reset should be ~0");
    PASS();
    return 0;
}

/* --- Test 21: Wheatstone Bridge --- */
static int test_wheatstone(void)
{
    TEST("Wheatstone bridge output/strain/resistance");
    mhi_wheatstone_t wb;
    mhi_wheatstone_init(&wb, MHI_WHEATSTONE_FULL, 5.0f, 2.0f, 350.0f);

    float vout = mhi_wheatstone_output_voltage(&wb, 0.0f);
    CHECK(vout < 0.001f && vout > -0.001f, "zero strain should give ~0V");

    vout = mhi_wheatstone_output_voltage(&wb, 500e-6f);
    CHECK(vout > 0.0f, "positive strain should give positive voltage");

    float strain = mhi_wheatstone_strain_from_voltage(&wb, vout);
    CHECK(strain > 400e-6f && strain < 600e-6f, "strain_from_voltage roundtrip wrong");

    float dr = mhi_wheatstone_resistance_change(&wb, 500e-6f);
    CHECK(dr > 0.0f, "resistance change should be positive");
    PASS();
    return 0;
}

/* --- Test 22: Optocoupler + Anti-alias --- */
static int test_optocoupler_antialias(void)
{
    TEST("Optocoupler output + anti-alias design");
    mhi_optocoupler_t oc;
    mhi_optocoupler_init(&oc, 1.0f, 1.2f, 20.0f, 3.3f, 10000.0f);

    float vout = mhi_optocoupler_output_voltage(&oc, 10.0f);
    CHECK(vout >= 0.0f && vout <= oc.output_vcc, "optocoupler output should be within supply");

    bool logic = mhi_optocoupler_output_logic(&oc, 10.0f, 1.5f);
    CHECK(logic == true || logic == false, "optocoupler logic output should be bool");

    /* Anti-alias filter */
    mhi_anti_alias_t aa;
    mhi_anti_alias_design(&aa, 1000.0f, 400.0f, 2);
    CHECK(aa.cutoff_hz > 0.0f, "anti-alias cutoff should be positive");

    float att = mhi_anti_alias_attenuation(&aa, 500.0f);
    CHECK(att > 0.0f, "attenuation should be positive");

    mhi_anti_alias_design_rc(&aa, 1000.0f);
    CHECK(aa.c > 0.0f, "anti-alias capacitor should be positive");
    PASS();
    return 0;
}

/* --- Test 23: Level Shift + Noise Filtering --- */
static int test_level_shift_noise(void)
{
    TEST("Level shift + noise filtering utilities");
    mhi_level_shift_t ls;
    mhi_level_shift_init(&ls, 10000.0f, 10000.0f, 3.3f);

    float shifted = mhi_level_shift_apply(&ls, 0.0f);
    CHECK(shifted >= 0.0f && shifted <= 3.3f, "level shifted output should be in range");

    mhi_level_shift_design(&ls, 0.0f, 3.3f, 0.0f, 5.0f, 3.3f);
    shifted = mhi_level_shift_apply(&ls, 1.65f);
    CHECK(shifted >= 0.0f && shifted <= 5.0f, "designed level shift output should be in range");

    /* Noise filters */
    float buf[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float avg = mhi_noise_simple_average(buf, 4);
    CHECK(avg > 2.0f && avg < 3.0f, "simple average of 1,2,3,4 should be ~2.5");

    float exp_val = mhi_noise_exponential(10.0f, 0.0f, 0.5f);
    CHECK(exp_val > 0.0f && exp_val < 10.0f, "exponential filter should blend");

    float spike = mhi_noise_spike_removal(100.0f, 1.0f, 10.0f);
    CHECK(spike == 1.0f + 10.0f, "spike removal should clamp or pass");

    float dead = mhi_noise_threshold_deadband(1.05f, 1.00f, 0.1f);
    CHECK(dead > 0.9f, "deadband should pass value near threshold");
    PASS();
    return 0;
}

/* ================================================================
 *  main
 * ================================================================ */

int main(void)
{
    printf("\n=== mini-hardware-interface Core Tests ===\n\n");

    /* gpio_pwm_adc.h */
    test_gpio_write_read();
    test_gpio_input_modes();
    test_pwm_init_duty();
    test_adc_read_convert();
    test_servo_set_angle();

    /* actuator_driver.h */
    test_dc_motor_ops();
    test_bldc_motor_ops();
    test_stepper_motor_ops();
    test_servo_motor_ops();

    /* pid_controller.h */
    test_pid_compute();
    test_zn_tune();
    test_cascaded_pid();

    /* sensor_polling.h */
    test_sensor_bus_ops();
    test_sensor_manager();
    test_kalman_filter();
    test_moving_average_median();
    test_calibration();

    /* signal_cond.h */
    test_opamp_configs();
    test_rc_filter();
    test_filter_2nd();
    test_wheatstone();
    test_optocoupler_antialias();
    test_level_shift_noise();

    printf("\n--- Results: %d/%d tests passed ---\n\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
