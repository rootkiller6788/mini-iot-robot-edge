# mini-mcu-blinky — LED Blinky 示例

> MCU 入门示例：初始化时钟、配置 GPIO、设置 SysTick、在 ISR 中翻转 LED。

---

## 概述

本示例演示了嵌入式 MCU 编程中最经典的 "Hello World" —— LED 闪烁程序。
通过从零开始配置系统时钟、GPIO 引脚和 SysTick 定时器中断，
展示 `mini-mcu-embedded` 库的核心 API 使用范式。

## 硬件需求

| 组件          | 说明                           |
|---------------|------------------------------- |
| MCU           | STM32F407VG (Cortex-M4, 168MHz)|
| LED           | 连接至 PA5 (GPIOA Pin 5)       |
| 调试器        | ST-Link / J-Link               |
| 电源          | 3.3V                           |

## 系统框图

```
┌──────────────────────────────────────────────────┐
│                  System Clock                      │
│  HSI (16MHz) ──> PLL ×336 ──> SYSCLK 168MHz      │
│    │                │                              │
│    └──┬── HCLK = 168MHz                           │
│       ├── APB1 = 42MHz                            │
│       └── APB2 = 84MHz                            │
├──────────────────────────────────────────────────┤
│              SysTick Timer                         │
│  reload = 168000000 / 1000 - 1 = 167999           │
│  → 1ms interval                                   │
├──────────────────────────────────────────────────┤
│              GPIOA Configuration                   │
│  PA5: Output mode, Push-Pull, No Pull             │
│  Speed: Very High                                 │
└──────────────────────────────────────────────────┘
```

## 初始化流程

### 1. 系统时钟配置

```c
clock_config_t clk_cfg = {
    .sysclk_source = CLOCK_SRC_PLL,
    .hclk_prescaler = 1,
    .pclk1_prescaler = 4,
    .pclk2_prescaler = 2,
    .hse_enabled = true,
    .hse_freq_hz = 8000000UL,
    .pll_enabled = true,
    .pll_source = CLOCK_PLL_SRC_HSE,
    .pll_m = 8,
    .pll_n = 336,
    .pll_p = 2,
    .pll_q = 7,
};
clock_init(&clk_cfg);              // HSE 8MHz -> PLL -> 168MHz

printf("SYSCLK  : %lu Hz\n", clock_get_freq(CLOCK_SYS));
printf("HCLK    : %lu Hz\n", clock_get_freq(CLOCK_HCLK));
printf("PCLK1   : %lu Hz\n", clock_get_freq(CLOCK_PCLK1));
printf("PCLK2   : %lu Hz\n", clock_get_freq(CLOCK_PCLK2));
printf("PLL     : %lu Hz\n", clock_get_freq(CLOCK_PLL));
printf("PLL48   : %lu Hz\n", clock_get_freq(CLOCK_PLL48));
```

### 2. GPIO 引脚配置

```c
// 使能 GPIOA 时钟
clock_enable_peripheral(GPIOA_BASE);

// 配置 PA5 为推挽输出
gpio_pin_t led_pin = { GPIOA_BASE, GPIO_PIN_5 };
gpio_init(led_pin, GPIO_MODE_OUTPUT, GPIO_OTYPE_PUSH_PULL,
          GPIO_PUPD_NONE, GPIO_SPEED_VERY_HIGH);

// 初始状态：LED 关闭
gpio_write_pin(led_pin, false);
```

### 3. SysTick 定时器配置

```c
// SysTick 时钟源 = HCLK = 168MHz
// 目标频率: 1kHz → reload = 168000000/1000 - 1
uint32_t reload = 168000 - 1;  // 168MHz / 168000 = 1kHz

cortex_m4_systick_init(reload);
cortex_m4_systick_enable_interrupt();  // 使能 SysTick 中断
cortex_m4_systick_enable();            // 启动 SysTick
```

### 4. NVIC 配置

```c
nvic_init();  // 初始化 NVIC，设置优先级分组为 4 bit group + 2 bit sub

// SysTick 是系统异常，优先级通过 SCB_SHPR 设置
cortex_m4_scb_set_priority_grouping(PRIGROUP_NVIC_4_2);
```

## 中断服务程序 (ISR)

```c
/**
 * @brief SysTick 中断处理函数
 *
 * 每 1ms 触发一次，通过软件计数器实现 500ms 间隔的 LED 翻转。
 * 在真实的 MCU 代码中，此函数需放置在向量表中。
 */
void SysTick_Handler(void) {
    static uint32_t tick_count = 0;
    static bool led_state = false;

    tick_count++;

    if (tick_count >= 500) {  // 500ms
        tick_count = 0;
        led_state = !led_state;

        gpio_pin_t led = { GPIOA_BASE, GPIO_PIN_5 };
        gpio_write_pin(led, led_state);
    }
}
```

## 向量表设置

```c
// 定义向量表
vector_table_t g_vector_table = {
    .initial_sp       = SRAM_BASE + SRAM_SIZE_DEFAULT,  // 栈顶
    .reset_handler    = Reset_Handler,
    .nmi_handler      = NMI_Handler,
    .hard_fault_handler = HardFault_Handler,
    .systick_handler  = SysTick_Handler,
    // ... 其他向量设为默认 handler
};
```

## 完整主程序

```c
#include "mcu_peripheral.h"
#include "gpio_uart.h"
#include "cortex_m_core.h"
#include "nvic_dma.h"
#include <stdio.h>

int main(void) {
    // 1. 系统初始化
    nvic_init();

    // 2. 时钟配置: HSE 8MHz → PLL → 168MHz
    clock_config_t clk = {
        .sysclk_source = CLOCK_SRC_PLL,
        .hclk_prescaler = 1,
        .pclk1_prescaler = 4,
        .pclk2_prescaler = 2,
        .hse_enabled = true,
        .hse_freq_hz = 8000000UL,
        .pll_enabled = true,
        .pll_source = CLOCK_PLL_SRC_HSE,
        .pll_m = 8, .pll_n = 336, .pll_p = 2, .pll_q = 7
    };
    clock_init(&clk);
    printf("Clock initialized: SYSCLK = %lu Hz\n", clock_get_freq(CLOCK_SYS));

    // 3. GPIO 初始化
    clock_enable_peripheral(GPIOA_BASE);
    gpio_pin_t led = { GPIOA_BASE, GPIO_PIN_5 };
    gpio_init(led, GPIO_MODE_OUTPUT, GPIO_OTYPE_PUSH_PULL,
              GPIO_PUPD_NONE, GPIO_SPEED_VERY_HIGH);

    // 4. SysTick 配置 (1ms 中断间隔)
    cortex_m4_systick_init(168000 - 1);
    cortex_m4_systick_enable_interrupt();
    cortex_m4_systick_enable();

    // 5. 主循环 (低功耗等待)
    while (1) {
        cortex_m4_wait_for_interrupt();  // WFI
    }
    return 0;
}
```

## 构建与烧录

### 使用 GCC + Make

```bash
cd mini-mcu-embedded
make
# 生成 bin/main_blinky.exe (模拟运行)
```

### 使用 arm-none-eabi-gcc (真实硬件)

```bash
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -O2 \
    -I include \
    src/cortex_m_core.c src/gpio_uart.c src/nvic_dma.c \
    src/memory_map.c src/mcu_peripheral.c \
    examples/example_timer.c \
    -o blinky.elf -T linker.ld
arm-none-eabi-objcopy -O binary blinky.elf blinky.bin
```

## 预期行为

```
Clock initialized: SYSCLK = 168000000 Hz
LED on PA5: ON (2Hz, 50% duty)
```

- LED 以 2Hz 频率闪烁 (每 500ms 翻转一次)
- 示波器测 PA5 应看到 2Hz 方波

## 扩展练习

1. **修改闪烁频率**: 改变 `tick_count >= 500` 中的阈值
2. **双 LED 交替闪烁**: 增加 PE0 引脚配置，实现交替闪烁
3. **使用 Timer 替代 SysTick**: 用 TIM2 实现硬件定时翻转
4. **低功耗模式**: 在 LED 关闭期间进入 Sleep 模式
5. **添加 UART 调试输出**: 通过 USART2 输出系统状态信息

## 故障排除

| 症状                    | 可能原因                          | 解决方案                      |
|------------------------|----------------------------------|------------------------------|
| LED 不亮                | GPIO 未初始化 / 时钟未使能         | 检查 `clock_enable_peripheral` |
| LED 常亮/常灭           | SysTick 中断未触发                 | 检查 `cortex_m4_systick_enable`|
| 闪烁频率不正确           | 时钟配置错误                       | 验证 PLL 参数 (M/N/P)          |
| Hard Fault             | 向量表未正确设置                   | 检查 SP 初始化值和向量表偏移   |
| 程序不运行              | 复位引脚 / 电源问题 / Boot0 配置    | 检查硬件连接                   |

---

## API 参考

本示例使用的核心 API:

| 模块            | 函数                                 | 说明                     |
|-----------------|-------------------------------------|--------------------------|
| clock           | `clock_init()`                      | 初始化系统时钟树           |
| clock           | `clock_get_freq()`                  | 获取时钟节点频率           |
| clock           | `clock_enable_peripheral()`         | 使能外设时钟              |
| gpio            | `gpio_init()`                       | 配置 GPIO 引脚模式         |
| gpio            | `gpio_write_pin()`                  | 设置引脚输出电平           |
| cortex_m4_core  | `cortex_m4_systick_init()`          | 初始化 SysTick 定时器     |
| cortex_m4_core  | `cortex_m4_systick_enable()`        | 启动 SysTick              |
| cortex_m4_core  | `cortex_m4_systick_enable_interrupt()`| 使能 SysTick 中断        |
| nvic_dma        | `nvic_init()`                       | 初始化 NVIC               |
| cortex_m4_core  | `cortex_m4_wait_for_interrupt()`    | WFI 低功耗等待             |

---

> **mini-mcu-embedded** — MCU 嵌入式开发框架 (C 语言实现)
