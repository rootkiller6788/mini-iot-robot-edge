#include "cortex_m_core.h"
#include "gpio_uart.h"
#include "mcu_peripheral.h"
#include "memory_map.h"
#include "nvic_dma.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_ms(void)
{
    return (double)clock() * 1000.0 / (double)CLOCKS_PER_SEC;
}

int main(int argc, char **argv)
{
    int N = 5000;
    if (argc > 1) N = atoi(argv[1]);
    if (N < 1) N = 5000;

    double t0, elapsed;
    int i;

    printf("\n=== mini-mcu-embedded Benchmarks (N=%d) ===\n\n", N);

    /* ── NVIC Enable/Disable IRQ ────────────────────────── */
    {
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            cortex_m4_nvic_enable_irq((uint8_t)(i % 128));
            cortex_m4_nvic_disable_irq((uint8_t)(i % 128));
        }
        elapsed = now_ms() - t0;
        printf("  nvic_enable/disable_irq:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 2, elapsed, (elapsed / (N * 2)) * 1000.0);
    }

    /* ── NVIC Priority ──────────────────────────────────── */
    {
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            cortex_m4_nvic_set_priority((uint8_t)(i % 64), (uint8_t)(i % 16) << 4);
        }
        elapsed = now_ms() - t0;
        printf("  nvic_set_priority:  %d ops in %.1f ms  (%.1f us/op)\n",
               N, elapsed, (elapsed / N) * 1000.0);
    }

    /* ── SysTick ────────────────────────────────────────── */
    {
        cortex_m4_systick_init(16000);
        t0 = now_ms();
        for (i = 0; i < N * 2; i++) {
            cortex_m4_systick_enable();
            cortex_m4_systick_disable();
        }
        elapsed = now_ms() - t0;
        printf("  systick_enable/disable:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 2, elapsed, (elapsed / (N * 2)) * 1000.0);

        t0 = now_ms();
        for (i = 0; i < N * 3; i++) {
            cortex_m4_systick_has_expired();
            cortex_m4_systick_get_current();
        }
        elapsed = now_ms() - t0;
        printf("  systick_has_expired+current:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 3, elapsed, (elapsed / (N * 3)) * 1000.0);
    }

    /* ── SCB Operations ─────────────────────────────────── */
    {
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            cortex_m4_scb_set_priority_grouping(PRIGROUP_NVIC_4_2);
            cortex_m4_scb_get_priority_grouping();
        }
        elapsed = now_ms() - t0;
        printf("  scb_priority_grouping:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 2, elapsed, (elapsed / (N * 2)) * 1000.0);

        t0 = now_ms();
        for (i = 0; i < N * 3; i++) {
            cortex_m4_scb_get_cpu_id();
            cortex_m4_get_processor_mode();
        }
        elapsed = now_ms() - t0;
        printf("  scb_cpu_id+processor_mode:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 3, elapsed, (elapsed / (N * 3)) * 1000.0);
    }

    /* ── MPU Configuration ──────────────────────────────── */
    {
        t0 = now_ms();
        for (i = 0; i < N / 10; i++) {
            cortex_m4_mpu_configure_region((uint8_t)(i % 8), 0x20000000UL,
                MPU_REGION_SIZE_32K, MPU_ACCESS_FULL, 0, true, true, false, false);
        }
        elapsed = now_ms() - t0;
        printf("  mpu_configure_region:  %d ops in %.1f ms  (%.1f us/op)\n",
               N / 10, elapsed, (elapsed / (N / 10)) * 1000.0);

        t0 = now_ms();
        for (i = 0; i < N * 2; i++) {
            cortex_m4_mpu_enable();
            cortex_m4_mpu_disable();
        }
        elapsed = now_ms() - t0;
        printf("  mpu_enable/disable:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 2, elapsed, (elapsed / (N * 2)) * 1000.0);
    }

    /* ── GPIO Operations ────────────────────────────────── */
    {
        gpio_pin_t pin = {GPIOA_BASE, GPIO_PIN_5};
        gpio_init(pin, GPIO_MODE_OUTPUT, GPIO_OTYPE_PUSH_PULL, GPIO_PUPD_NONE, GPIO_SPEED_HIGH);
        t0 = now_ms();
        for (i = 0; i < N * 3; i++) {
            gpio_write_pin(pin, true);
            gpio_read_pin(pin);
            gpio_write_pin(pin, false);
            gpio_read_pin(pin);
        }
        elapsed = now_ms() - t0;
        printf("  gpio_write/read_pin:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 3, elapsed, (elapsed / (N * 3)) * 1000.0);

        t0 = now_ms();
        for (i = 0; i < N; i++) {
            gpio_set_alt_func(pin, GPIO_AF1);
            gpio_toggle_pin(pin);
        }
        elapsed = now_ms() - t0;
        printf("  gpio_set_alt_func+toggle:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 2, elapsed, (elapsed / (N * 2)) * 1000.0);
    }

    /* ── UART Operations ────────────────────────────────── */
    {
        uart_config_t ucfg;
        memset(&ucfg, 0, sizeof(ucfg));
        ucfg.uart_base = USART1_BASE;
        ucfg.baud_rate = 115200;
        ucfg.word_length = UART_WORD_LENGTH_8;
        ucfg.stop_bits = UART_STOP_BITS_1;
        ucfg.parity = UART_PARITY_NONE;
        ucfg.flow_ctrl = UART_FLOW_CTRL_NONE;
        ucfg.oversampling = UART_OVERSAMPLING_16;
        uart_init(&ucfg);
        uint8_t uart_buf[64];
        memset(uart_buf, 0xAA, sizeof(uart_buf));
        t0 = now_ms();
        for (i = 0; i < N / 2; i++) {
            uart_transmit(USART1_BASE, uart_buf, 16);
        }
        elapsed = now_ms() - t0;
        printf("  uart_transmit:  %d ops in %.1f ms  (%.1f us/op)\n",
               N / 2, elapsed, (elapsed / (N / 2)) * 1000.0);

        t0 = now_ms();
        for (i = 0; i < N * 2; i++) {
            uart_receive(USART1_BASE, uart_buf, 8);
        }
        elapsed = now_ms() - t0;
        printf("  uart_receive:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 2, elapsed, (elapsed / (N * 2)) * 1000.0);

        t0 = now_ms();
        for (i = 0; i < N * 3; i++) {
            uart_send_string(USART1_BASE, "OK");
        }
        elapsed = now_ms() - t0;
        printf("  uart_send_string:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 3, elapsed, (elapsed / (N * 3)) * 1000.0);
        uart_deinit(USART1_BASE);
    }

    /* ── SPI Operations ─────────────────────────────────── */
    {
        spi_config_t scfg;
        memset(&scfg, 0, sizeof(scfg));
        scfg.spi_base = SPI1_BASE;
        scfg.mode = SPI_MODE_MASTER;
        scfg.direction = SPI_DIRECTION_2LINES;
        scfg.data_size = SPI_DATA_SIZE_8BIT;
        scfg.cpol = SPI_CPOL_LOW;
        scfg.cpha = SPI_CPHA_1EDGE;
        scfg.baud_prescaler = SPI_BAUD_PRESCALER_4;
        scfg.first_bit = SPI_FRAME_MSB_FIRST;
        spi_init(&scfg);
        t0 = now_ms();
        for (i = 0; i < N * 2; i++) {
            spi_transmit_receive_8(SPI1_BASE, (uint8_t)(i & 0xFF));
        }
        elapsed = now_ms() - t0;
        printf("  spi_transmit_receive_8:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 2, elapsed, (elapsed / (N * 2)) * 1000.0);
        spi_deinit(SPI1_BASE);
    }

    /* ── I2C Operations ─────────────────────────────────── */
    {
        i2c_config_t icfg;
        memset(&icfg, 0, sizeof(icfg));
        icfg.i2c_base = I2C1_BASE;
        icfg.clock_speed = 100000;
        icfg.duty_cycle = I2C_DUTY_CYCLE_2;
        icfg.own_address = 0x10;
        icfg.addr_mode = I2C_ADDR_MODE_7BIT;
        i2c_init(&icfg);
        uint8_t i2c_buf[16];
        memset(i2c_buf, 0x55, sizeof(i2c_buf));
        t0 = now_ms();
        for (i = 0; i < N / 2; i++) {
            i2c_master_transmit(I2C1_BASE, 0x50, i2c_buf, 8);
        }
        elapsed = now_ms() - t0;
        printf("  i2c_master_transmit:  %d ops in %.1f ms  (%.1f us/op)\n",
               N / 2, elapsed, (elapsed / (N / 2)) * 1000.0);

        t0 = now_ms();
        for (i = 0; i < N; i++) {
            i2c_is_device_ready(I2C1_BASE, 0x50, 3);
        }
        elapsed = now_ms() - t0;
        printf("  i2c_is_device_ready:  %d ops in %.1f ms  (%.1f us/op)\n",
               N, elapsed, (elapsed / N) * 1000.0);
        i2c_deinit(I2C1_BASE);
    }

    /* ── Timer Operations ────────────────────────────────── */
    {
        timer_config_t tcfg;
        memset(&tcfg, 0, sizeof(tcfg));
        tcfg.tim_base = TIM2_BASE;
        tcfg.prescaler = 84;
        tcfg.period = 1000;
        tcfg.counter_mode = TIM_COUNTER_UP;
        timer_init(&tcfg);
        t0 = now_ms();
        for (i = 0; i < N * 2; i++) {
            timer_start(TIM2_BASE);
            timer_get_counter(TIM2_BASE);
            timer_stop(TIM2_BASE);
        }
        elapsed = now_ms() - t0;
        printf("  timer_start/get/stop:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 2, elapsed, (elapsed / (N * 2)) * 1000.0);
        timer_deinit(TIM2_BASE);
    }

    /* ── PWM Operations ─────────────────────────────────── */
    {
        pwm_config_t pcfg;
        memset(&pcfg, 0, sizeof(pcfg));
        pcfg.tim_base = TIM3_BASE;
        pcfg.channel = TIM_CHANNEL_1;
        pcfg.mode = PWM_MODE_FAST;
        pcfg.output_type = PWM_OUTPUT_NORMAL;
        pcfg.duty_cycle = 500;
        pcfg.period = 1000;
        pcfg.output_enable = true;
        pwm_init(&pcfg);
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            pwm_set_duty_cycle(TIM3_BASE, TIM_CHANNEL_1, (uint32_t)(i % 1001));
        }
        elapsed = now_ms() - t0;
        printf("  pwm_set_duty_cycle:  %d ops in %.1f ms  (%.1f us/op)\n",
               N, elapsed, (elapsed / N) * 1000.0);

        t0 = now_ms();
        for (i = 0; i < N * 2; i++) {
            pwm_start(TIM3_BASE, TIM_CHANNEL_1);
            pwm_stop(TIM3_BASE, TIM_CHANNEL_1);
        }
        elapsed = now_ms() - t0;
        printf("  pwm_start/stop:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 2, elapsed, (elapsed / (N * 2)) * 1000.0);
        pwm_disable_output(TIM3_BASE, TIM_CHANNEL_1);
    }

    /* ── Encoder Operations ──────────────────────────────── */
    {
        encoder_config_t ecfg;
        memset(&ecfg, 0, sizeof(ecfg));
        ecfg.tim_base = TIM4_BASE;
        ecfg.mode = TIM_ENCODER_MODE_BOTH;
        ecfg.counts_per_rev = 400;
        ecfg.prescaler = 0;
        encoder_init(&ecfg);
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            encoder_get_count(TIM4_BASE);
        }
        elapsed = now_ms() - t0;
        printf("  encoder_get_count:  %d ops in %.1f ms  (%.1f us/op)\n",
               N, elapsed, (elapsed / N) * 1000.0);
    }

    /* ── RTC Operations ──────────────────────────────────── */
    {
        rtc_init();
        rtc_calendar_t cal;
        memset(&cal, 0, sizeof(cal));
        cal.year = 25; cal.month = 5; cal.day = 22;
        cal.hour = 14; cal.minute = 30; cal.second = 0;
        cal.format = RTC_FORMAT_BIN;
        t0 = now_ms();
        for (i = 0; i < N / 2; i++) {
            rtc_set_calendar(&cal);
            rtc_get_calendar(&cal);
        }
        elapsed = now_ms() - t0;
        printf("  rtc_set/get_calendar:  %d ops in %.1f ms  (%.1f us/op)\n",
               N, elapsed, (elapsed / N) * 1000.0);

        t0 = now_ms();
        for (i = 0; i < N * 2; i++) {
            rtc_get_timestamp();
        }
        elapsed = now_ms() - t0;
        printf("  rtc_get_timestamp:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 2, elapsed, (elapsed / (N * 2)) * 1000.0);
    }

    /* ── ADC Operations ──────────────────────────────────── */
    {
        adc_config_t acfg;
        uint8_t ch_list[2] = {0, 1};
        memset(&acfg, 0, sizeof(acfg));
        acfg.adc_base = ADC1_BASE;
        acfg.resolution = ADC_RESOLUTION_12BIT;
        acfg.mode = ADC_MODE_SINGLE;
        acfg.num_channels = 1;
        acfg.channels = ch_list;
        acfg.sample_time = 15;
        adc_init(&acfg);
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            adc_read(ADC1_BASE, 0);
        }
        elapsed = now_ms() - t0;
        printf("  adc_read:  %d ops in %.1f ms  (%.1f us/op)\n",
               N, elapsed, (elapsed / N) * 1000.0);
        adc_deinit(ADC1_BASE);
    }

    /* ── CRC Operations ──────────────────────────────────── */
    {
        crc_init();
        uint32_t crc_data[64];
        for (i = 0; i < 64; i++) crc_data[i] = (uint32_t)i;
        t0 = now_ms();
        for (i = 0; i < N * 2; i++) {
            crc_calculate(crc_data, 64);
        }
        elapsed = now_ms() - t0;
        printf("  crc_calculate:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 2, elapsed, (elapsed / (N * 2)) * 1000.0);
    }

    /* ── Clock Operations ────────────────────────────────── */
    {
        clock_config_t ccfg;
        memset(&ccfg, 0, sizeof(ccfg));
        ccfg.sysclk_source = CLOCK_SRC_HSI;
        ccfg.hclk_prescaler = 1;
        ccfg.pclk1_prescaler = 2;
        ccfg.pclk2_prescaler = 1;
        clock_init(&ccfg);
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            clock_get_freq(CLOCK_SYS);
            clock_get_freq(CLOCK_HCLK);
        }
        elapsed = now_ms() - t0;
        printf("  clock_get_freq:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 2, elapsed, (elapsed / (N * 2)) * 1000.0);
    }

    /* ── Power Management ────────────────────────────────── */
    {
        t0 = now_ms();
        for (i = 0; i < N * 2; i++) {
            power_set_mode(POWER_MODE_SLEEP);
            power_get_mode();
        }
        elapsed = now_ms() - t0;
        printf("  power_set/get_mode:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 2, elapsed, (elapsed / (N * 2)) * 1000.0);
    }

    /* ── Memory Map Operations ───────────────────────────── */
    {
        memory_map_ctx_t mmc;
        memory_map_init(&mmc);
        memory_map_add_region(&mmc, "FLASH", FLASH_BASE, FLASH_SIZE_DEFAULT,
                              MEM_TYPE_FLASH, MEM_ACCESS_RX, false, false);
        memory_map_add_region(&mmc, "SRAM", SRAM_BASE, SRAM_SIZE_DEFAULT,
                              MEM_TYPE_SRAM, MEM_ACCESS_RW, true, true);
        memory_map_add_region(&mmc, "PERIPH", PERIPH_BASE, AHB1_PERIPH_SIZE,
                              MEM_TYPE_PERIPH, MEM_ACCESS_RW, false, false);
        t0 = now_ms();
        for (i = 0; i < N * 3; i++) {
            memory_map_is_address_valid(&mmc, (uint32_t)(SRAM_BASE + (i & 0xFFF)));
            memory_map_is_in_sram(&mmc, (uint32_t)(SRAM_BASE + (i & 0xFFF)));
        }
        elapsed = now_ms() - t0;
        printf("  memory_map_is_valid+in_sram:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 3, elapsed, (elapsed / (N * 3)) * 1000.0);

        memory_map_configure_section(&mmc, MEM_SECTION_STACK,
            SRAM_BASE + 0x20000, SRAM_BASE + 0x20000, 0x4000, 8, false, true);
        t0 = now_ms();
        for (i = 0; i < N * 2; i++) {
            memory_map_get_free_sram(&mmc);
        }
        elapsed = now_ms() - t0;
        printf("  memory_map_get_free_sram:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 2, elapsed, (elapsed / (N * 2)) * 1000.0);

        /* Bitband */
        t0 = now_ms();
        for (i = 0; i < N * 2; i++) {
            bitband_write(SRAM_BASE, (uint8_t)(i % 8), (bool)(i & 1));
            bitband_read(SRAM_BASE, (uint8_t)(i % 8));
        }
        elapsed = now_ms() - t0;
        printf("  bitband_write+read:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 2, elapsed, (elapsed / (N * 2)) * 1000.0);
    }

    /* ── NVIC Handler Registration ──────────────────────── */
    {
        nvic_init();
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            nvic_enable_irq((uint8_t)(i % 64));
            nvic_set_priority((uint8_t)(i % 64), NVIC_PRIORITY_MEDIUM);
            nvic_set_tail_chaining((uint8_t)(i % 64), true);
        }
        elapsed = now_ms() - t0;
        printf("  nvic_enable+set_priority+tail_chain:  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 3, elapsed, (elapsed / (N * 3)) * 1000.0);

        t0 = now_ms();
        for (i = 0; i < N; i++) {
            nvic_get_active_vector();
        }
        elapsed = now_ms() - t0;
        printf("  nvic_get_active_vector:  %d ops in %.1f ms  (%.1f us/op)\n",
               N, elapsed, (elapsed / N) * 1000.0);
    }

    /* ── DMA Configuration ──────────────────────────────── */
    {
        dma_init(DMA1_BASE);
        dma_config_t dcfg;
        memset(&dcfg, 0, sizeof(dcfg));
        dcfg.dma_base = DMA1_BASE;
        dcfg.stream = DMA_STREAM_0;
        dcfg.channel = DMA_CHANNEL_0;
        dcfg.direction = DMA_DIR_MEM_TO_MEM;
        dcfg.periph_data_size = DMA_DATA_SIZE_WORD;
        dcfg.mem_data_size = DMA_DATA_SIZE_WORD;
        dcfg.periph_burst_size = DMA_BURST_SINGLE;
        dcfg.mem_burst_size = DMA_BURST_SINGLE;
        dcfg.periph_inc = true;
        dcfg.mem_inc = true;
        dma_configure_stream(DMA1_BASE, &dcfg);
        uint32_t src_buf[32], dst_buf[32];
        memset(src_buf, 0xAB, sizeof(src_buf));
        memset(dst_buf, 0, sizeof(dst_buf));
        dma_transfer_t transfer;
        memset(&transfer, 0, sizeof(transfer));
        transfer.stream = DMA_STREAM_0;
        memcpy(&transfer.config, &dcfg, sizeof(dcfg));
        t0 = now_ms();
        for (i = 0; i < N / 5; i++) {
            dma_start_transfer(DMA1_BASE, &transfer);
            dma_wait_for_completion(DMA1_BASE, DMA_STREAM_0, 100);
        }
        elapsed = now_ms() - t0;
        printf("  dma_start_transfer+wait:  %d ops in %.1f ms  (%.1f us/op)\n",
               N / 5, elapsed, (elapsed / (N / 5)) * 1000.0);

        /* Scatter-gather */
        dma_sg_descriptor_t *sg = dma_sg_allocate_list(4);
        for (i = 0; i < 4; i++) {
            dma_sg_set_descriptor(&sg[i], (uint32_t)(src_buf + (size_t)i * 8),
                (uint32_t)(dst_buf + (size_t)i * 8), (i < 3) ? &sg[i + 1] : NULL, 32);
        }
        t0 = now_ms();
        for (i = 0; i < N / 10; i++) {
            dma_sg_configure(DMA1_BASE, DMA_STREAM_1, sg, 4);
        }
        elapsed = now_ms() - t0;
        printf("  dma_sg_configure:  %d ops in %.1f ms  (%.1f us/op)\n",
               N / 10, elapsed, (elapsed / (N / 10)) * 1000.0);
        dma_sg_free_list(sg);
    }

    printf("\n=== Benchmarks Complete ===\n\n");
    return 0;
}
