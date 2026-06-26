# mini-mcu-embedded — MCU嵌入式 (C 语言实现)

> 精简版 Cortex-M4 MCU 嵌入式驱动库，提供 GPIO、UART、SPI、I2C、Timer、PWM、ADC、DMA、RTC、Watchdog 等核心外设的完整 HAL 封装。

---

## 特性

- **Cortex-M4 核心**: NVIC、SCB、SysTick、MPU 寄存器级编程
- **内存管理**: Flash/SRAM/外设区域映射、位带 (bit-band) 操作、链接器脚本建模
- **中断系统**: 240 路 IRQ 管理、优先级分组、尾链/迟到配置
- **DMA 传输**: 双控制器、16 路 Stream、散射-聚集 (scatter-gather)、FIFO 模式
- **通信外设**: UART (6路)、SPI (4路)、I2C (3路) — 阻塞/中断/DMA 三种模式
- **定时器**: TIM1-14 通用/高级/基本定时器，PWM 输出、输入捕获、编码器模式
- **模拟外设**: ADC1-3 (12/10/8/6-bit, 温度传感器)
- **系统管理**: 时钟树配置 (HSI/HSE/PLL)、电源模式 (Sleep/Standby)、看门狗 (IWDG/WWDG)
- **硬件 CRC**: CRC32 计算引擎，可配置多项式
- **纯 C 实现**: 无依赖、仅使用标准库 `stdint.h`/`stdbool.h`，可直接编译为裸机固件或 Linux 模拟测试

---

## 项目结构

```
mini-mcu-embedded/
├── include/
│   ├── cortex_m_core.h      # Cortex-M4 内核: NVIC, SysTick, SCB, MPU
│   ├── gpio_uart.h          # GPIO, UART, SPI, I2C 外设接口
│   ├── mcu_peripheral.h     # Timer, PWM, RTC, WDG, ADC, CRC, Power, Clock
│   ├── memory_map.h         # 内存映射, 位带操作, 链接器段模型
│   └── nvic_dma.h           # NVIC 中断控制, DMA 传输及 Scatter-Gather
├── src/
│   ├── cortex_m_core.c      # Core 寄存器级实现
│   ├── gpio_uart.c          # GPIO + UART + SPI + I2C 寄存器驱动
│   ├── mcu_peripheral.c     # 综合外设驱动实现
│   ├── memory_map.c         # 内存/位带/段管理实现
│   └── nvic_dma.c           # NVIC + DMA 控制器实现
├── examples/
│   ├── example_gpio.c       # GPIO + UART + SPI + I2C 综合示例
│   ├── example_timer.c      # Timer / PWM / Watchdog 示例
│   └── example_memory.c     # Memory Map / NVIC 优先级 / DMA 示例
├── demos/
│   ├── mini-mcu-blinky/     # Blinky: 时钟+GPIO+SysTick ISR LED 翻转
│   └── mini-mcu-peripherals/# 综合外设: UART + SPI Flash + I2C EEPROM + PWM
├── docs/
│   ├── mcu_arch.md          # MCU 架构概述
│   └── mcu_peripheral_guide.md # 外设编程指南
├── Makefile
└── README.md
```

---

## 快速开始

### 编译 (模拟测试)

```bash
make
```

### 编译单个示例

```bash
gcc -Wall -Wextra -O2 -I include \
    src/cortex_m_core.c src/gpio_uart.c src/nvic_dma.c \
    src/memory_map.c src/mcu_peripheral.c \
    examples/example_gpio.c \
    -o bin/example_gpio.exe
./bin/example_gpio.exe
```

### 交叉编译到 ARM Cortex-M4

```bash
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -O2 \
    -I include \
    src/*.c examples/example_timer.c \
    -o firmware.elf -T linker.ld -nostartfiles
arm-none-eabi-objcopy -O binary firmware.elf firmware.bin
```

---

## API 概览

### GPIO

```c
gpio_pin_t led = { GPIOA_BASE, GPIO_PIN_5 };
gpio_init(led, GPIO_MODE_OUTPUT, GPIO_OTYPE_PUSH_PULL, GPIO_PUPD_NONE, GPIO_SPEED_HIGH);
gpio_write_pin(led, true);
gpio_toggle_pin(led);
bool state = gpio_read_pin(led);
gpio_set_alt_func(led, GPIO_AF7);
```

### UART

```c
uart_config_t cfg = { .uart_base = USART2_BASE, .baud_rate = 115200, ... };
uart_init(&cfg);
uart_send_string(USART2_BASE, "Hello\r\n");
uint8_t ch = uart_receive_byte(USART2_BASE);
```

### SPI

```c
spi_config_t cfg = { .spi_base = SPI1_BASE, .mode = SPI_MODE_MASTER, ... };
spi_init(&cfg);
uint8_t rx = spi_transmit_receive_8(SPI1_BASE, 0x9F); // 读取 Flash ID
```

### I2C

```c
i2c_config_t cfg = { .i2c_base = I2C1_BASE, .clock_speed = 100000, ... };
i2c_init(&cfg);
uint8_t buf[16];
i2c_master_write_read(I2C1_BASE, 0x50, 0x00, NULL, 0, buf, 16);
```

### Timer / PWM

```c
timer_config_t tim = { .tim_base = TIM2_BASE, .prescaler = 8399, .period = 9999 };
timer_init(&tim);
timer_start(TIM2_BASE);

pwm_config_t pwm = { .tim_base = TIM1_BASE, .channel = 1, .duty_cycle = 500 };
pwm_init(&pwm);
pwm_start(TIM1_BASE, TIM_CHANNEL_1);
```

### DMA

```c
dma_config_t dma = { .stream = 0, .channel = 0, .direction = DMA_DIR_MEM_TO_MEM };
dma_configure_stream(DMA1_BASE, &dma);
dma_set_source(DMA1_BASE, 0, src); dma_set_destination(DMA1_BASE, 0, dst);
dma_set_transfer_count(DMA1_BASE, 0, 1024);
dma_enable_stream(DMA1_BASE, 0);
```

### NVIC

```c
nvic_init();
nvic_set_priority(NVIC_IRQ_TIM2, NVIC_PRIORITY_MEDIUM);
nvic_enable_irq(NVIC_IRQ_TIM2);
```

### Clock

```c
clock_config_t clk = {
    .pll_enabled = true, .pll_m = 8, .pll_n = 336, .pll_p = 2,
};
clock_init(&clk);
uint32_t sysclk = clock_get_freq(CLOCK_SYS); // 168000000
```

### Memory Map

```c
memory_map_ctx_t mm;
memory_map_init(&mm);
memory_map_print_layout(&mm);
bool ok = memory_map_is_in_sram(&mm, 0x20001000);
bitband_write(0x40020014, 5, true);  // 原子位带写
```

---

## 内存布局

| 区域       | 起始地址      | 大小      | 类型       |
|-----------|-------------|----------|-----------|
| Flash     | `0x08000000` | 1 MB     | Code/Const|
| SRAM      | `0x20000000` | 192 KB   | Data/BSS/Heap/Stack |
| CCMRAM    | `0x10000000` | 64 KB    | Fast Data |
| Periph    | `0x40000000` | 128 MB   | Memory-Mapped I/O |
| Cortex Int| `0xE0000000` | 1 MB     | System Control |

---

## 构建参数

```makefile
CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -I include
OUTDIR  = bin
```

---

## 许可

MIT

> **mini-mcu-embedded** — MCU 嵌入式开发框架 (C 语言实现)
