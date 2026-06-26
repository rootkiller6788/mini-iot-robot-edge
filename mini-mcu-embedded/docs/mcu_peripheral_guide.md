# MCU Peripheral Programming Guide

> mini-mcu-embedded 外设编程指南 — 从入门到深入理解各个外设模块的 API 使用

---

## 目录

1. [快速入门](#快速入门)
2. [GPIO 编程](#gpio-编程)
3. [UART 编程](#uart-编程)
4. [SPI 编程](#spi-编程)
5. [I2C 编程](#i2c-编程)
6. [Timer 编程](#timer-编程)
7. [PWM 编程](#pwm-编程)
8. [ADC 编程](#adc-编程)
9. [DMA 编程](#dma-编程)
10. [RTC 编程](#rtc-编程)
11. [Watchdog 编程](#watchdog-编程)
12. [CRC 编程](#crc-编程)
13. [Power Management](#power-management)
14. [Clock Configuration](#clock-configuration)
15. [Memory Map & Bit-Band](#memory-map--bit-band)
16. [NVIC & Interrupts](#nvic--interrupts)
17. [故障排除速查表](#故障排除速查表)

---

## 快速入门

### 最小工程框架

```c
#include "mcu_peripheral.h"
#include "gpio_uart.h"
#include "cortex_m_core.h"
#include "nvic_dma.h"
#include "memory_map.h"

int main(void) {
    nvic_init();
    dma_init(DMA1_BASE);
    dma_init(DMA2_BASE);

    clock_config_t clk = {
        .sysclk_source = CLOCK_SRC_PLL,
        .hclk_prescaler = 1, .pclk1_prescaler = 4, .pclk2_prescaler = 2,
        .hse_enabled = true, .hse_freq_hz = 8000000UL,
        .pll_enabled = true, .pll_source = CLOCK_PLL_SRC_HSE,
        .pll_m = 8, .pll_n = 336, .pll_p = 2, .pll_q = 7,
    };
    clock_init(&clk);

    while (1) {
        cortex_m4_wait_for_interrupt();
    }
    return 0;
}
```

---

## GPIO 编程

### 引脚配置范式

```c
// 1. 定义引脚
gpio_pin_t pin = { GPIOA_BASE, GPIO_PIN_5 };

// 2. 初始化
gpio_init(pin, GPIO_MODE_OUTPUT, GPIO_OTYPE_PUSH_PULL,
          GPIO_PUPD_NONE, GPIO_SPEED_HIGH);

// 3. 操作
gpio_write_pin(pin, true);    // 置高
gpio_write_pin(pin, false);   // 置低
gpio_toggle_pin(pin);          // 翻转
bool state = gpio_read_pin(pin); // 读取

// 4. 配置复用功能
gpio_set_alt_func(pin, GPIO_AF7); // PA5 → AF7 (如 USART1_TX)
```

### 模式选择指南

| 用途               | mode               | otype            | pull          | speed           |
|-------------------|--------------------|------------------|---------------|-----------------|
| LED 输出           | GPIO_MODE_OUTPUT   | PUSH_PULL        | NONE          | HIGH            |
| 按键输入           | GPIO_MODE_INPUT    | 任意              | UP/DOWN       | LOW             |
| UART TX           | GPIO_MODE_ALTFUNC  | PUSH_PULL        | UP            | HIGH            |
| I2C SDA/SCL       | GPIO_MODE_ALTFUNC  | OPEN_DRAIN       | UP            | HIGH            |
| ADC 输入           | GPIO_MODE_ANALOG   | 任意              | NONE          | 任意             |
| SPI SCK/MOSI/MISO | GPIO_MODE_ALTFUNC  | PUSH_PULL        | NONE          | VERY_HIGH       |

### 端口级操作

```c
gpio_write_port(GPIOA_BASE, 0x00FF);   // 写 PA0-PA7 为 1
uint16_t val = gpio_read_port(GPIOB_BASE); // 读整个端口
```

---

## UART 编程

### 初始化模板

```c
uart_config_t uart = {
    .uart_base    = USART1_BASE,
    .baud_rate    = 115200,
    .word_length  = UART_WORD_LENGTH_8,
    .stop_bits    = UART_STOP_BITS_1,
    .parity       = UART_PARITY_NONE,
    .flow_ctrl    = UART_FLOW_CTRL_NONE,
    .oversampling = UART_OVERSAMPLING_16,
};
uart_init(&uart);
```

### 常用发送模式

```c
uart_transmit_byte(USART1_BASE, 0x55);                  // 单字节
uart_transmit(USART1_BASE, buf, len);                   // 缓冲
uart_send_string(USART1_BASE, "Hello\r\n");             // 字符串
```

### 常用接收模式

```c
uint8_t byte = uart_receive_byte(USART1_BASE);          // 单字节 (阻塞)
uint32_t n = uart_receive(USART1_BASE, buf, 128);       // 非阻塞读取
uint32_t len = uart_receive_line(USART1_BASE, buf, 256); // 行读取
```

### 中断配置

```c
uart_enable_interrupt(USART1_BASE, UART_CR1_RXNEIE);    // 接收中断
uart_enable_interrupt(USART1_BASE, UART_CR1_TXEIE);     // 发送空中断
uart_enable_interrupt(USART1_BASE, UART_CR1_TCIE);      // 发送完成中断
```

### 状态检测

```c
uint32_t status = uart_get_status(USART1_BASE);
if (status & UART_SR_RXNE) { /* 接收缓冲区非空 */ }
if (status & UART_SR_TXE)  { /* 发送缓冲区空 */ }
if (status & UART_SR_ORE)  { /* 溢出错误 */ }
if (status & UART_SR_FE)   { /* 帧错误 */ }
if (status & UART_SR_PE)   { /* 奇偶校验错误 */ }
```

---

## SPI 编程

### 初始化模板

```c
spi_config_t spi = {
    .spi_base       = SPI1_BASE,
    .mode           = SPI_MODE_MASTER,
    .direction      = SPI_DIRECTION_2LINES,
    .data_size      = SPI_DATA_SIZE_8BIT,
    .cpol           = SPI_CPOL_LOW,       // Mode 0
    .cpha           = SPI_CPHA_1EDGE,
    .baud_prescaler = SPI_BAUD_PRESCALER_8,
    .first_bit      = SPI_FRAME_MSB_FIRST,
};
spi_init(&spi);
```

### SPI 模式速查

| Mode | CPOL | CPHA | 空闲 CK | 采样沿  |
|------|------|------|---------|--------|
| 0    | 0    | 0    | Low     | 第 1 沿  |
| 1    | 0    | 1    | Low     | 第 2 沿  |
| 2    | 1    | 0    | High    | 第 1 沿  |
| 3    | 1    | 1    | High    | 第 2 沿  |

### 数据传输

```c
uint8_t  rx8  = spi_transmit_receive_8(SPI1_BASE, 0x9F);
uint16_t rx16 = spi_transmit_receive_16(SPI1_BASE, 0x1234);
spi_transmit_8(SPI1_BASE, 0x06);     // 只发不收
uint8_t d = spi_receive_8(SPI1_BASE); // 只收不发 (发 0xFF)
```

---

## I2C 编程

### 初始化模板

```c
i2c_config_t i2c = {
    .i2c_base     = I2C1_BASE,
    .clock_speed  = 100000,           // 标准 100kHz
    .duty_cycle   = I2C_DUTY_CYCLE_2,
    .own_address  = 0x30,
    .addr_mode    = I2C_ADDR_MODE_7BIT,
};
i2c_init(&i2c);
```

### 高速模式 (400kHz)

```c
i2c.clock_speed = 400000;
i2c.duty_cycle  = I2C_DUTY_CYCLE_16_9;  // 快速模式用 16:9
```

### 基本读写

```c
uint8_t tx[] = { 0x12, 0x34 };
i2c_master_transmit(I2C1_BASE, 0x50, tx, 2);

uint8_t rx[4];
i2c_master_receive(I2C1_BASE, 0x50, rx, 4);
```

### 寄存器读写 (组合操作)

```c
uint8_t reg = 0x00;
uint8_t data[8];
i2c_master_write_read(I2C1_BASE, 0x50,  // dev addr
    reg, NULL, 0,                        // write reg addr only
    data, sizeof(data));                 // read data
```

---

## Timer 编程

### 基本定时器

```c
timer_config_t tim = {
    .tim_base       = TIM2_BASE,
    .prescaler      = 8400 - 1,     // 84MHz / 8400 = 10kHz
    .period         = 10000 - 1,     // 10kHz / 10000 = 1Hz
    .clock_division = 0,
    .counter_mode   = TIM_COUNTER_UP,
    .auto_reload_preload = true,
};
timer_init(&tim);
timer_start(TIM2_BASE);

// 等待溢出
while (!timer_is_update_pending(TIM2_BASE)) { }
timer_clear_update_flag(TIM2_BASE);
```

### 中断模式

```c
timer_enable_interrupt(TIM2_BASE, (1UL << 0));  // UIE
nvic_enable_irq(NVIC_IRQ_TIM2);

void TIM2_IRQHandler(void) {
    if (timer_is_update_pending(TIM2_BASE)) {
        timer_clear_update_flag(TIM2_BASE);
        // 处理定时事件
    }
}
```

### 定时器频率计算公式

```
Timer Clock = APB Clock / (Prescaler + 1)
Timer Freq  = Timer Clock / (Period + 1)

示例:
APB1 = 42MHz, TIM2 on APB1
若 APB1 prescaler ≠ 1: Timer Clock = 42MHz × 2 = 84MHz
PSC = 8399 → Timer Clock = 84MHz / 8400 = 10kHz
ARR = 9999 → Timer Freq  = 10kHz / 10000 = 1Hz
```

---

## PWM 编程

### 单通道 PWM

```c
pwm_config_t pwm = {
    .tim_base      = TIM3_BASE,
    .channel       = TIM_CHANNEL_1,
    .mode          = PWM_MODE_FAST,
    .output_type   = PWM_OUTPUT_NORMAL,
    .duty_cycle    = 500,          // 50%
    .period        = 1000 - 1,
    .output_enable = true,
};
pwm_init(&pwm);
pwm_start(TIM3_BASE, TIM_CHANNEL_1);
```

### 动态调整

```c
pwm_set_duty_cycle(TIM3_BASE, TIM_CHANNEL_1, 750);  // 75%
pwm_set_period(TIM3_BASE, 2000 - 1);                 // 新频率
pwm_set_polarity(TIM3_BASE, TIM_CHANNEL_1, PWM_OUTPUT_INVERTED);
```

### PWM 模式说明

| 模式                 | 描述                             | 适用场景         |
|---------------------|---------------------------------|-----------------|
| PWM_MODE_FAST       | 快速 PWM, 单边对齐               | LED 控制, 电源   |
| PWM_MODE_PHASE_CORRECT | 相位修正 PWM, 双边对齐         | 电机控制 (高精度) |
| PWM_MODE_CENTER_ALIGNED1 | 中心对齐模式 1               | 电机控制 (低 EMI) |
| PWM_MODE_CENTER_ALIGNED2 | 中心对齐模式 2               | 电机控制         |
| PWM_MODE_CENTER_ALIGNED3 | 中心对齐模式 3               | 电机控制         |

---

## ADC 编程

### 初始化

```c
uint8_t channels[] = { 0, 1, 2, ADC_CHANNEL_TEMP };
adc_config_t adc = {
    .adc_base    = ADC1_BASE,
    .resolution  = ADC_RESOLUTION_12BIT,
    .mode        = ADC_MODE_SINGLE,
    .num_channels = 4,
    .channels     = channels,
    .sample_time = 3,
};
adc_init(&adc);
adc_enable_temperature_sensor();

adc_start_conversion(ADC1_BASE);
uint32_t val0 = adc_read(ADC1_BASE, 0);
float temp = adc_get_temperature(ADC1_BASE);
```

### 多通道读取

```c
uint8_t ch[] = { 0, 1, 2 };
uint32_t results[3];
adc_read_multi(ADC1_BASE, ch, results, 3);
```

---

## DMA 编程

### Mem-to-Mem 传输

```c
dma_config_t dma = {
    .dma_base    = DMA1_BASE,
    .stream      = DMA_STREAM_0,
    .channel     = DMA_CHANNEL_0,
    .direction   = DMA_DIR_MEM_TO_MEM,
    .periph_inc  = true,
    .mem_inc     = true,
    .periph_data_size = DMA_DATA_SIZE_WORD,
    .mem_data_size    = DMA_DATA_SIZE_WORD,
};
dma_configure_stream(DMA1_BASE, &dma);
dma_set_source(DMA1_BASE, 0, src_addr);
dma_set_destination(DMA1_BASE, 0, dst_addr);
dma_set_transfer_count(DMA1_BASE, 0, count);
dma_enable_stream(DMA1_BASE, 0);
```

### Scatter-Gather

```c
dma_sg_descriptor_t *sg = dma_sg_allocate_list(3);
dma_sg_set_descriptor(&sg[0], src1, dst1, &sg[1], 64);
dma_sg_set_descriptor(&sg[1], src2, dst2, &sg[2], 128);
dma_sg_set_descriptor(&sg[2], src3, dst3, NULL, 256);
dma_sg_start(DMA1_BASE, DMA_STREAM_0);
dma_sg_free_list(sg);
```

---

## RTC 编程

```c
rtc_init();

rtc_calendar_t cal = {
    .year = 25, .month = 5, .day = 20,
    .weekday = 2, .hour = 14, .minute = 30, .second = 0,
    .format = RTC_FORMAT_BIN,
};
rtc_set_calendar(&cal);

rtc_calendar_t now;
rtc_get_calendar(&now);

rtc_alarm_config_t alarm = {
    .alarm = RTC_ALARM_A,
    .hour = 15, .minute = 0, .second = 0,
};
rtc_set_alarm(&alarm);
rtc_enable_alarm(RTC_ALARM_A);

uint32_t ts = rtc_get_timestamp();
rtc_set_timestamp(1700000000);
```

---

## Watchdog 编程

### 独立看门狗 (IWDG)

```c
iwdg_init(4, 0xFFF);   // prescaler=64, reload=max
iwdg_enable();

while (1) {
    do_work();
    iwdg_refresh();     // 喂狗
}
```

### 窗口看门狗 (WWDG)

```c
wwdg_init(3, 0x50, 0x7F);  // prescaler=8, window=0x50, counter=0x7F
wwdg_enable_early_wakeup();

while (1) {
    wwdg_refresh();     // 必须在窗口内喂狗 (counter < window)
}
```

---

## CRC 编程

```c
crc_init();

uint32_t data[] = { 0x12345678, 0x9ABCDEF0 };
uint32_t crc32 = crc_calculate(data, 2);

uint8_t bytes[] = "Hello";
uint32_t crc_byte = crc_calculate_byte(bytes, 5);

crc_set_polynomial(0x1EDC6F41);  // CRC-32C
crc_reset();
```

---

## Power Management

```c
power_set_mode(POWER_MODE_SLEEP);
power_enter_sleep();    // WFI + SLEEPDEEP=0

power_set_regulator(POWER_REGULATOR_LP);

power_enter_standby();
if (power_is_standby_reset()) {
    power_clear_standby_reset();
}
```

---

## Clock Configuration

### PLL 配置速查 (HSE 8MHz)

| SYSCLK | M | N   | P | Q   | HCLK | PCLK1 | PCLK2 |
|--------|---|-----|---|-----|------|-------|-------|
| 168MHz | 8 | 336 | 2 | 7   | 168  | 42    | 84    |
| 120MHz | 8 | 240 | 2 | 5   | 120  | 30    | 60    |
| 72MHz  | 8 | 144 | 2 | 3   | 72   | 36    | 72    |
| 48MHz  | 8 | 96  | 2 | 2   | 48   | 24    | 48    |

### 外设时钟使能

```c
clock_enable_peripheral(GPIOA_BASE);   // GPIO 时钟
clock_enable_peripheral(USART1_BASE);  // UART 时钟
clock_enable_peripheral(SPI1_BASE);    // SPI 时钟
clock_enable_peripheral(I2C1_BASE);    // I2C 时钟
clock_enable_peripheral(TIM1_BASE);    // TIM 时钟
clock_enable_peripheral(ADC1_BASE);    // ADC 时钟
clock_enable_peripheral(DMA1_BASE);    // DMA 时钟
clock_enable_peripheral(CRC_BASE);     // CRC 时钟

clock_disable_peripheral(USART1_BASE); // 关闭以省电
```

---

## Memory Map & Bit-Band

### 地址区域判定

```c
memory_map_ctx_t mm;
memory_map_init(&mm);

bool flash_ok  = memory_map_is_in_flash(&mm, 0x08001000);
bool sram_ok   = memory_map_is_in_sram(&mm, 0x20001000);
bool periph_ok = memory_map_is_in_peripheral(&mm, 0x40020000);
bool valid     = memory_map_is_address_valid(&mm, addr);

memory_map_print_layout(&mm);
```

### 位带操作

```c
volatile uint32_t *reg = (volatile uint32_t *)0x40020014; // GPIOA_ODR

bitband_write(0x40020014, 5, true);   // 原子置位 PA5
bitband_write(0x40020014, 5, false);  // 原子清零 PA5
bool bit = bitband_read(0x40020014, 5); // 原子读取 PA5

// 等效于:
// *reg |=  (1 << 5);  ← 非原子 (Read-Modify-Write)
// *reg &= ~(1 << 5);  ← 非原子 (Read-Modify-Write)
```

---

## NVIC & Interrupts

### 注册中断处理器

```c
nvic_init();

void my_handler(void) {
    // 中断处理
}

nvic_register_handler(NVIC_IRQ_USART1, my_handler, NVIC_PRIORITY_MEDIUM);
// 等效于:
nvic_set_priority(NVIC_IRQ_USART1, NVIC_PRIORITY_MEDIUM);
nvic_enable_irq(NVIC_IRQ_USART1);
```

### 中断控制

```c
nvic_set_pending(NVIC_IRQ_TIM2);      // 软件触发中断
nvic_clear_pending(NVIC_IRQ_TIM2);    // 清除 pending
bool is_active = nvic_get_active(NVIC_IRQ_TIM2);
bool is_pending = nvic_get_pending(NVIC_IRQ_TIM2);
nvic_disable_irq(NVIC_IRQ_TIM2);
nvic_unregister_handler(NVIC_IRQ_TIM2);
```

### 全局中断控制

```c
cortex_m4_disable_interrupts();  // CPSID I
cortex_m4_enable_interrupts();   // CPSIE I
```

---

## 故障排除速查表

| 症状                          | 可能原因                        | 检查项                              |
|------------------------------|--------------------------------|------------------------------------|
| GPIO 输出无变化               | 外设时钟未使能                   | `clock_enable_peripheral(GPIOx_BASE)` |
| UART 无输出                   | AF 设置错误 / 波特率不匹配        | 引脚 AF 映射, 时钟频率验证             |
| UART 接收乱码                 | 波特率偏差大 / 过采样设置错误      | 使用 16x 过采样, 检查 PCLK 时钟         |
| SPI 无时钟                    | SPI 未使能 / 引脚未配置成 AF       | Check `spi_init` + `gpio_set_alt_func` |
| SPI 返回全 0xFF               | MISO 未连接 / 芯片未选中(CS高)     | 检查 NSS 和 MISO 硬件连接               |
| I2C 一直 NACK                 | 设备地址错误 / 上拉电阻缺失         | 逻辑分析仪检查波形, 确认 4.7kΩ 上拉      |
| I2C 时序异常                  | 时钟速度设置错误 / TRISE 不匹配     | 验证 PCLK1 和 `i2c_init` 参数            |
| Timer 频率不对                | 时钟源选择错误 / PSC 计算错误       | 验证 APB 时钟 x2 规则 (若 prescaler ≠ 1) |
| PWM 无输出                    | 通道输出未使能 / 极性配置冲突       | `pwm_enable_output()` + `pwm_start()`   |
| ADC 读数为 0                  | ADC 时钟未使能 / 通道未选中         | `clock_enable_peripheral(ADCx_BASE)`    |
| 温度传感器读数不准             | 未校准 / 参考电压不对              | 使用 VREFINT 校准, 检查 `v_sense` 阈值    |
| DMA 不启动                    | 传输方向/通道不匹配 / 流未使能      | 检查 CHSEL 映射表和 DMA 使能位           |
| Hard Fault                    | 非法内存访问 / 未对齐访问           | 检查 MPU 配置, 地址对齐, 栈溢出           |
| 看门狗复位                    | 喂狗间隔超过超时 / 喂狗过早(WWDG)  | 确保 `iwdg_refresh` 周期 < reload        |
| PLL 不上锁                    | 晶振频率不匹配 / PLL 参数超限       | 验证 HSE 起振, PLL VCO ∈ [100,432] MHz  |

---

> **mini-mcu-embedded** — MCU 嵌入式开发框架 (C 语言实现)
