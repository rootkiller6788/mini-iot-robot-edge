#ifndef MCU_PERIPHERAL_H
#define MCU_PERIPHERAL_H

#include <stdbool.h>
#include <stdint.h>

/* ── Timer base addresses ────────────────────────────── */
#define TIM1_BASE             0x40010000UL
#define TIM2_BASE             0x40000000UL
#define TIM3_BASE             0x40000400UL
#define TIM4_BASE             0x40000800UL
#define TIM5_BASE             0x40000C00UL
#define TIM6_BASE             0x40001000UL
#define TIM7_BASE             0x40001400UL
#define TIM8_BASE             0x40010400UL
#define TIM9_BASE             0x40014000UL
#define TIM10_BASE            0x40014400UL
#define TIM11_BASE            0x40014800UL
#define TIM12_BASE            0x40001800UL
#define TIM13_BASE            0x40001C00UL
#define TIM14_BASE            0x40002000UL

#define TIM_CHANNEL_1         0x0U
#define TIM_CHANNEL_2         0x1U
#define TIM_CHANNEL_3         0x2U
#define TIM_CHANNEL_4         0x3U

/* ── PWM mode ────────────────────────────────────────── */
typedef enum {
    PWM_MODE_PHASE_CORRECT = 0,
    PWM_MODE_FAST          = 1,
    PWM_MODE_CENTER_ALIGNED1 = 2,
    PWM_MODE_CENTER_ALIGNED2 = 3,
    PWM_MODE_CENTER_ALIGNED3 = 4
} pwm_mode_t;

typedef enum {
    PWM_OUTPUT_NORMAL    = 0,
    PWM_OUTPUT_INVERTED  = 1
} pwm_output_t;

typedef enum {
    TIM_EDGE_RISING  = 0x0U,
    TIM_EDGE_FALLING = 0x1U,
    TIM_EDGE_BOTH    = 0x2U
} timer_edge_t;

typedef enum {
    TIM_ENCODER_MODE_TI1  = 0x1U,
    TIM_ENCODER_MODE_TI2  = 0x2U,
    TIM_ENCODER_MODE_BOTH = 0x3U
} encoder_mode_t;

typedef enum {
    TIM_COUNTER_UP       = 0x0U,
    TIM_COUNTER_DOWN     = 0x1U,
    TIM_COUNTER_CENTER1  = 0x2U,
    TIM_COUNTER_CENTER2  = 0x3U,
    TIM_COUNTER_CENTER3  = 0x4U
} counter_mode_t;

/* ── Timer configuration ─────────────────────────────── */
typedef struct {
    uint32_t tim_base;
    uint32_t prescaler;
    uint32_t period;
    uint32_t clock_division;
    counter_mode_t counter_mode;
    uint8_t repetition_counter;
    bool auto_reload_preload;
} timer_config_t;

typedef struct {
    uint32_t tim_base;
    uint8_t channel;
    pwm_mode_t mode;
    pwm_output_t output_type;
    uint32_t duty_cycle;
    uint32_t period;
    bool output_enable;
} pwm_config_t;

typedef struct {
    uint32_t tim_base;
    uint8_t input_channel;
    uint8_t output_channel;
    timer_edge_t input_edge;
    uint32_t prescaler;
    uint32_t period;
    bool dma_enable;
} capture_compare_config_t;

typedef struct {
    uint32_t tim_base;
    encoder_mode_t mode;
    uint16_t counts_per_rev;
    uint32_t prescaler;
} encoder_config_t;

/* ── Timer API ───────────────────────────────────────── */
void timer_init(const timer_config_t *config);
void timer_deinit(uint32_t tim_base);
void timer_start(uint32_t tim_base);
void timer_stop(uint32_t tim_base);
void timer_set_counter(uint32_t tim_base, uint32_t value);
uint32_t timer_get_counter(uint32_t tim_base);
void timer_set_period(uint32_t tim_base, uint32_t period);
void timer_set_prescaler(uint32_t tim_base, uint32_t prescaler);
void timer_generate_update(uint32_t tim_base);
bool timer_is_update_pending(uint32_t tim_base);
void timer_clear_update_flag(uint32_t tim_base);
void timer_enable_interrupt(uint32_t tim_base, uint16_t flags);
void timer_disable_interrupt(uint32_t tim_base, uint16_t flags);

/* ── PWM API ─────────────────────────────────────────── */
void pwm_init(const pwm_config_t *config);
void pwm_set_duty_cycle(uint32_t tim_base, uint8_t channel, uint32_t duty);
void pwm_set_period(uint32_t tim_base, uint32_t period);
void pwm_start(uint32_t tim_base, uint8_t channel);
void pwm_stop(uint32_t tim_base, uint8_t channel);
void pwm_enable_output(uint32_t tim_base, uint8_t channel);
void pwm_disable_output(uint32_t tim_base, uint8_t channel);
void pwm_set_polarity(uint32_t tim_base, uint8_t channel, pwm_output_t polarity);

/* ── Capture/Compare API ─────────────────────────────── */
void capture_compare_init(const capture_compare_config_t *config);
uint32_t capture_get_value(uint32_t tim_base, uint8_t channel);
void compare_set_value(uint32_t tim_base, uint8_t channel, uint32_t value);
void capture_compare_enable_dma(uint32_t tim_base, uint8_t channel);

/* ── Encoder API ─────────────────────────────────────── */
void encoder_init(const encoder_config_t *config);
int32_t encoder_get_count(uint32_t tim_base);
void encoder_reset_count(uint32_t tim_base);
uint32_t encoder_get_speed(uint32_t tim_base, uint32_t sample_period_ms);
bool encoder_get_direction(uint32_t tim_base);

/* ── RTC base address ────────────────────────────────── */
#define RTC_BASE              0x40002800UL

typedef enum {
    RTC_FORMAT_BIN   = 0x0U,
    RTC_FORMAT_BCD   = 0x1U
} rtc_format_t;

typedef enum {
    RTC_ALARM_A      = 0x0U,
    RTC_ALARM_B      = 0x1U
} rtc_alarm_t;

typedef enum {
    RTC_WAKEUP_CLOCK_DIV16    = 0x0U,
    RTC_WAKEUP_CLOCK_DIV8     = 0x1U,
    RTC_WAKEUP_CLOCK_DIV4     = 0x2U,
    RTC_WAKEUP_CLOCK_DIV2     = 0x3U,
    RTC_WAKEUP_CLOCK_CK_SPRE  = 0x4U
} rtc_wakeup_clock_t;

typedef struct {
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t weekday;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    rtc_format_t format;
} rtc_calendar_t;

typedef struct {
    rtc_alarm_t alarm;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    bool week_day_sel;
    uint8_t week_day;
    rtc_format_t format;
} rtc_alarm_config_t;

/* ── RTC API ─────────────────────────────────────────── */
void rtc_init(void);
void rtc_set_calendar(const rtc_calendar_t *calendar);
void rtc_get_calendar(rtc_calendar_t *calendar);
void rtc_set_alarm(const rtc_alarm_config_t *config);
void rtc_get_alarm(rtc_alarm_t alarm, rtc_alarm_config_t *config);
void rtc_enable_alarm(rtc_alarm_t alarm);
void rtc_disable_alarm(rtc_alarm_t alarm);
void rtc_clear_alarm_flag(rtc_alarm_t alarm);
void rtc_set_wakeup_timer(uint32_t period, rtc_wakeup_clock_t clock);
void rtc_enable_wakeup_timer(void);
void rtc_disable_wakeup_timer(void);
uint32_t rtc_get_timestamp(void);
void rtc_set_timestamp(uint32_t timestamp);

/* ── Watchdog base addresses ─────────────────────────── */
#define IWDG_BASE             0x40003000UL
#define WWDG_BASE             0x40002C00UL

#define IWDG_TIMEOUT_MAX      0xFFFUL
#define WWDG_TIMEOUT_MAX      0x7FUL

/* ── Watchdog API ────────────────────────────────────── */
void iwdg_init(uint32_t prescaler, uint32_t reload);
void iwdg_refresh(void);
void iwdg_enable(void);
bool iwdg_is_reset_flagged(void);
void iwdg_clear_reset_flag(void);

void wwdg_init(uint32_t prescaler, uint32_t window, uint32_t counter);
void wwdg_refresh(void);
void wwdg_enable_early_wakeup(void);

/* ── ADC base addresses ──────────────────────────────── */
#define ADC1_BASE             0x40012000UL
#define ADC2_BASE             0x40012100UL
#define ADC3_BASE             0x40012200UL

#define ADC_RESOLUTION_12BIT  0x0U
#define ADC_RESOLUTION_10BIT  0x1U
#define ADC_RESOLUTION_8BIT   0x2U
#define ADC_RESOLUTION_6BIT   0x3U

#define ADC_CHANNEL_TEMP      16
#define ADC_CHANNEL_VREFINT   17
#define ADC_CHANNEL_VBAT      18

typedef enum {
    ADC_MODE_SINGLE      = 0x0U,
    ADC_MODE_CONTINUOUS  = 0x1U,
    ADC_MODE_SCAN        = 0x2U,
    ADC_MODE_DISCONTINUOUS = 0x3U
} adc_conversion_mode_t;

typedef struct {
    uint32_t adc_base;
    uint8_t resolution;
    adc_conversion_mode_t mode;
    uint8_t num_channels;
    uint8_t *channels;
    uint32_t sample_time;
    bool dma_enable;
    bool continuous_dma;
} adc_config_t;

/* ── ADC API ─────────────────────────────────────────── */
void adc_init(const adc_config_t *config);
void adc_deinit(uint32_t adc_base);
void adc_start_conversion(uint32_t adc_base);
uint32_t adc_read(uint32_t adc_base, uint8_t channel);
void adc_read_multi(uint32_t adc_base, uint8_t *channels,
    uint32_t *results, uint8_t count);
void adc_start_dma(uint32_t adc_base, uint32_t *buffer, uint32_t size);
void adc_set_channel(uint32_t adc_base, uint8_t channel);
void adc_enable_temperature_sensor(void);
float adc_get_temperature(uint32_t adc_base);

/* ── CRC ─────────────────────────────────────────────── */
#define CRC_BASE              0x40023000UL

void crc_init(void);
uint32_t crc_calculate(const uint32_t *data, uint32_t size_words);
uint32_t crc_calculate_byte(const uint8_t *data, uint32_t size_bytes);
uint32_t crc_get_result(void);
void crc_reset(void);
void crc_set_polynomial(uint32_t polynomial);

/* ── Power management ────────────────────────────────── */
typedef enum {
    POWER_MODE_RUN      = 0x0U,
    POWER_MODE_SLEEP    = 0x1U,
    POWER_MODE_DEEPSLEEP = 0x2U,
    POWER_MODE_STANDBY  = 0x3U
} power_mode_t;

typedef enum {
    POWER_REGULATOR_MAIN    = 0x0U,
    POWER_REGULATOR_LP      = 0x1U
} power_regulator_t;

/* ── Power management API ────────────────────────────── */
void power_set_mode(power_mode_t mode);
power_mode_t power_get_mode(void);
void power_enter_sleep(void);
void power_enter_deep_sleep(void);
void power_enter_standby(void);
void power_enable_backup_regulator(void);
bool power_is_standby_reset(void);
void power_clear_standby_reset(void);
void power_set_regulator(power_regulator_t regulator);

/* ── Clock tree nodes ────────────────────────────────── */
typedef enum {
    CLOCK_SRC_HSI     = 0,
    CLOCK_SRC_HSE     = 1,
    CLOCK_SRC_PLL     = 2,
    CLOCK_SRC_LSI     = 3,
    CLOCK_SRC_LSE     = 4
} clock_source_t;

typedef enum {
    CLOCK_SYS = 0,
    CLOCK_HCLK = 1,
    CLOCK_PCLK1 = 2,
    CLOCK_PCLK2 = 3,
    CLOCK_PLL = 4,
    CLOCK_PLL48 = 5,
    CLOCK_TIM = 6
} clock_node_t;

typedef enum {
    CLOCK_PLL_SRC_HSI = 0,
    CLOCK_PLL_SRC_HSE = 1
} pll_clock_source_t;

typedef struct {
    clock_source_t sysclk_source;
    uint32_t hclk_prescaler;
    uint32_t pclk1_prescaler;
    uint32_t pclk2_prescaler;
    bool hse_enabled;
    uint32_t hse_freq_hz;
    bool pll_enabled;
    pll_clock_source_t pll_source;
    uint32_t pll_m;
    uint32_t pll_n;
    uint32_t pll_p;
    uint32_t pll_q;
} clock_config_t;

/* ── Clock API ───────────────────────────────────────── */
void clock_init(const clock_config_t *config);
void clock_deinit(void);
uint32_t clock_get_freq(clock_node_t node);
void clock_hse_enable(bool enable);
void clock_hse_bypass(bool enable);
void clock_pll_enable(bool enable);
void clock_pll_set_source(pll_clock_source_t source);
void clock_pll_configure(uint32_t m, uint32_t n, uint32_t p, uint32_t q);
bool clock_is_hse_ready(void);
bool clock_is_pll_ready(void);
bool clock_is_hsi_ready(void);
void clock_set_sysclk(clock_source_t source);
void clock_set_prescaler(clock_node_t node, uint32_t prescaler);
void clock_enable_peripheral(uint32_t base_addr);
void clock_disable_peripheral(uint32_t base_addr);

#endif /* MCU_PERIPHERAL_H */
