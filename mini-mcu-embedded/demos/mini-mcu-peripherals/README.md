# mini-mcu-peripherals — MCU 外设编程示例

> 综合外设示例：UART 收发、SPI 传感器读取、I2C EEPROM 读写、定时器 PWM 输出。

---

## 概述

本示例演示 `mini-mcu-embedded` 库中核心通信外设和定时器的使用，
涵盖典型的嵌入式外设编程范式：初始化、数据传输、DMA 配合、中断处理。

## 硬件需求

| 组件        | 说明                                      |
|-------------|------------------------------------------|
| MCU         | STM32F407VG (Cortex-M4, 168MHz)           |
| UART-USB    | CP2102 / FT232 (连接 USART1 PA9/PA10)      |
| SPI Flash   | W25Q64 (连接 SPI1 PB3-PB6)                 |
| I2C EEPROM  | AT24C02 (连接 I2C1 PB6-PB7, 地址 0x50)     |
| LED         | PA5 (PWM 控制亮度)                         |
| 逻辑分析仪   | 可选，用于抓取 SPI/I2C 波形                |

## 系统框图

```
┌─────────────────────────────────────────────────────────┐
│                    STM32F407VG                            │
│                                                           │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐              │
│  │  USART1  │   │   SPI1   │   │   I2C1   │              │
│  │ PA9/TX   │   │ PB3/SCK  │   │ PB6/SCL  │              │
│  │ PA10/RX  │   │ PB4/MISO │   │ PB7/SDA  │              │
│  └────┬─────┘   │ PB5/MOSI │   └────┬─────┘              │
│       │         │ PB6/NSS  │        │                     │
│       │         └────┬─────┘        │                     │
│  ┌────┴─────┐        │         ┌────┴─────┐              │
│  │  USB-UART│   ┌────┴─────┐  │  24C02   │              │
│  │ (CP2102) │   │  W25Q64  │  │  EEPROM  │              │
│  └──────────┘   └──────────┘  └──────────┘              │
│                                                           │
│  ┌──────────┐   ┌──────────────────────────┐             │
│  │ TIM1 CH1 │   │         DMA              │             │
│  │ PA5/PWM  │   │  Stream 0: USART1 TX     │             │
│  │  ── LED  │   │  Stream 1: SPI1 RX       │             │
│  └──────────┘   │  Stream 2: I2C1 transfer  │             │
│                  └──────────────────────────┘             │
└─────────────────────────────────────────────────────────┘
```

---

## 初始化流程

### 1. 系统时钟配置

```c
// HSE 8MHz → PLL → SYSCLK 168MHz
clock_config_t clk = {
    .sysclk_source = CLOCK_SRC_PLL,
    .hclk_prescaler = 1,        // HCLK  = 168 MHz
    .pclk1_prescaler = 4,       // APB1  =  42 MHz
    .pclk2_prescaler = 2,       // APB2  =  84 MHz
    .hse_enabled = true,
    .hse_freq_hz = 8000000UL,
    .pll_enabled = true,
    .pll_source = CLOCK_PLL_SRC_HSE,
    .pll_m = 8, .pll_n = 336, .pll_p = 2, .pll_q = 7,
};
clock_init(&clk);
nvic_init();
dma_init(DMA1_BASE);
dma_init(DMA2_BASE);
```

### 2. 外设时钟使能

```c
clock_enable_peripheral(GPIOA_BASE);    // UART1 pins
clock_enable_peripheral(GPIOB_BASE);    // SPI1/I2C1 pins
clock_enable_peripheral(USART1_BASE);   // UART peripheral
clock_enable_peripheral(SPI1_BASE);     // SPI peripheral
clock_enable_peripheral(I2C1_BASE);     // I2C peripheral
clock_enable_peripheral(TIM1_BASE);     // Timer for PWM
```

---

## UART 收发示例

### 引脚配置

```c
// PA9 = USART1_TX (AF7), PA10 = USART1_RX (AF7)
gpio_pin_t tx_pin = { GPIOA_BASE, GPIO_PIN_9  };
gpio_pin_t rx_pin = { GPIOA_BASE, GPIO_PIN_10 };

gpio_init(tx_pin, GPIO_MODE_ALTFUNC, GPIO_OTYPE_PUSH_PULL,
          GPIO_PUPD_UP, GPIO_SPEED_HIGH);
gpio_init(rx_pin, GPIO_MODE_ALTFUNC, GPIO_OTYPE_PUSH_PULL,
          GPIO_PUPD_UP, GPIO_SPEED_HIGH);
gpio_set_alt_func(tx_pin, GPIO_AF7);
gpio_set_alt_func(rx_pin, GPIO_AF7);
```

### UART 初始化

```c
uart_config_t uart1 = {
    .uart_base    = USART1_BASE,
    .baud_rate    = 115200,            // 115200 bps
    .word_length  = UART_WORD_LENGTH_8, // 8 bit data
    .stop_bits    = UART_STOP_BITS_1,  // 1 stop bit
    .parity       = UART_PARITY_NONE,  // No parity
    .flow_ctrl    = UART_FLOW_CTRL_NONE,
    .oversampling = UART_OVERSAMPLING_16,
};
uart_init(&uart1);
```

### 发送数据

```c
// 方式1: 单字节发送
uart_transmit_byte(USART1_BASE, 'A');

// 方式2: 缓冲发送
uint8_t tx_data[] = "Hello World\r\n";
uart_transmit(USART1_BASE, tx_data, sizeof(tx_data) - 1);

// 方式3: 字符串发送
uart_send_string(USART1_BASE, "System Ready!\r\n");

// 方式4: 格式化字符串发送
char buf[64];
snprintf(buf, sizeof(buf), "Freq: %lu Hz\r\n", clock_get_freq(CLOCK_SYS));
uart_send_string(USART1_BASE, buf);
```

### 接收数据

```c
// 方式1: 单字节接收
uint8_t ch = uart_receive_byte(USART1_BASE);

// 方式2: 多字节缓冲接收
uint8_t rx_buf[128];
uint32_t count = uart_receive(USART1_BASE, rx_buf, sizeof(rx_buf));

// 方式3: 行接收 (遇 '\n' 结束)
char line_buf[256];
uint32_t len = uart_receive_line(USART1_BASE, line_buf, sizeof(line_buf));
```

### 中断驱动接收

```c
// 使能 RXNE (接收缓冲区非空中断)
uart_enable_interrupt(USART1_BASE, UART_CR1_RXNEIE);

// 在 ISR 中处理
void USART1_IRQHandler(void) {
    if (uart_get_status(USART1_BASE) & UART_SR_RXNE) {
        uint8_t byte = uart_receive_byte(USART1_BASE);
        // 处理接收到的字节
        process_byte(byte);
    }
}
```

### UART DMA 发送

```c
// 配置 DMA 传输
dma_configure_stream(DMA2_BASE, &dma_uart_cfg);
dma_set_source(DMA2_BASE, DMA_STREAM_7, (uint32_t)tx_buffer);
dma_set_destination(DMA2_BASE, DMA_STREAM_7, USART1_BASE + UART_DR_OFFSET);
dma_set_transfer_count(DMA2_BASE, DMA_STREAM_7, data_size);
dma_enable_stream(DMA2_BASE, DMA_STREAM_7);
```

---

## SPI 传感器读取示例

### SPI 初始化

```c
spi_config_t spi1 = {
    .spi_base       = SPI1_BASE,
    .mode           = SPI_MODE_MASTER,
    .direction      = SPI_DIRECTION_2LINES,   // 全双工
    .data_size      = SPI_DATA_SIZE_8BIT,
    .cpol           = SPI_CPOL_LOW,           // CK=0 idle
    .cpha           = SPI_CPHA_1EDGE,         // 1st edge sample
    .baud_prescaler = SPI_BAUD_PRESCALER_8,  // PCLK/8 = 10.5MHz
    .first_bit      = SPI_FRAME_MSB_FIRST,
    .crc_enable     = false,
};

// PB3=SCK(AF5), PB4=MISO(AF5), PB5=MOSI(AF5), PB6=NSS(AF5)
gpio_pin_t sck  = { GPIOB_BASE, GPIO_PIN_3 };
gpio_pin_t miso = { GPIOB_BASE, GPIO_PIN_4 };
gpio_pin_t mosi = { GPIOB_BASE, GPIO_PIN_5 };
gpio_pin_t nss  = { GPIOB_BASE, GPIO_PIN_6 };
// ... gpio_init + gpio_set_alt_func(AF5) for each pin

spi_init(&spi1);
```

### 读取 W25Q64 Flash ID

```c
// W25Q64 指令: 0x9F = JEDEC ID
// 发送 0x9F，接收 3 字节 (Manufacturer + Memory Type + Capacity)

gpio_write_pin(nss, false);  // CS低
uint8_t mfr = spi_transmit_receive_8(SPI1_BASE, 0x9F);
uint8_t type = spi_transmit_receive_8(SPI1_BASE, 0xFF);
uint8_t cap = spi_transmit_receive_8(SPI1_BASE, 0xFF);
gpio_write_pin(nss, true);   // CS高

printf("SPI Flash: MFR=0x%02X Type=0x%02X Cap=0x%02X\n", mfr, type, cap);
// 预期: MFR=0xEF Type=0x40 Cap=0x17 (Winbond W25Q64)
```

### SPI DMA 接收

```c
// 读取 Flash 状态寄存器 (DMA 模式)
uint8_t rx_dma_buf[256];

dma_config_t spi_dma = {
    .dma_base  = DMA2_BASE,
    .stream    = DMA_STREAM_0,
    .channel   = DMA_CHANNEL_3,      // SPI1_RX
    .direction = DMA_DIR_PERIPH_TO_MEM,
    // ... 其他配置
};
dma_configure_stream(DMA2_BASE, &spi_dma);
dma_set_source(DMA2_BASE, 0, SPI1_BASE + 0x0C);  // SPI_DR
dma_set_destination(DMA2_BASE, 0, (uint32_t)rx_dma_buf);
dma_set_transfer_count(DMA2_BASE, 0, 256);
dma_enable_stream(DMA2_BASE, 0);
```

---

## I2C EEPROM 读写示例

### I2C 初始化

```c
i2c_config_t i2c1 = {
    .i2c_base     = I2C1_BASE,
    .clock_speed  = 100000,              // 标准模式 100kHz
    .duty_cycle   = I2C_DUTY_CYCLE_2,    // 标准速度用 2:1
    .own_address  = 0x30,                // 自身地址
    .addr_mode    = I2C_ADDR_MODE_7BIT,  // 7-bit 寻址
    .general_call = false,               // 不响应通用呼叫
    .no_stretch   = false,               // 允许时钟拉伸
};

gpio_pin_t scl = { GPIOB_BASE, GPIO_PIN_6 };
gpio_pin_t sda = { GPIOB_BASE, GPIO_PIN_7 };
// ... gpio_init(OD, PU, AF4)

i2c_init(&i2c1);
```

### I2C 设备探测 (Bus Scan)

```c
void i2c_scan_bus(uint32_t i2c_base) {
    printf("I2C Bus Scan:\n");
    for (uint8_t addr = 1; addr < 128; addr++) {
        if (i2c_is_device_ready(i2c_base, addr, 1)) {
            printf("  0x%02X found\n", addr);
        }
    }
}
// 输出示例:
// I2C Bus Scan:
//   0x50 found  (AT24C02 EEPROM)
//   0x68 found  (DS3231 RTC)
```

### EEPROM 单字节写入

```c
// AT24C02 写入流程:
// START → DevAddr(W) → RegAddr → DataByte → STOP

uint8_t wr_data[] = { 0xAB };
bool ok = i2c_master_transmit(I2C1_BASE, 0x50, wr_data, 1);
printf("EEPROM write: %s\n", ok ? "ACK" : "NACK");
```

### EEPROM 多字节读取

```c
// 方法: 先写寄存器地址, 再 restart 读取

uint8_t dev_addr = 0x50;
uint8_t reg_addr = 0x00;   // 从地址 0 开始读取
uint8_t rd_buf[16];

// 使用组合 write+read 操作
bool result = i2c_master_write_read(
    I2C1_BASE,             // I2C 基地址
    dev_addr,              // 设备地址
    reg_addr,              // 起始寄存器地址
    NULL, 0,               // 不写数据 (仅写地址)
    rd_buf, sizeof(rd_buf) // 读取 16 字节
);

if (result) {
    printf("EEPROM dump [0x00-0x0F]:\n");
    for (int i = 0; i < 16; i++) {
        printf(" %02X", rd_buf[i]);
    }
    printf("\n");
}
```

### EEPROM 页写入

```c
// AT24C02 页大小: 8 字节 (地址 0x000~0x007 为一页)
// 页写入必须对齐页边界，否则会回卷

uint8_t page_data[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };

i2c_generate_start(I2C1_BASE);
i2c_send_address(I2C1_BASE, 0x50, false);  // write
i2c_send_data(I2C1_BASE, 0x00);            // register = 0x00

for (int i = 0; i < 8; i++) {
    i2c_send_data(I2C1_BASE, page_data[i]);
}
i2c_generate_stop(I2C1_BASE);

// 等待写入完成 (最大 5ms)
// 通过 polling ACK 检测写入状态
uint32_t timeout = 0;
while (!i2c_is_device_ready(I2C1_BASE, 0x50, 1) && timeout < 1000) {
    timeout++;
}
```

---

## Timer PWM 示例

### PWM 初始化

```c
// TIM1 CH1 → PA8, PWM 输出控制 LED 亮度
pwm_config_t pwm_led = {
    .tim_base      = TIM1_BASE,
    .channel       = TIM_CHANNEL_1,
    .mode          = PWM_MODE_FAST,          // 快速 PWM
    .output_type   = PWM_OUTPUT_NORMAL,
    .duty_cycle    = 0,                       // 初始占空比 0%
    .period        = 1000 - 1,                // 1000 counts (84MHz/84 = 1MHz, 1MHz/1000 = 1kHz)
    .output_enable = true,
};
pwm_init(&pwm_led);
pwm_start(TIM1_BASE, TIM_CHANNEL_1);
```

### 动态调整占空比 (呼吸灯)

```c
// 呼吸灯效果: 占空比从 0% → 100% → 0% 循环
static int32_t duty = 0;
static int8_t direction = 1;

void pwm_breath_update(void) {
    duty += direction * 10;
    if (duty >= 1000) { duty = 1000; direction = -1; }
    if (duty <= 0)    { duty = 0;    direction =  1; }

    pwm_set_duty_cycle(TIM1_BASE, TIM_CHANNEL_1, (uint32_t)duty);
}

// 在主循环中每 10ms 调用一次
void main_loop(void) {
    static uint32_t last_ms = 0;
    uint32_t now = system_millis();  // 需实现 systick 计时

    if (now - last_ms >= 10) {
        last_ms = now;
        pwm_breath_update();
    }
}
```

### 互补输出 (TIM1 CH1N)

```c
// TIM1 CH1N → PA7 (仅在 TIM1/TIM8 可用)
pwm_config_t pwm_ch1n = {
    .tim_base      = TIM1_BASE,
    .channel       = TIM_CHANNEL_1,
    .mode          = PWM_MODE_FAST,
    .output_type   = PWM_OUTPUT_INVERTED,  // CH1N 反相 CH1
    .duty_cycle    = 750,                  // 75%
    .period        = 1000 - 1,
    .output_enable = true,
};
pwm_init(&pwm_ch1n);
```

### 多通道独立 PWM

```c
// 控制 RGB LED: TIM3 CH1(R) CH2(G) CH3(B)
void rgb_led_set(uint32_t r, uint32_t g, uint32_t b) {
    pwm_set_duty_cycle(TIM3_BASE, TIM_CHANNEL_1, r);  // Red
    pwm_set_duty_cycle(TIM3_BASE, TIM_CHANNEL_2, g);  // Green
    pwm_set_duty_cycle(TIM3_BASE, TIM_CHANNEL_3, b);  // Blue
}

// 初始化 3 个通道
static void init_rgb_led(void) {
    pwm_config_t cfg = {
        .tim_base = TIM3_BASE, .period = 1000 - 1,
        .mode = PWM_MODE_FAST, .output_type = PWM_OUTPUT_NORMAL
    };
    cfg.channel = TIM_CHANNEL_1; cfg.duty_cycle = 0; pwm_init(&cfg);
    cfg.channel = TIM_CHANNEL_2; cfg.duty_cycle = 0; pwm_init(&cfg);
    cfg.channel = TIM_CHANNEL_3; cfg.duty_cycle = 0; pwm_init(&cfg);
    pwm_start(TIM3_BASE, TIM_CHANNEL_1);
    pwm_start(TIM3_BASE, TIM_CHANNEL_2);
    pwm_start(TIM3_BASE, TIM_CHANNEL_3);
}
```

---

## 完整主程序框架

```c
#include "mcu_peripheral.h"
#include "gpio_uart.h"
#include "cortex_m_core.h"
#include "nvic_dma.h"
#include <stdio.h>
#include <string.h>

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

    init_uart();
    init_spi();
    init_i2c();
    init_pwm();

    uint32_t sysclk = clock_get_freq(CLOCK_SYS);
    char msg[64];
    snprintf(msg, sizeof(msg), "\r\nSystem booted @ %lu Hz\r\n", sysclk);
    uart_send_string(USART1_BASE, msg);

    printf("=== mini-mcu-peripherals Demo ===\n");
    printf("  UART1: 115200 8N1  (PA9/PA10)\n");
    printf("  SPI1:  Master, 8b  (PB3-PB6)\n");
    printf("  I2C1:  100kHz      (PB6/PB7)\n");
    printf("  TIM1:  PWM CH1     (PA8)\n");

    while (1) {
        // CLI via UART
        if (uart_receive_line(USART1_BASE, line_buf, sizeof(line_buf))) {
            process_command(line_buf);
        }

        // I2C sensor poll
        static uint32_t last_poll = 0;
        if (system_millis() - last_poll > 100) {
            last_poll = system_millis();
            read_i2c_sensor();
        }

        // PWM breath update
        pwm_breath_update();
    }
    return 0;
}
```

---

## 调试技巧

### 逻辑分析仪检查表

| 信号     | 预期                              | 常见异常                    |
|----------|----------------------------------|----------------------------|
| UART TX  | 115200 bps, 8N1                  | 波特率不匹配, 极性反转       |
| SPI SCK  | 10.5 MHz (PCLK/8)               | 分频器设置错误               |
| SPI MOSI | CS 拉低后发数据                   | NSS 未手动控制               |
| I2C SCL  | 100kHz, SDA 在 SCL 低期间变化     | 上拉电阻缺失 (需 4.7kΩ)      |
| PWM      | 1kHz 方波, 占空比变化              | 定时器时钟源错误              |

### 常见问题

| 问题                     | 原因                    | 解决                             |
|-------------------------|------------------------|---------------------------------|
| UART 无输出              | AF 未正确设置           | PA9=AF7, PA10=AF7               |
| SPI 全返回 0xFF          | MISO 未上拉 / 设备未响应 | 检查硬件连接和 CS                |
| I2C 一直 NACK            | 设备地址错 / 设备未上电  | 用逻辑分析仪看波形，确认地址     |
| PWM 无输出               | 定时器未启动 / 通道未使能 | `timer_start()` + `pwm_start()` |
| DMA 传输不完整           | 传输计数器未设置         | `dma_set_transfer_count()`      |

---

## API 参考

| 模块         | 关键函数                                   | 说明                  |
|-------------|------------------------------------------|----------------------|
| uart        | `uart_init()`, `uart_transmit()`, `uart_receive()` | 串口通信          |
| spi         | `spi_init()`, `spi_transmit_receive_8()`  | SPI 主从通信          |
| i2c         | `i2c_init()`, `i2c_master_write_read()`    | I2C 读写操作          |
| pwm         | `pwm_init()`, `pwm_set_duty_cycle()`       | PWM 输出配置          |
| dma         | `dma_configure_stream()`, `dma_start_transfer()` | 直接内存访问      |
| clock       | `clock_init()`, `clock_enable_peripheral()` | 时钟与外设使能       |
| nvic        | `nvic_init()`, `nvic_enable_irq()`         | 中断控制器配置         |

---

> **mini-mcu-embedded** — MCU 嵌入式开发框架 (C 语言实现)
