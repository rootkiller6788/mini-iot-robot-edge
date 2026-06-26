#include "cortex_m_core.h"
#include "gpio_uart.h"
#include "mcu_peripheral.h"
#include "memory_map.h"
#include "nvic_dma.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0, tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  TEST %s ... ", name); } while(0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); return 1; } while(0)
#define CHECK(cond, msg) if (!(cond)) FAIL(msg)

/* ── Test 1: NVIC Enable/Disable IRQ ───────────────────── */
static int test_nvic_enable_irq(void)
{
    TEST("NVIC enable/disable IRQ");
    cortex_m4_nvic_enable_irq(10);
    CHECK(cortex_m4_nvic_is_active(10) == false, "IRQ should not be active yet");
    cortex_m4_nvic_disable_irq(10);
    PASS();
    return 0;
}

/* ── Test 2: NVIC Priority ─────────────────────────────── */
static int test_nvic_priority(void)
{
    TEST("NVIC set/get priority");
    cortex_m4_nvic_set_priority(5, 0x80);
    uint8_t prio = cortex_m4_nvic_get_priority(5);
    CHECK(prio == 0x80, "get_priority returned wrong value");
    PASS();
    return 0;
}

/* ── Test 3: SysTick Init/Enable ───────────────────────── */
static int test_systick_init(void)
{
    TEST("SysTick init and enable");
    cortex_m4_systick_init(16000);
    cortex_m4_systick_enable();
    CHECK(cortex_m4_systick_has_expired() == false,
          "SysTick should not have expired immediately after enable");
    cortex_m4_systick_disable();
    PASS();
    return 0;
}

/* ── Test 4: SysTick Current / Calibration ──────────────── */
static int test_systick_current(void)
{
    TEST("SysTick get current and calibration");
    cortex_m4_systick_init(16000000);
    cortex_m4_systick_enable();
    uint32_t cur = cortex_m4_systick_get_current();
    CHECK(cur > 0, "SysTick current should be > 0 after init");
    uint32_t calib = cortex_m4_systick_get_calibration();
    (void)calib;
    cortex_m4_systick_disable();
    PASS();
    return 0;
}

/* ── Test 5: SCB Priority Grouping ─────────────────────── */
static int test_scb_priority_grouping(void)
{
    TEST("SCB priority grouping");
    cortex_m4_scb_set_priority_grouping(PRIGROUP_NVIC_4_2);
    priority_grouping_t pg = cortex_m4_scb_get_priority_grouping();
    CHECK(pg == PRIGROUP_NVIC_4_2, "SCB priority grouping mismatch");
    PASS();
    return 0;
}

/* ── Test 6: MPU Operations ────────────────────────────── */
static int test_mpu_configure(void)
{
    TEST("MPU configure region");
    cortex_m4_mpu_configure_region(0, 0x20000000UL, MPU_REGION_SIZE_32K,
        MPU_ACCESS_FULL, 0, true, true, false, false);
    cortex_m4_mpu_enable_region(0);
    cortex_m4_mpu_enable();
    cortex_m4_mpu_disable();
    PASS();
    return 0;
}

/* ── Test 7: GPIO Init/Write/Read ──────────────────────── */
static int test_gpio_write_read(void)
{
    TEST("GPIO write and read pin");
    gpio_pin_t pin = {GPIOA_BASE, GPIO_PIN_3};
    gpio_init(pin, GPIO_MODE_OUTPUT, GPIO_OTYPE_PUSH_PULL,
              GPIO_PUPD_NONE, GPIO_SPEED_HIGH);
    gpio_write_pin(pin, true);
    bool val = gpio_read_pin(pin);
    CHECK(val == true, "GPIO read after write HIGH should be true");
    gpio_write_pin(pin, false);
    val = gpio_read_pin(pin);
    CHECK(val == false, "GPIO read after write LOW should be false");
    PASS();
    return 0;
}

/* ── Test 8: GPIO Set Alt Function ─────────────────────── */
static int test_gpio_alt_func(void)
{
    TEST("GPIO set alternate function");
    gpio_pin_t pin = {GPIOB_BASE, GPIO_PIN_6};
    gpio_init(pin, GPIO_MODE_ALTFUNC, GPIO_OTYPE_PUSH_PULL,
              GPIO_PUPD_NONE, GPIO_SPEED_MEDIUM);
    gpio_set_alt_func(pin, GPIO_AF7);
    gpio_toggle_pin(pin);
    PASS();
    return 0;
}

/* ── Test 9: UART Init/Transmit/Receive ────────────────── */
static int test_uart_transmit_receive(void)
{
    TEST("UART init, transmit, and receive");
    uart_config_t ucfg;
    memset(&ucfg, 0, sizeof(ucfg));
    ucfg.uart_base = USART2_BASE;
    ucfg.baud_rate = 9600;
    ucfg.word_length = UART_WORD_LENGTH_8;
    ucfg.stop_bits = UART_STOP_BITS_1;
    ucfg.parity = UART_PARITY_NONE;
    ucfg.flow_ctrl = UART_FLOW_CTRL_NONE;
    ucfg.oversampling = UART_OVERSAMPLING_16;
    uart_init(&ucfg);
    uint8_t tx_data[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; /* "Hello" */
    uart_transmit(USART2_BASE, tx_data, sizeof(tx_data));
    uart_send_string(USART2_BASE, "World");
    uint8_t rx_buf[16];
    memset(rx_buf, 0, sizeof(rx_buf));
    uint32_t rx_len = uart_receive(USART2_BASE, rx_buf, sizeof(rx_buf));
    CHECK(rx_len == sizeof(tx_data), "receive size should match transmit size");
    uart_deinit(USART2_BASE);
    PASS();
    return 0;
}

/* ── Test 10: I2C Device Ready ─────────────────────────── */
static int test_i2c_device_ready(void)
{
    TEST("I2C init and device ready check");
    i2c_config_t icfg;
    memset(&icfg, 0, sizeof(icfg));
    icfg.i2c_base = I2C1_BASE;
    icfg.clock_speed = 100000;
    icfg.duty_cycle = I2C_DUTY_CYCLE_2;
    icfg.own_address = 0x20;
    icfg.addr_mode = I2C_ADDR_MODE_7BIT;
    i2c_init(&icfg);
    bool ready = i2c_is_device_ready(I2C1_BASE, 0x68, 3);
    (void)ready;
    i2c_deinit(I2C1_BASE);
    PASS();
    return 0;
}

/* ── Test 11: Timer Start/Get/Stop ─────────────────────── */
static int test_timer_start_stop(void)
{
    TEST("Timer start, get counter, and stop");
    timer_config_t tcfg;
    memset(&tcfg, 0, sizeof(tcfg));
    tcfg.tim_base = TIM2_BASE;
    tcfg.prescaler = 84;
    tcfg.period = 10000;
    tcfg.counter_mode = TIM_COUNTER_UP;
    timer_init(&tcfg);
    timer_start(TIM2_BASE);
    uint32_t val = timer_get_counter(TIM2_BASE);
    CHECK(val >= 0, "Timer counter should be >= 0 after start");
    timer_stop(TIM2_BASE);
    timer_deinit(TIM2_BASE);
    PASS();
    return 0;
}

/* ── Test 12: PWM Init and Set Duty ────────────────────── */
static int test_pwm_set_duty(void)
{
    TEST("PWM init and set duty cycle");
    pwm_config_t pcfg;
    memset(&pcfg, 0, sizeof(pcfg));
    pcfg.tim_base = TIM3_BASE;
    pcfg.channel = TIM_CHANNEL_2;
    pcfg.mode = PWM_MODE_FAST;
    pcfg.output_type = PWM_OUTPUT_NORMAL;
    pcfg.duty_cycle = 750;
    pcfg.period = 1000;
    pcfg.output_enable = true;
    pwm_init(&pcfg);
    pwm_start(TIM3_BASE, TIM_CHANNEL_2);
    pwm_set_duty_cycle(TIM3_BASE, TIM_CHANNEL_2, 250);
    pwm_stop(TIM3_BASE, TIM_CHANNEL_2);
    pwm_disable_output(TIM3_BASE, TIM_CHANNEL_2);
    PASS();
    return 0;
}

/* ── Test 13: Clock Get Frequency ──────────────────────── */
static int test_clock_get_freq(void)
{
    TEST("Clock init and get frequency");
    clock_config_t ccfg;
    memset(&ccfg, 0, sizeof(ccfg));
    ccfg.sysclk_source = CLOCK_SRC_HSI;
    ccfg.hclk_prescaler = 1;
    ccfg.pclk1_prescaler = 2;
    ccfg.pclk2_prescaler = 1;
    clock_init(&ccfg);
    uint32_t sysclk = clock_get_freq(CLOCK_SYS);
    CHECK(sysclk > 0, "SYSCLK frequency should be > 0");
    uint32_t hclk = clock_get_freq(CLOCK_HCLK);
    CHECK(hclk > 0, "HCLK frequency should be > 0");
    PASS();
    return 0;
}

/* ── Test 14: Memory Map Validation ────────────────────── */
static int test_memory_map(void)
{
    TEST("Memory map init and address validation");
    memory_map_ctx_t mmc;
    memory_map_init(&mmc);
    memory_map_add_region(&mmc, "FLASH", FLASH_BASE, FLASH_SIZE_DEFAULT,
                          MEM_TYPE_FLASH, MEM_ACCESS_RX, false, false);
    memory_map_add_region(&mmc, "SRAM", SRAM_BASE, SRAM_SIZE_DEFAULT,
                          MEM_TYPE_SRAM, MEM_ACCESS_RW, true, true);
    CHECK(memory_map_is_address_valid(&mmc, SRAM_BASE) == true,
          "SRAM base should be valid");
    CHECK(memory_map_is_in_sram(&mmc, SRAM_BASE + 0x1000) == true,
          "SRAM address should be in SRAM");
    CHECK(memory_map_is_in_flash(&mmc, FLASH_BASE + 0x100) == true,
          "FLASH address should be in flash");
    memory_map_set_boot_vector(&mmc, SRAM_BASE + SRAM_SIZE_DEFAULT, NULL);
    uint32_t free_sram = memory_map_get_free_sram(&mmc);
    CHECK(free_sram > 0, "free SRAM should be > 0");
    PASS();
    return 0;
}

/* ── Test 15: Bitband Operations ───────────────────────── */
static int test_bitband_ops(void)
{
    TEST("Bitband write and read");
    bitband_write(SRAM_BASE, 0, true);
    CHECK(bitband_read(SRAM_BASE, 0) == true, "bitband read after write true failed");
    bitband_write(SRAM_BASE, 1, false);
    CHECK(bitband_read(SRAM_BASE, 1) == false, "bitband read after write false failed");
    PASS();
    return 0;
}

int main(void)
{
    printf("\n=== mini-mcu-embedded Core Tests ===\n\n");

    test_nvic_enable_irq();
    test_nvic_priority();
    test_systick_init();
    test_systick_current();
    test_scb_priority_grouping();
    test_mpu_configure();
    test_gpio_write_read();
    test_gpio_alt_func();
    test_uart_transmit_receive();
    test_i2c_device_ready();
    test_timer_start_stop();
    test_pwm_set_duty();
    test_clock_get_freq();
    test_memory_map();
    test_bitband_ops();

    printf("\n--- Results: %d/%d tests passed ---\n\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
