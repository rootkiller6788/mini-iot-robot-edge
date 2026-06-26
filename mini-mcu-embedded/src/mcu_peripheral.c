#include "mcu_peripheral.h"
#include "memory_map.h"
#include <stdio.h>
#include <string.h>

#define TIM_CR1_OFFSET     0x00UL
#define TIM_CR2_OFFSET     0x04UL
#define TIM_SMCR_OFFSET    0x08UL
#define TIM_DIER_OFFSET    0x0CUL
#define TIM_SR_OFFSET      0x10UL
#define TIM_EGR_OFFSET     0x14UL
#define TIM_CCMR1_OFFSET   0x18UL
#define TIM_CCMR2_OFFSET   0x1CUL
#define TIM_CCER_OFFSET    0x20UL
#define TIM_CNT_OFFSET     0x24UL
#define TIM_PSC_OFFSET     0x28UL
#define TIM_ARR_OFFSET     0x2CUL
#define TIM_CCR1_OFFSET    0x34UL
#define TIM_CCR2_OFFSET    0x38UL
#define TIM_CCR3_OFFSET    0x3CUL
#define TIM_CCR4_OFFSET    0x40UL
#define TIM_BDTR_OFFSET    0x44UL

#define TIM_CR1_CEN        (1UL << 0)
#define TIM_CR1_UDIS       (1UL << 1)
#define TIM_CR1_URS        (1UL << 2)
#define TIM_CR1_ARPE       (1UL << 7)
#define TIM_CR1_CKD_POS    8

#define TIM_DIER_UIE       (1UL << 0)
#define TIM_DIER_CC1IE     (1UL << 1)

#define TIM_SR_UIF         (1UL << 0)

#define TIM_EGR_UG         (1UL << 0)

#define TIM_CCER_CC1E      (1UL << 0)
#define TIM_CCER_CC1P      (1UL << 1)

#define TIM_CCMR_OC1M_POS  4

static uint32_t s_tim_counter[TIM_BDTR_OFFSET + 4];

static volatile uint32_t *tim_reg(uint32_t base, uint32_t offset) {
    return (volatile uint32_t *)(base + offset);
}

static uint32_t s_rtc_timestamp;
static rtc_calendar_t s_rtc_calendar;
static bool s_rtc_alarm_enabled[2];

static uint32_t s_iwdg_counter;
static uint32_t s_iwdg_reload;
static bool s_iwdg_enabled;
static bool s_iwdg_reset_flagged;

static uint32_t s_adc_values[19];
static bool s_temp_sensor_enabled;

static uint32_t s_crc_result;
static uint32_t s_crc_polynomial;

static power_mode_t s_power_mode;
static power_regulator_t s_power_regulator;
static bool s_backup_regulator_enabled;

static clock_config_t s_clock_cfg;
static uint32_t s_periph_clocks_enabled[64];

void timer_init(const timer_config_t *config) {
    if (!config) return;
    volatile uint32_t *cr1 = tim_reg(config->tim_base, TIM_CR1_OFFSET);
    volatile uint32_t *psc = tim_reg(config->tim_base, TIM_PSC_OFFSET);
    volatile uint32_t *arr = tim_reg(config->tim_base, TIM_ARR_OFFSET);
    volatile uint32_t *cnt = tim_reg(config->tim_base, TIM_CNT_OFFSET);
    *cr1 = 0;
    *psc = config->prescaler;
    *arr = config->period;
    *cnt = 0;
    if (config->auto_reload_preload)
        *cr1 |= TIM_CR1_ARPE;
    *cr1 |= ((uint32_t)config->clock_division << TIM_CR1_CKD_POS);
}

void timer_deinit(uint32_t tim_base) {
    volatile uint32_t *cr1 = tim_reg(tim_base, TIM_CR1_OFFSET);
    *cr1 = 0;
}

void timer_start(uint32_t tim_base) {
    volatile uint32_t *cr1 = tim_reg(tim_base, TIM_CR1_OFFSET);
    *cr1 |= TIM_CR1_CEN;
}

void timer_stop(uint32_t tim_base) {
    volatile uint32_t *cr1 = tim_reg(tim_base, TIM_CR1_OFFSET);
    *cr1 &= ~TIM_CR1_CEN;
}

void timer_set_counter(uint32_t tim_base, uint32_t value) {
    volatile uint32_t *cnt = tim_reg(tim_base, TIM_CNT_OFFSET);
    *cnt = value;
}

uint32_t timer_get_counter(uint32_t tim_base) {
    volatile uint32_t *cnt = tim_reg(tim_base, TIM_CNT_OFFSET);
    return *cnt;
}

void timer_set_period(uint32_t tim_base, uint32_t period) {
    volatile uint32_t *arr = tim_reg(tim_base, TIM_ARR_OFFSET);
    *arr = period;
}

void timer_set_prescaler(uint32_t tim_base, uint32_t prescaler) {
    volatile uint32_t *psc = tim_reg(tim_base, TIM_PSC_OFFSET);
    *psc = prescaler;
}

void timer_generate_update(uint32_t tim_base) {
    volatile uint32_t *egr = tim_reg(tim_base, TIM_EGR_OFFSET);
    *egr |= TIM_EGR_UG;
}

bool timer_is_update_pending(uint32_t tim_base) {
    volatile uint32_t *sr = tim_reg(tim_base, TIM_SR_OFFSET);
    return (*sr & TIM_SR_UIF) != 0;
}

void timer_clear_update_flag(uint32_t tim_base) {
    volatile uint32_t *sr = tim_reg(tim_base, TIM_SR_OFFSET);
    *sr &= ~TIM_SR_UIF;
}

void timer_enable_interrupt(uint32_t tim_base, uint16_t flags) {
    volatile uint32_t *dier = tim_reg(tim_base, TIM_DIER_OFFSET);
    *dier |= flags;
}

void timer_disable_interrupt(uint32_t tim_base, uint16_t flags) {
    volatile uint32_t *dier = tim_reg(tim_base, TIM_DIER_OFFSET);
    *dier &= ~flags;
}

void pwm_init(const pwm_config_t *config) {
    if (!config) return;
    timer_config_t tcfg;
    memset(&tcfg, 0, sizeof(tcfg));
    tcfg.tim_base = config->tim_base;
    tcfg.prescaler = 0;
    tcfg.period = config->period;
    tcfg.auto_reload_preload = true;
    timer_init(&tcfg);

    uint32_t ccmr_offset = (config->channel <= TIM_CHANNEL_2) ? TIM_CCMR1_OFFSET : TIM_CCMR2_OFFSET;
    volatile uint32_t *ccmr = tim_reg(config->tim_base, ccmr_offset);
    uint32_t shift = (config->channel & 1) ? 8 : 0;
    *ccmr &= ~(0x7FUL << shift);
    *ccmr |= (((uint32_t)config->mode + 6) << (shift + TIM_CCMR_OC1M_POS));

    volatile uint32_t *ccer = tim_reg(config->tim_base, TIM_CCER_OFFSET);
    uint32_t ch_shift = (config->channel * 4);
    if (config->output_enable)
        *ccer |= (TIM_CCER_CC1E << ch_shift);
    if (config->output_type == PWM_OUTPUT_INVERTED)
        *ccer |= (TIM_CCER_CC1P << ch_shift);

    pwm_set_duty_cycle(config->tim_base, config->channel, config->duty_cycle);
}

void pwm_set_duty_cycle(uint32_t tim_base, uint8_t channel, uint32_t duty) {
    uint32_t ccr_offset = TIM_CCR1_OFFSET + ((channel - TIM_CHANNEL_1) * 4UL);
    volatile uint32_t *ccr = tim_reg(tim_base, ccr_offset);
    *ccr = duty;
}

void pwm_set_period(uint32_t tim_base, uint32_t period) {
    timer_set_period(tim_base, period);
}

void pwm_start(uint32_t tim_base, uint8_t channel) {
    volatile uint32_t *ccer = tim_reg(tim_base, TIM_CCER_OFFSET);
    *ccer |= (TIM_CCER_CC1E << (channel * 4));
    timer_start(tim_base);
}

void pwm_stop(uint32_t tim_base, uint8_t channel) {
    volatile uint32_t *ccer = tim_reg(tim_base, TIM_CCER_OFFSET);
    *ccer &= ~(TIM_CCER_CC1E << (channel * 4));
}

void pwm_enable_output(uint32_t tim_base, uint8_t channel) {
    volatile uint32_t *ccer = tim_reg(tim_base, TIM_CCER_OFFSET);
    *ccer |= (TIM_CCER_CC1E << (channel * 4));
}

void pwm_disable_output(uint32_t tim_base, uint8_t channel) {
    volatile uint32_t *ccer = tim_reg(tim_base, TIM_CCER_OFFSET);
    *ccer &= ~(TIM_CCER_CC1E << (channel * 4));
}

void pwm_set_polarity(uint32_t tim_base, uint8_t channel, pwm_output_t polarity) {
    volatile uint32_t *ccer = tim_reg(tim_base, TIM_CCER_OFFSET);
    if (polarity == PWM_OUTPUT_INVERTED)
        *ccer |= (TIM_CCER_CC1P << (channel * 4));
    else
        *ccer &= ~(TIM_CCER_CC1P << (channel * 4));
}

void capture_compare_init(const capture_compare_config_t *config) {
    if (!config) return;
    timer_config_t tcfg;
    memset(&tcfg, 0, sizeof(tcfg));
    tcfg.tim_base = config->tim_base;
    tcfg.prescaler = config->prescaler;
    tcfg.period = config->period;
    timer_init(&tcfg);
}

uint32_t capture_get_value(uint32_t tim_base, uint8_t channel) {
    uint32_t ccr_offset = TIM_CCR1_OFFSET + ((channel - TIM_CHANNEL_1) * 4UL);
    volatile uint32_t *ccr = tim_reg(tim_base, ccr_offset);
    return *ccr;
}

void compare_set_value(uint32_t tim_base, uint8_t channel, uint32_t value) {
    uint32_t ccr_offset = TIM_CCR1_OFFSET + ((channel - TIM_CHANNEL_1) * 4UL);
    volatile uint32_t *ccr = tim_reg(tim_base, ccr_offset);
    *ccr = value;
}

void capture_compare_enable_dma(uint32_t tim_base, uint8_t channel) {
    volatile uint32_t *dier = tim_reg(tim_base, TIM_DIER_OFFSET);
    *dier |= (1UL << (9 + channel));
}

void encoder_init(const encoder_config_t *config) {
    if (!config) return;
    volatile uint32_t *smcr = tim_reg(config->tim_base, TIM_SMCR_OFFSET);
    *smcr = (uint32_t)config->mode;
    timer_config_t tcfg;
    memset(&tcfg, 0, sizeof(tcfg));
    tcfg.tim_base = config->tim_base;
    tcfg.prescaler = config->prescaler;
    tcfg.period = config->counts_per_rev;
    timer_init(&tcfg);
}

int32_t encoder_get_count(uint32_t tim_base) {
    volatile uint32_t *cnt = tim_reg(tim_base, TIM_CNT_OFFSET);
    return (int32_t)*cnt;
}

void encoder_reset_count(uint32_t tim_base) {
    volatile uint32_t *cnt = tim_reg(tim_base, TIM_CNT_OFFSET);
    *cnt = 0;
}

uint32_t encoder_get_speed(uint32_t tim_base, uint32_t sample_period_ms) {
    int32_t count = encoder_get_count(tim_base);
    encoder_reset_count(tim_base);
    if (sample_period_ms == 0) return 0;
    return (uint32_t)(count < 0 ? -count : count) * 1000UL / sample_period_ms;
}

bool encoder_get_direction(uint32_t tim_base) {
    return encoder_get_count(tim_base) >= 0;
}

void rtc_init(void) {
    s_rtc_timestamp = 0;
    memset(&s_rtc_calendar, 0, sizeof(s_rtc_calendar));
    s_rtc_calendar.year = 24;
    s_rtc_calendar.month = 1;
    s_rtc_calendar.day = 1;
    s_rtc_alarm_enabled[0] = false;
    s_rtc_alarm_enabled[1] = false;
}

void rtc_set_calendar(const rtc_calendar_t *calendar) {
    if (calendar) memcpy(&s_rtc_calendar, calendar, sizeof(rtc_calendar_t));
}

void rtc_get_calendar(rtc_calendar_t *calendar) {
    if (calendar) memcpy(calendar, &s_rtc_calendar, sizeof(rtc_calendar_t));
}

void rtc_set_alarm(const rtc_alarm_config_t *config) {
    (void)config;
}

void rtc_get_alarm(rtc_alarm_t alarm, rtc_alarm_config_t *config) {
    if (config) memset(config, 0, sizeof(*config));
    (void)alarm;
}

void rtc_enable_alarm(rtc_alarm_t alarm) {
    s_rtc_alarm_enabled[alarm] = true;
}

void rtc_disable_alarm(rtc_alarm_t alarm) {
    s_rtc_alarm_enabled[alarm] = false;
}

void rtc_clear_alarm_flag(rtc_alarm_t alarm) {
    (void)alarm;
}

void rtc_set_wakeup_timer(uint32_t period, rtc_wakeup_clock_t clock) {
    (void)period;
    (void)clock;
}

void rtc_enable_wakeup_timer(void) {
}

void rtc_disable_wakeup_timer(void) {
}

uint32_t rtc_get_timestamp(void) {
    return s_rtc_timestamp;
}

void rtc_set_timestamp(uint32_t timestamp) {
    s_rtc_timestamp = timestamp;
}

void iwdg_init(uint32_t prescaler, uint32_t reload) {
    s_iwdg_counter = 0;
    s_iwdg_reload = (reload > IWDG_TIMEOUT_MAX) ? IWDG_TIMEOUT_MAX : reload;
    (void)prescaler;
}

void iwdg_refresh(void) {
    s_iwdg_counter = 0;
}

void iwdg_enable(void) {
    s_iwdg_enabled = true;
}

bool iwdg_is_reset_flagged(void) {
    return s_iwdg_reset_flagged;
}

void iwdg_clear_reset_flag(void) {
    s_iwdg_reset_flagged = false;
}

void wwdg_init(uint32_t prescaler, uint32_t window, uint32_t counter) {
    (void)prescaler;
    (void)window;
    (void)counter;
}

void wwdg_refresh(void) {
}

void wwdg_enable_early_wakeup(void) {
}

void adc_init(const adc_config_t *config) {
    if (!config) return;
    for (uint8_t i = 0; i < config->num_channels && config->channels; i++) {
        s_adc_values[config->channels[i]] = 0;
    }
}

void adc_deinit(uint32_t adc_base) {
    (void)adc_base;
}

void adc_start_conversion(uint32_t adc_base) {
    (void)adc_base;
}

uint32_t adc_read(uint32_t adc_base, uint8_t channel) {
    if (channel > 18) return 0;
    (void)adc_base;
    return s_adc_values[channel];
}

void adc_read_multi(uint32_t adc_base, uint8_t *channels,
    uint32_t *results, uint8_t count)
{
    if (!channels || !results) return;
    for (uint8_t i = 0; i < count; i++) {
        results[i] = adc_read(adc_base, channels[i]);
    }
}

void adc_start_dma(uint32_t adc_base, uint32_t *buffer, uint32_t size) {
    (void)adc_base;
    (void)buffer;
    (void)size;
}

void adc_set_channel(uint32_t adc_base, uint8_t channel) {
    (void)adc_base;
    (void)channel;
}

void adc_enable_temperature_sensor(void) {
    s_temp_sensor_enabled = true;
    s_adc_values[ADC_CHANNEL_TEMP] = 1725;
}

float adc_get_temperature(uint32_t adc_base) {
    (void)adc_base;
    uint32_t raw = s_adc_values[ADC_CHANNEL_TEMP];
    float v_sense = (float)raw * 3.3f / 4095.0f;
    return (v_sense - 0.76f) / 0.0025f + 25.0f;
}

void crc_init(void) {
    s_crc_result = 0xFFFFFFFFUL;
    s_crc_polynomial = 0x04C11DB7UL;
}

uint32_t crc_calculate(const uint32_t *data, uint32_t size_words) {
    if (!data) return 0;
    s_crc_result = 0xFFFFFFFFUL;
    for (uint32_t i = 0; i < size_words; i++) {
        s_crc_result ^= data[i];
        for (uint8_t b = 0; b < 32; b++) {
            if (s_crc_result & 0x80000000UL)
                s_crc_result = (s_crc_result << 1) ^ s_crc_polynomial;
            else
                s_crc_result <<= 1;
        }
    }
    return s_crc_result;
}

uint32_t crc_calculate_byte(const uint8_t *data, uint32_t size_bytes) {
    if (!data) return 0;
    s_crc_result = 0xFFFFFFFFUL;
    for (uint32_t i = 0; i < size_bytes; i++) {
        s_crc_result ^= (uint32_t)data[i] << 24;
        for (uint8_t b = 0; b < 8; b++) {
            if (s_crc_result & 0x80000000UL)
                s_crc_result = (s_crc_result << 1) ^ s_crc_polynomial;
            else
                s_crc_result <<= 1;
        }
    }
    return s_crc_result;
}

uint32_t crc_get_result(void) {
    return s_crc_result;
}

void crc_reset(void) {
    s_crc_result = 0xFFFFFFFFUL;
}

void crc_set_polynomial(uint32_t polynomial) {
    s_crc_polynomial = polynomial;
}

void power_set_mode(power_mode_t mode) {
    s_power_mode = mode;
}

power_mode_t power_get_mode(void) {
    return s_power_mode;
}

void power_enter_sleep(void) {
    s_power_mode = POWER_MODE_SLEEP;
}

void power_enter_deep_sleep(void) {
    s_power_mode = POWER_MODE_DEEPSLEEP;
}

void power_enter_standby(void) {
    s_power_mode = POWER_MODE_STANDBY;
}

void power_enable_backup_regulator(void) {
    s_backup_regulator_enabled = true;
}

bool power_is_standby_reset(void) {
    return false;
}

void power_clear_standby_reset(void) {
}

void power_set_regulator(power_regulator_t regulator) {
    s_power_regulator = regulator;
}

void clock_init(const clock_config_t *config) {
    if (!config) return;
    memcpy(&s_clock_cfg, config, sizeof(clock_config_t));
}

void clock_deinit(void) {
    memset(&s_clock_cfg, 0, sizeof(s_clock_cfg));
}

uint32_t clock_get_freq(clock_node_t node) {
    uint32_t sysclk = 168000000UL;
    switch (node) {
        case CLOCK_SYS:   return sysclk;
        case CLOCK_HCLK:  return sysclk / s_clock_cfg.hclk_prescaler;
        case CLOCK_PCLK1: return (sysclk / s_clock_cfg.hclk_prescaler) / s_clock_cfg.pclk1_prescaler;
        case CLOCK_PCLK2: return (sysclk / s_clock_cfg.hclk_prescaler) / s_clock_cfg.pclk2_prescaler;
        case CLOCK_PLL:   return (s_clock_cfg.hse_freq_hz / s_clock_cfg.pll_m) * s_clock_cfg.pll_n / s_clock_cfg.pll_p;
        case CLOCK_PLL48: return 48000000UL;
        case CLOCK_TIM:   return 168000000UL;
        default: return 0;
    }
}

void clock_hse_enable(bool enable) {
    s_clock_cfg.hse_enabled = enable;
}

void clock_hse_bypass(bool enable) {
    (void)enable;
}

void clock_pll_enable(bool enable) {
    s_clock_cfg.pll_enabled = enable;
}

void clock_pll_set_source(pll_clock_source_t source) {
    s_clock_cfg.pll_source = source;
}

void clock_pll_configure(uint32_t m, uint32_t n, uint32_t p, uint32_t q) {
    s_clock_cfg.pll_m = m;
    s_clock_cfg.pll_n = n;
    s_clock_cfg.pll_p = p;
    s_clock_cfg.pll_q = q;
}

bool clock_is_hse_ready(void) {
    return s_clock_cfg.hse_enabled;
}

bool clock_is_pll_ready(void) {
    return s_clock_cfg.pll_enabled;
}

bool clock_is_hsi_ready(void) {
    return true;
}

void clock_set_sysclk(clock_source_t source) {
    s_clock_cfg.sysclk_source = source;
}

void clock_set_prescaler(clock_node_t node, uint32_t prescaler) {
    switch (node) {
        case CLOCK_HCLK:  s_clock_cfg.hclk_prescaler = prescaler; break;
        case CLOCK_PCLK1: s_clock_cfg.pclk1_prescaler = prescaler; break;
        case CLOCK_PCLK2: s_clock_cfg.pclk2_prescaler = prescaler; break;
        default: break;
    }
}

void clock_enable_peripheral(uint32_t base_addr) {
    for (uint8_t i = 0; i < 64; i++) {
        if (s_periph_clocks_enabled[i] == 0) {
            s_periph_clocks_enabled[i] = base_addr;
            break;
        }
    }
}

void clock_disable_peripheral(uint32_t base_addr) {
    for (uint8_t i = 0; i < 64; i++) {
        if (s_periph_clocks_enabled[i] == base_addr) {
            s_periph_clocks_enabled[i] = 0;
            break;
        }
    }
}
