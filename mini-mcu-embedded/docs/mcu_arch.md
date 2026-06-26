# MCU Architecture Overview

> mini-mcu-embedded — 精简版 Cortex-M4 MCU 嵌入式库架构说明

---

## 架构总览

```
┌─────────────────────────────────────────────────────────┐
│                  Application Layer                       │
│  (example_gpio.c, example_timer.c, example_memory.c)    │
├─────────────────────────────────────────────────────────┤
│                  Hardware Abstraction Layer (HAL)        │
│  ┌─────────────┬──────────────────┬──────────────────┐  │
│  │ cortex_m_core│   nvic_dma       │  memory_map      │  │
│  │ (Cortex-M4  │  (NVIC + DMA    │  (Memory Layout  │  │
│  │  Core Regs) │   Controller)    │   + Bit-Band)    │  │
│  └─────────────┴──────────────────┴──────────────────┘  │
│  ┌─────────────┬──────────────────────────────────────┐  │
│  │  gpio_uart   │          mcu_peripheral              │  │
│  │ (GPIO + UART │  (Timer/PWM/RTC/WDG/ADC/CRC/        │  │
│  │  + SPI + I2C)│   Power Management/Clock Tree)       │  │
│  └─────────────┴──────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────┤
│                  Register Layer                          │
│  Direct volatile pointer access to memory-mapped I/O    │
├─────────────────────────────────────────────────────────┤
│                  Hardware (STM32F4xx)                    │
│  Cortex-M4 Core, NVIC, SCB, MPU, SysTick,               │
│  GPIO, UART, SPI, I2C, Timer, DMA, ADC, RTC, CRC, etc.  │
└─────────────────────────────────────────────────────────┘
```

## 模块划分

### 1. Core 模块 (`cortex_m_core.h/.c`)

负责 Cortex-M4 内核核心寄存器编程。

| 子模块      | 寄存器基址      | 功能                                                                 |
|------------|----------------|----------------------------------------------------------------------|
| NVIC       | `0xE000E100`   | 中断使能、禁能、挂起、优先级控制                                         |
| SysTick    | `0xE000E010`   | 24-bit 系统滴答定时器，用于 RTOS tick / 延时                             |
| SCB        | `0xE000ED00`   | 系统控制块：优先级分组、向量表偏移、系统复位、故障使能                     |
| MPU        | `0xE000ED90`   | 内存保护单元：8 个可编程区域，控制访问权限、缓存/缓冲/共享属性              |

**关键 API:**

```c
// NVIC
void cortex_m4_nvic_enable_irq(uint8_t irq_num);
void cortex_m4_nvic_set_priority(uint8_t irq_num, uint8_t priority);

// SysTick
void cortex_m4_systick_init(uint32_t reload_value);
void cortex_m4_systick_enable(void);

// SCB
void cortex_m4_scb_system_reset(void);
void cortex_m4_scb_set_priority_grouping(priority_grouping_t grouping);

// MPU
void cortex_m4_mpu_configure_region(uint8_t region, ...);
void cortex_m4_mpu_enable(void);

// CPU utility
void cortex_m4_wait_for_interrupt(void);    // WFI
void cortex_m4_disable_interrupts(void);    // CPSID I
```

### 2. Memory Map 模块 (`memory_map.h/.c`)

内存布局管理、位带操作、链接器脚本建模。

| 功能              | 说明                                                       |
|-------------------|----------------------------------------------------------|
| 区域描述           | Flash (1MB), SRAM (192KB), CCMRAM (64KB), 外设区, FSMC     |
| 段描述             | .text, .rodata, .data, .bss, .heap, .stack                 |
| 位带操作           | SRAM bit-band 别名 0x22000000, 外设 bit-band 别名 0x42000000 |
| 地址查询           | 判断地址是否在 Flash/SRAM/外设区域                           |

**内存布局:**

```
0x00000000 ┌────────────┐  Alias (Flash/BootROM)
           │   ...      │
0x08000000 ├────────────┤  Flash (1MB)
           │  .text     │
0x08004000 │  .rodata   │
0x08006000 │  .data(LMA) │
0x080FFFFF ├────────────┤
           │            │
0x10000000 ├────────────┤  CCMRAM (64KB)
0x1000FFFF ├────────────┤
           │            │
0x1FFF0000 ├────────────┤  BootROM / System Memory
0x1FFF77FF ├────────────┤
           │            │
0x20000000 ├────────────┤  SRAM (192KB)
           │  .data(VMA)│
0x20000800 │  .bss      │
0x20000C00 │  .heap     │
0x20010C00 │  ...       │
0x20010FFF │  .stack    │  (grows downward)
0x2002FFFF ├────────────┤
           │            │
0x40000000 ├────────────┤  Peripherals
           │  APB1      │  TIM2-7,12-14, RTC, WWDG, I2C1-3, SPI2-3, USART2-3,UART4-5
0x4000FFFF ├────────────┤
0x40010000 │  APB2      │  TIM1,8-11, USART1,6, ADC1-3, SPI1,4, SYSCFG, EXTI
0x4001FFFF ├────────────┤
0x40020000 │  AHB1      │  GPIOA-I, CRC, RCC, Flash, DMA1, DMA2
0x4003FFFF ├────────────┤
0x50000000 │  AHB2      │  USB OTG FS, DCMI
           │            │
0xE0000000 ├────────────┤  Cortex-M4 Internal
           │  ITM, DWT │
0xE000E000 ├────────────┤  NVIC, SCB, SysTick, MPU
           │            │
0xFFFFFFFF └────────────┘
```

### 3. NVIC + DMA 模块 (`nvic_dma.h/.c`)

中断向量控制器和直接内存访问控制器。

**NVIC 功能:**
- 240 个外部中断 (IRQ 0-239)
- 4-bit 优先级字段，支持优先级分组 (PRIGROUP)
- 尾链 (tail-chaining) 和迟到 (late-arrival) 配置
- 中断注册/注销

**DMA 功能:**
- 2 个 DMA 控制器 (DMA1, DMA2)，每个 8 个 Stream
- Stream 通道选择 (CHSEL)
- 传输方向: P→M, M→P, M→M
- 散射-聚集 (Scatter-Gather) 支持
- FIFO 模式、双缓冲、循环模式
- 中断: 传输完成、半传输、传输错误、FIFO 错误

### 4. GPIO + UART + SPI + I2C 模块 (`gpio_uart.h/.c`)

| 外设    | 基地址区间                     | 主要功能                              |
|--------|------------------------------|-------------------------------------|
| GPIO   | `0x40020000` - `0x40021C00`  | 16 引脚/端口, 模式/速度/上下拉/复用功能 |
| UART   | `0x40011000` 等 6 个           | 异步串口, 波特率可配, 中断/状态检测       |
| SPI    | `0x40013000` 等 4 个           | 主/从模式, 8/16-bit, 4 线, CRC          |
| I2C    | `0x40005400` 等 3 个           | 主/从模式, 7/10-bit 地址, 100/400kHz     |

### 5. MCU Peripheral 模块 (`mcu_peripheral.h/.c`)

综合外设驱动集合。

| 外设      | 说明                                                                |
|----------|-------------------------------------------------------------------|
| Timer    | TIM1-14, 基本/通用/高级定时器, 计数器/预分频/周期配置                   |
| PWM      | 输出比较模式, 占空比/极性/通道独立控制                                   |
| Capture  | 输入捕获模式, 上升/下降/双边沿触发, DMA 配合                            |
| Encoder  | 正交编码器模式, 计数/方向/速度检测                                     |
| RTC      | 日历/闹钟/唤醒定时器/时间戳                                            |
| Watchdog | IWDG (独立看门狗) + WWDG (窗口看门狗)                                   |
| ADC      | 12/10/8/6-bit 分辨率, 单次/连续/扫描模式, 温度传感器/Vref                |
| CRC      | 硬件 CRC 计算, 可配置多项式 (默认 0x04C11DB7)                           |
| Power    | Run/Sleep/DeepSleep/Standby 模式切换, 备份域/调压器控制                  |
| Clock    | 系统时钟树: HSI/HSE/PLL, 总线预分频 (HCLK/PCLK1/PCLK2), 外设时钟门控    |

---

## 时钟树

```
                      ┌──────────┐
     HSI 16MHz ──────>│          │
                      │  PLL     │    SYSCLK (max 168MHz)
     HSE 8MHz  ──────>│ ×N /(M×P)│──────┬────> ──
                      │  /Q →48 │        │      │
                      └──────────┘        │      │
                                          v      v
                                     ┌─────────┬─────────┐
                                     │ HCLK    │ PCLK1   │ PCLK2
                                     │ /1..512 │ /1..16  │ /1..16
                                     │ (AHB)   │ (APB1)  │ (APB2)
                                     └─────────┴─────────┴─────────┘
                                          │         │         │
                                     Cortex-M4   UART2-5  USART1,6
                                     DMA,GPIO    I2C      SPI1,4
                                     SRAM        TIM2-7   TIM1,8-11
                                     FSMC        RTC      ADC
                                                 WWDG     SYSCFG
```

---

## 中断系统

### 优先级分组

| PRIGROUP     | 抢占位 | 子优先级位 | 抢占优先级 | 子优先级 |
|-------------|-------|----------|-----------|---------|
| NVIC_16_0   | 4     | 0        | 16        | 1       |
| NVIC_8_1    | 3     | 1        | 8         | 2       |
| NVIC_4_2    | 2     | 2        | 4         | 4       |
| NVIC_2_3    | 1     | 3        | 2         | 8       |
| NVIC_1_4    | 0     | 4        | 1         | 16      |

### 关键中断向量

| IRQ # | 中断源              | 用途                       |
|-------|-------------------|---------------------------|
| 0-5   | WWDG,PVD,TAMP,etc.| 系统级中断                  |
| 11-17 | DMA1 Stream 0-6   | DMA1 传输完成/半完成/错误    |
| 28-30 | TIM2, TIM3, TIM4  | 通用定时器更新/捕获           |
| 31-34 | I2C1/I2C2 EV/ER   | I2C 事件/错误中断             |
| 35-36 | SPI1, SPI2        | SPI 中断                   |
| 37-39 | USART1-3          | UART 中断                  |
| 56-70 | DMA2 Stream 0-7   | DMA2 传输中断               |

---

## 工具链

| 组件           | 说明                               |
|---------------|----------------------------------|
| 编译器         | GCC (gcc for simulation, arm-none-eabi-gcc for target) |
| 汇编器         | GNU as                           |
| 链接器         | GNU ld, linker script (.ld)       |
| 构建工具       | GNU Make                          |

---

> **mini-mcu-embedded** — MCU 嵌入式开发框架 (C 语言实现)
