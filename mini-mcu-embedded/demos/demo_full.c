/*
 * demo_full.c - Full Demonstration of mini-mcu-embedded
 *
 * Walks through all five sub-modules:
 *   cortex_m_core.h, gpio_uart.h, mcu_peripheral.h, memory_map.h, nvic_dma.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "cortex_m_core.h"
#include "gpio_uart.h"
#include "mcu_peripheral.h"
#include "memory_map.h"
#include "nvic_dma.h"

int main(void) {
    printf("\n");
    printf("*************************************************************\n");
    printf("*                                                           *\n");
    printf("*     MINI-MCU-EMBEDDED  --  Full Feature Demonstration     *\n");
    printf("*    Cortex-M4 Core | GPIO/UART/SPI/I2C | Timers | Power    *\n");
    printf("*        Memory Map | NVIC | DMA | Scatter-Gather           *\n");
    printf("*                                                           *\n");
    printf("*************************************************************\n");
    printf("\n");

    /* ---------------------------------------------------------------
     *  SECTION 1 -- cortex_m_core.h  (NVIC, SysTick, SCB, MPU)
     * --------------------------------------------------------------- */
    printf("--- Section 1: Cortex-M4 Core (NVIC / SysTick / SCB / MPU) ---\n\n");

    {
        uint8_t irq = NVIC_IRQ_USART1;
        cortex_m4_nvic_enable_irq(irq);
        printf("[OK] cortex_m4_nvic_enable_irq(%d)\n", irq);
        cortex_m4_nvic_set_priority(irq, NVIC_PRIORITY_MEDIUM);
        uint8_t pri = cortex_m4_nvic_get_priority(irq);
        printf("[OK] cortex_m4_nvic_set_priority / get_priority -> 0x%02X\n", pri);
        cortex_m4_nvic_set_pending(irq);
        bool pend = cortex_m4_nvic_is_pending(irq);
        printf("[OK] cortex_m4_nvic_set_pending / is_pending -> %s\n",
               pend ? "true" : "false");
        cortex_m4_nvic_clear_pending(irq);
        cortex_m4_nvic_disable_irq(irq);
        printf("[OK] cortex_m4_nvic_clear_pending / disable_irq(%d)\n", irq);
    }

    {
        cortex_m4_systick_init(72000);
        printf("[OK] cortex_m4_systick_init(reload=72000)\n");
        cortex_m4_systick_enable();
        cortex_m4_systick_enable_interrupt();
        printf("[OK] cortex_m4_systick_enable + interrupt\n");
        uint32_t current = cortex_m4_systick_get_current();
        bool expired = cortex_m4_systick_has_expired();
        printf("[OK] systick current=%u expired=%s\n", current,
               expired ? "true" : "false");
        cortex_m4_systick_disable_interrupt();
        cortex_m4_systick_disable();
        printf("[OK] systick disabled\n");
    }

    {
        uint32_t cpuid = cortex_m4_scb_get_cpu_id();
        printf("[OK] cortex_m4_scb_get_cpu_id -> 0x%08lX\n", (unsigned long)cpuid);
        cortex_m4_scb_set_vector_table_offset(0x08000000UL);
        uint32_t vt = cortex_m4_scb_get_vector_table_offset();
        printf("[OK] SCB vector table offset -> 0x%08lX\n", (unsigned long)vt);
        cortex_m4_scb_set_priority_grouping(PRIGROUP_NVIC_4_2);
        priority_grouping_t grp = cortex_m4_scb_get_priority_grouping();
        printf("[OK] SCB priority grouping -> %d (4 group, 2 sub)\n", (int)grp);
        cortex_m4_scb_enable_fault_handlers();
        printf("[OK] SCB fault handlers enabled\n");
    }

    {
        cortex_m4_mpu_configure_region(0, 0x20000000UL,
            MPU_REGION_SIZE_64K, MPU_ACCESS_FULL, 0, true, true, false, false);
        printf("[OK] MPU region 0: SRAM 64K, FULL access\n");
        cortex_m4_mpu_enable_hard_fault_nmi(true);
        cortex_m4_mpu_enable_default_map(true);
        cortex_m4_mpu_enable();
        printf("[OK] MPU enabled (HFNMI + default map)\n");
        cortex_m4_mpu_disable();
        printf("[OK] MPU disabled\n");
    }

    {
        processor_mode_t mode = cortex_m4_get_processor_mode();
        printf("[OK] processor mode -> %s\n",
               mode == PROCESSOR_MODE_THREAD ? "THREAD" : "HANDLER");
    }

    printf("\n");

    /* ---------------------------------------------------------------
     *  SECTION 2 -- gpio_uart.h  (GPIO, UART, SPI, I2C)
     * --------------------------------------------------------------- */
    printf("--- Section 2: GPIO / UART / SPI / I2C ---\n\n");

    {
        gpio_pin_t led = { GPIOA_BASE, GPIO_PIN_5 };
        gpio_init(led, GPIO_MODE_OUTPUT, GPIO_OTYPE_PUSH_PULL,
                  GPIO_PUPD_NONE, GPIO_SPEED_HIGH);
        printf("[OK] gpio_init(PA5, OUTPUT, PUSH_PULL)\n");
        gpio_write_pin(led, true);
        bool state = gpio_read_pin(led);
        printf("[OK] gpio_write/read -> %s\n", state ? "HIGH" : "LOW");
        gpio_toggle_pin(led);
        state = gpio_read_pin(led);
        printf("[OK] gpio_toggle -> %s\n", state ? "HIGH" : "LOW");
        gpio_set_pull(led, GPIO_PUPD_DOWN);
        printf("[OK] gpio_set_pull(PULL_DOWN)\n");
        gpio_write_port(GPIOB_BASE, 0x00FF);
        uint16_t pv = gpio_read_port(GPIOB_BASE);
        printf("[OK] gpio_write/read_port(GPIOB, 0x00FF) -> 0x%04X\n", pv);
    }

    {
        uart_config_t uart_cfg = { USART1_BASE, 115200, UART_WORD_LENGTH_8,
            UART_STOP_BITS_1, UART_PARITY_NONE, UART_FLOW_CTRL_NONE,
            UART_OVERSAMPLING_16 };
        uart_init(&uart_cfg);
        printf("[OK] uart_init(USART1, 115200-8N1)\n");
        uart_transmit_byte(USART1_BASE, 0x55);
        uint8_t rx = uart_receive_byte(USART1_BASE);
        printf("[OK] uart tx(0x55) / rx -> 0x%02X\n", rx);
        uint32_t sr = uart_get_status(USART1_BASE);
        printf("[OK] uart status -> 0x%02lX\n", (unsigned long)sr);
        uart_deinit(USART1_BASE);
        printf("[OK] uart_deinit\n");
    }

    {
        spi_config_t spi_cfg = { SPI1_BASE, SPI_MODE_MASTER,
            SPI_DIRECTION_2LINES, SPI_DATA_SIZE_8BIT,
            SPI_CPOL_LOW, SPI_CPHA_1EDGE, SPI_BAUD_PRESCALER_8,
            SPI_FRAME_MSB_FIRST, false };
        spi_init(&spi_cfg);
        printf("[OK] spi_init(SPI1, MASTER, Mode 0)\n");
        uint8_t sr = spi_transmit_receive_8(SPI1_BASE, 0xAA);
        printf("[OK] spi_transfer(0xAA) -> 0x%02X\n", sr);
        bool busy = spi_is_busy(SPI1_BASE);
        printf("[OK] spi_is_busy -> %s\n", busy ? "true" : "false");
        spi_deinit(SPI1_BASE);
        printf("[OK] spi_deinit\n");
    }

    {
        i2c_config_t i2c_cfg = { I2C1_BASE, 100000, I2C_DUTY_CYCLE_2,
            0x10, I2C_ADDR_MODE_7BIT, false, false };
        i2c_init(&i2c_cfg);
        printf("[OK] i2c_init(I2C1, 100kHz)\n");
        bool ready = i2c_is_device_ready(I2C1_BASE, 0x50, 3);
        printf("[OK] i2c_is_device_ready(0x50) -> %s\n",
               ready ? "ready" : "nack");
        uint8_t tx[2] = { 0x00, 0x42 };
        bool ok = i2c_master_transmit(I2C1_BASE, 0x50, tx, 2);
        printf("[OK] i2c_master_transmit -> %s\n", ok ? "OK" : "FAIL");
        i2c_deinit(I2C1_BASE);
        printf("[OK] i2c_deinit\n");
    }

    printf("\n");

    /* ---------------------------------------------------------------
     *  SECTION 3 -- mcu_peripheral.h  (Timer, PWM, Capture, Encoder)
     * --------------------------------------------------------------- */
    printf("--- Section 3: Timers / PWM / Capture-Compare / Encoder ---\n\n");

    {
        timer_config_t tim_cfg = { TIM2_BASE, 7200, 10000, 0,
            TIM_COUNTER_UP, 0, true };
        timer_init(&tim_cfg);
        printf("[OK] timer_init(TIM2, presc=7200, period=10000)\n");
        timer_start(TIM2_BASE);
        uint32_t cnt = timer_get_counter(TIM2_BASE);
        printf("[OK] timer start -> counter=%u\n", cnt);
        timer_set_period(TIM2_BASE, 20000);
        timer_set_prescaler(TIM2_BASE, 3600);
        printf("[OK] timer period=20000 prescaler=3600\n");
        timer_clear_update_flag(TIM2_BASE);
        timer_stop(TIM2_BASE);
        printf("[OK] timer stop\n");
    }

    {
        pwm_config_t pwm_cfg = { TIM3_BASE, TIM_CHANNEL_1, PWM_MODE_FAST,
            PWM_OUTPUT_NORMAL, 5000, 10000, true };
        pwm_init(&pwm_cfg);
        printf("[OK] pwm_init(TIM3_CH1, FAST, duty=50%%)\n");
        pwm_set_duty_cycle(TIM3_BASE, TIM_CHANNEL_1, 7500);
        pwm_start(TIM3_BASE, TIM_CHANNEL_1);
        printf("[OK] pwm duty=75%% started\n");
        pwm_enable_output(TIM3_BASE, TIM_CHANNEL_1);
        pwm_disable_output(TIM3_BASE, TIM_CHANNEL_1);
        pwm_stop(TIM3_BASE, TIM_CHANNEL_1);
        printf("[OK] pwm output toggled + stop\n");
    }

    {
        capture_compare_config_t cap_cfg = { TIM4_BASE, TIM_CHANNEL_1,
            TIM_CHANNEL_2, TIM_EDGE_RISING, 0, 0xFFFF, true };
        capture_compare_init(&cap_cfg);
        printf("[OK] capture_compare_init(TIM4, edge=RISING, DMA)\n");
        uint32_t cap_val = capture_get_value(TIM4_BASE, TIM_CHANNEL_1);
        compare_set_value(TIM4_BASE, TIM_CHANNEL_2, 5000);
        printf("[OK] capture=%u compare=5000\n", cap_val);
    }

    {
        encoder_config_t enc_cfg = { TIM5_BASE, TIM_ENCODER_MODE_BOTH,
            2048, 0 };
        encoder_init(&enc_cfg);
        printf("[OK] encoder_init(TIM5, BOTH, 2048 CPR)\n");
        int32_t count = encoder_get_count(TIM5_BASE);
        bool dir = encoder_get_direction(TIM5_BASE);
        uint32_t speed = encoder_get_speed(TIM5_BASE, 100);
        printf("[OK] encoder count=%d dir=%s speed=%u RPM\n",
               (int)count, dir ? "fwd" : "rev", speed);
    }

    printf("\n");

    /* ---------------------------------------------------------------
     *  SECTION 4 -- mcu_peripheral.h  (RTC, WDG, ADC, Clock, Power, CRC)
     * --------------------------------------------------------------- */
    printf("--- Section 4: RTC / WDG / ADC / Clock / Power / CRC ---\n\n");

    {
        rtc_init();
        printf("[OK] rtc_init\n");
        rtc_calendar_t cal = { 25, 12, 25, 5, 10, 30, 0, RTC_FORMAT_BIN };
        rtc_set_calendar(&cal);
        rtc_calendar_t rb;
        rtc_get_calendar(&rb);
        printf("[OK] rtc set/get -> %02d-%02d-%02d %02d:%02d:%02d\n",
               rb.year, rb.month, rb.day, rb.hour, rb.minute, rb.second);
        uint32_t ts = rtc_get_timestamp();
        printf("[OK] rtc timestamp -> %u\n", ts);
        rtc_set_wakeup_timer(1000, RTC_WAKEUP_CLOCK_CK_SPRE);
        rtc_enable_wakeup_timer();
        printf("[OK] rtc wakeup timer set (1s)\n");
    }

    {
        iwdg_init(4, 0xFFF);
        iwdg_refresh();
        printf("[OK] iwdg_init + refresh\n");
        wwdg_init(0, 0x7F, 0x7F);
        wwdg_refresh();
        printf("[OK] wwdg_init + refresh\n");
    }

    {
        uint8_t ch = 5;
        adc_config_t adc_cfg = { ADC1_BASE, ADC_RESOLUTION_12BIT,
            ADC_MODE_SINGLE, 1, &ch, 15, false, false };
        adc_init(&adc_cfg);
        uint32_t val = adc_read(ADC1_BASE, 5);
        printf("[OK] adc_init + read(CH5) -> %u (%.2f V)\n",
               val, (float)val * 3.3f / 4095.0f);
        adc_enable_temperature_sensor();
        float temp = adc_get_temperature(ADC1_BASE);
        printf("[OK] adc temperature -> %.1f C\n", temp);
        adc_deinit(ADC1_BASE);
        printf("[OK] adc_deinit\n");
    }

    {
        clock_config_t clk_cfg = { CLOCK_SRC_HSI, 1, 2, 2,
            false, 0, true, CLOCK_PLL_SRC_HSI, 8, 72, 2, 2 };
        clock_init(&clk_cfg);
        uint32_t sys_freq = clock_get_freq(CLOCK_SYS);
        uint32_t hclk = clock_get_freq(CLOCK_HCLK);
        printf("[OK] clock init -> SYS=%u Hz HCLK=%u Hz\n", sys_freq, hclk);
        clock_set_sysclk(CLOCK_SRC_HSI);
        printf("[OK] clock switch to HSI\n");
    }

    {
        power_set_mode(POWER_MODE_SLEEP);
        power_enter_sleep();
        printf("[OK] power_enter_sleep\n");
        power_enter_standby();
        printf("[OK] power_enter_standby\n");
        power_set_regulator(POWER_REGULATOR_MAIN);
        printf("[OK] power regulator -> MAIN\n");
    }

    {
        crc_init();
        uint32_t data[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
        uint32_t crc_res = crc_calculate(data, 8);
        printf("[OK] crc_calculate(8 words) -> 0x%08lX\n", (unsigned long)crc_res);
        crc_set_polynomial(0x04C11DB7);
        crc_res = crc_calculate_byte((const uint8_t *)data, 32);
        printf("[OK] crc_calculate_byte(CRC32 poly) -> 0x%08lX\n", (unsigned long)crc_res);
        crc_reset();
        printf("[OK] crc_reset\n");
    }

    printf("\n");

    /* ---------------------------------------------------------------
     *  SECTION 5 -- memory_map.h + nvic_dma.h
     * --------------------------------------------------------------- */
    printf("--- Section 5: Memory Map / NVIC Handler / DMA / Scatter-Gather ---\n\n");

    {
        memory_map_ctx_t mem_ctx;
        memory_map_init(&mem_ctx);
        memory_map_add_region(&mem_ctx, "FLASH", FLASH_BASE, FLASH_SIZE_DEFAULT,
            MEM_TYPE_FLASH, MEM_ACCESS_RX, false, false);
        memory_map_add_region(&mem_ctx, "SRAM", SRAM_BASE, SRAM_SIZE_DEFAULT,
            MEM_TYPE_SRAM, MEM_ACCESS_RW, false, false);
        memory_map_add_region(&mem_ctx, "CCMRAM", CCMRAM_BASE,
            CCMRAM_SIZE_DEFAULT, MEM_TYPE_CCMRAM, MEM_ACCESS_RW, false, false);
        printf("[OK] memory_map_init + 3 regions\n");
        printf("[OK] addr_valid(FLASH)=%s in_sram=%s in_periph=%s\n",
               memory_map_is_address_valid(&mem_ctx, FLASH_BASE) ? "Y" : "N",
               memory_map_is_in_sram(&mem_ctx, SRAM_BASE + 0x1000) ? "Y" : "N",
               memory_map_is_in_peripheral(&mem_ctx, PERIPH_BASE) ? "Y" : "N");
        printf("[OK] free SRAM=%u B  free FLASH=%u B\n",
               memory_map_get_free_sram(&mem_ctx),
               memory_map_get_free_flash(&mem_ctx));
        memory_map_configure_section(&mem_ctx, MEM_SECTION_DATA,
            FLASH_BASE + FLASH_SIZE_DEFAULT, SRAM_BASE,
            32 * 1024, 4, true, false);
        memory_map_configure_section(&mem_ctx, MEM_SECTION_BSS,
            0, SRAM_BASE + 32 * 1024, 16 * 1024, 4, false, true);
        printf("[OK] sections configured (DATA 32K, BSS 16K)\n");
        bitband_write(SRAM_BASE + 0x100, 3, true);
        bool bit = bitband_read(SRAM_BASE + 0x100, 3);
        printf("[OK] bitband write/read bit 3 -> %s\n", bit ? "1" : "0");
    }

    {
        nvic_init();
        nvic_enable_irq(NVIC_IRQ_USART1);
        nvic_set_priority(NVIC_IRQ_USART1, NVIC_PRIORITY_HIGH);
        printf("[OK] nvic_init + enable USART1 (pri=HIGH)\n");
        nvic_set_tail_chaining(NVIC_IRQ_USART1, true);
        nvic_set_late_arrival(NVIC_IRQ_USART1, true);
        printf("[OK] tail-chaining + late-arrival enabled\n");
        uint32_t active = nvic_get_active_vector();
        uint32_t pending = nvic_get_pending_vector();
        printf("[OK] active_vector=%u pending_vector=%u\n", active, pending);
    }

    {
        dma_init(DMA1_BASE);
        printf("[OK] dma_init(DMA1)\n");
        dma_config_t dma_cfg = { DMA1_BASE, DMA_STREAM_0, DMA_CHANNEL_0,
            DMA_DIR_MEM_TO_MEM, false, true,
            DMA_DATA_SIZE_WORD, DMA_DATA_SIZE_WORD,
            DMA_BURST_SINGLE, DMA_BURST_SINGLE,
            false, false, DMA_FIFO_THRESHOLD_1_2, false, NULL, 0 };
        dma_configure_stream(DMA1_BASE, &dma_cfg);
        dma_set_source(DMA1_BASE, DMA_STREAM_0, 0x20001000UL);
        dma_set_destination(DMA1_BASE, DMA_STREAM_0, 0x20002000UL);
        dma_set_transfer_count(DMA1_BASE, DMA_STREAM_0, 256);
        dma_enable_interrupt(DMA1_BASE, DMA_STREAM_0,
            DMA_IT_TCIF | DMA_IT_TEIF);
        printf("[OK] DMA stream 0 configured: MEM2MEM 256 words\n");
    }

    {
        dma_sg_descriptor_t *sg = dma_sg_allocate_list(4);
        dma_sg_set_descriptor(&sg[0], 0x20001000UL, 0x20002000UL, &sg[1], 128);
        dma_sg_set_descriptor(&sg[1], 0x20002000UL, 0x20003000UL, &sg[2], 128);
        dma_sg_set_descriptor(&sg[2], 0x20003000UL, 0x20004000UL, &sg[3], 128);
        dma_sg_set_descriptor(&sg[3], 0x20004000UL, 0x20005000UL, NULL, 128);
        dma_sg_configure(DMA1_BASE, DMA_STREAM_0, sg, 4);
        printf("[OK] scatter-gather: 4 descriptors, 128 words each\n");
        dma_sg_free_list(sg);
        printf("[OK] sg list freed\n");
    }

    printf("\n");

    /* ---------------------------------------------------------------
     *  COMPLETION
     * --------------------------------------------------------------- */
    printf("*************************************************************\n");
    printf("*                                                           *\n");
    printf("*   mini-mcu-embedded Full Demonstration Complete!          *\n");
    printf("*   All 5 sub-modules exercised successfully:              *\n");
    printf("*     - Cortex-M4 Core (NVIC, SysTick, SCB, MPU)           *\n");
    printf("*     - GPIO / UART / SPI / I2C                             *\n");
    printf("*     - Timers / PWM / Capture-Compare / Encoder            *\n");
    printf("*     - RTC / WDG / ADC / Clock / Power / CRC               *\n");
    printf("*     - Memory Map / NVIC Handler / DMA / Scatter-Gather    *\n");
    printf("*                                                           *\n");
    printf("*************************************************************\n");
    printf("\n");

    return 0;
}
