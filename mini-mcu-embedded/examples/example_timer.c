#include "mcu_peripheral.h"
#include "nvic_dma.h"
#include <stdio.h>
#include <string.h>

static void demo_basic_timer(void) {
    printf("\n=== Basic Timer Demo (TIM2) ===\n");

    timer_config_t tcfg;
    memset(&tcfg, 0, sizeof(tcfg));
    tcfg.tim_base           = TIM2_BASE;
    tcfg.prescaler          = 8400 - 1;
    tcfg.period             = 10000 - 1;
    tcfg.clock_division     = 0;
    tcfg.counter_mode       = TIM_COUNTER_UP;
    tcfg.auto_reload_preload = true;

    timer_init(&tcfg);
    printf("  TIM2: PSC=%lu ARR=%lu (1Hz @ 84MHz)\n",
           (unsigned long)tcfg.prescaler, (unsigned long)tcfg.period);

    uint32_t cnt = timer_get_counter(TIM2_BASE);
    printf("  Initial counter: %lu\n", (unsigned long)cnt);

    timer_set_counter(TIM2_BASE, 5000);
    printf("  Counter set to 5000\n");

    timer_enable_interrupt(TIM2_BASE, (1UL << 0));
    printf("  Update interrupt enabled\n");

    timer_start(TIM2_BASE);
    printf("  TIM2 started\n");

    timer_generate_update(TIM2_BASE);
    bool pending = timer_is_update_pending(TIM2_BASE);
    printf("  Update pending after generate: %s\n", pending ? "yes" : "no");

    timer_clear_update_flag(TIM2_BASE);
    timer_stop(TIM2_BASE);
    timer_deinit(TIM2_BASE);
    printf("  TIM2 stopped and deinitialized\n");
}

static void demo_pwm(void) {
    printf("\n=== PWM Demo (TIM3 CH1) ===\n");

    pwm_config_t pcfg;
    memset(&pcfg, 0, sizeof(pcfg));
    pcfg.tim_base     = TIM3_BASE;
    pcfg.channel      = TIM_CHANNEL_1;
    pcfg.mode         = PWM_MODE_FAST;
    pcfg.output_type  = PWM_OUTPUT_NORMAL;
    pcfg.duty_cycle   = 500;
    pcfg.period       = 1000;
    pcfg.output_enable = true;

    pwm_init(&pcfg);
    printf("  PWM: TIM3 CH1, period=1000, duty=500 (50%%)\n");

    pwm_set_duty_cycle(TIM3_BASE, TIM_CHANNEL_1, 750);
    printf("  Duty cycle updated to 750 (75%%)\n");

    pwm_start(TIM3_BASE, TIM_CHANNEL_1);
    printf("  PWM output started\n");

    pwm_set_period(TIM3_BASE, 2000);
    printf("  Period increased to 2000\n");

    pwm_set_polarity(TIM3_BASE, TIM_CHANNEL_1, PWM_OUTPUT_INVERTED);
    printf("  Polarity inverted\n");

    pwm_stop(TIM3_BASE, TIM_CHANNEL_1);
    printf("  PWM stopped\n");
}

static void demo_multi_channel_pwm(void) {
    printf("\n=== Multi-Channel PWM Demo (TIM1) ===\n");

    pwm_config_t ch1;
    memset(&ch1, 0, sizeof(ch1));
    ch1.tim_base      = TIM1_BASE;
    ch1.channel       = TIM_CHANNEL_1;
    ch1.mode          = PWM_MODE_CENTER_ALIGNED1;
    ch1.output_type   = PWM_OUTPUT_NORMAL;
    ch1.duty_cycle    = 250;
    ch1.period        = 1000;
    ch1.output_enable = true;
    pwm_init(&ch1);

    pwm_config_t ch2;
    memset(&ch2, 0, sizeof(ch2));
    ch2.tim_base      = TIM1_BASE;
    ch2.channel       = TIM_CHANNEL_2;
    ch2.mode          = PWM_MODE_CENTER_ALIGNED1;
    ch2.output_type   = PWM_OUTPUT_NORMAL;
    ch2.duty_cycle    = 500;
    ch2.period        = 1000;
    ch2.output_enable = true;
    pwm_init(&ch2);

    printf("  TIM1 CH1 duty=250 (25%%), CH2 duty=500 (50%%)\n");

    pwm_start(TIM1_BASE, TIM_CHANNEL_1);
    pwm_start(TIM1_BASE, TIM_CHANNEL_2);
    printf("  Both channels started\n");

    pwm_stop(TIM1_BASE, TIM_CHANNEL_1);
    pwm_stop(TIM1_BASE, TIM_CHANNEL_2);
    printf("  Both channels stopped\n");
}

static void demo_capture_compare(void) {
    printf("\n=== Capture/Compare Demo (TIM4) ===\n");

    capture_compare_config_t cc_cfg;
    memset(&cc_cfg, 0, sizeof(cc_cfg));
    cc_cfg.tim_base       = TIM4_BASE;
    cc_cfg.input_channel  = 1;
    cc_cfg.output_channel = 1;
    cc_cfg.input_edge     = TIM_EDGE_RISING;
    cc_cfg.prescaler      = 84 - 1;
    cc_cfg.period         = 0xFFFF;
    cc_cfg.dma_enable     = true;

    capture_compare_init(&cc_cfg);
    printf("  TIM4: Capture CH1, rising edge, 1MHz, period=0xFFFF\n");

    compare_set_value(TIM4_BASE, 1, 0x4000);
    uint32_t cmp_val = capture_get_value(TIM4_BASE, 1);
    printf("  Compare register: 0x%lX\n", (unsigned long)cmp_val);

    capture_compare_enable_dma(TIM4_BASE, 1);
    printf("  DMA enabled for capture\n");
}

static void demo_encoder(void) {
    printf("\n=== Encoder Demo (TIM5) ===\n");

    encoder_config_t enc_cfg;
    memset(&enc_cfg, 0, sizeof(enc_cfg));
    enc_cfg.tim_base        = TIM5_BASE;
    enc_cfg.mode            = TIM_ENCODER_MODE_BOTH;
    enc_cfg.counts_per_rev  = 400;
    enc_cfg.prescaler       = 0;

    encoder_init(&enc_cfg);
    printf("  TIM5 encoder mode: TI1+TI2, 400 counts/rev\n");

    int32_t pos = encoder_get_count(TIM5_BASE);
    bool dir = encoder_get_direction(TIM5_BASE);
    printf("  Position: %ld, Direction: %s\n", (long)pos, dir ? "forward" : "reverse");

    encoder_reset_count(TIM5_BASE);
    uint32_t speed = encoder_get_speed(TIM5_BASE, 100);
    printf("  Speed: %lu counts/sec\n", (unsigned long)speed);
}

static void demo_watchdog(void) {
    printf("\n=== Watchdog Demo ===\n");

    iwdg_init(4, 0xFFF);
    printf("  IWDG: prescaler=64, reload=0xFFF\n");

    iwdg_enable();
    printf("  IWDG enabled\n");

    iwdg_refresh();
    printf("  IWDG refreshed (kicked)\n");

    bool reset_flag = iwdg_is_reset_flagged();
    printf("  IWDG reset flag: %s\n", reset_flag ? "SET" : "CLEAR");

    iwdg_clear_reset_flag();

    wwdg_init(3, 0x50, 0x7F);
    printf("  WWDG: prescaler=8, window=0x50, counter=0x7F\n");

    wwdg_enable_early_wakeup();
    wwdg_refresh();
    printf("  WWDG refreshed\n");
}

int main(void) {
    printf("╔═══════════════════════════════════════╗\n");
    printf("║  mini-mcu Timer/PWM/Watchdog Demo     ║\n");
    printf("╚═══════════════════════════════════════╝\n");

    nvic_init();
    demo_basic_timer();
    demo_pwm();
    demo_multi_channel_pwm();
    demo_capture_compare();
    demo_encoder();
    demo_watchdog();

    printf("\n=== All timer/peripheral demos complete ===\n");
    return 0;
}
