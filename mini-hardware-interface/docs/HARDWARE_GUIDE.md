# mini-hardware-interface 硬件集成指南

## 概述

本库为嵌入式硬件抽象层 (HAL)，提供 C99 标准实现的 GPIO/PWM/ADC 控制、传感器采集与滤波、PID 闭环控制、执行器驱动、信号调理等模块。

适用场景: 机器人控制、物联网终端、工业自动化、环境监测等。

## 平台适配

库核心算法 (PID、滤波器、传感器管理) 是平台无关的。GPIO/PWM/ADC 的实际寄存器操作需要通过 `src/gpio_pwm_adc.c` 中的内部状态结构替换为平台特定 HAL 调用。当前实现使用内存模拟，便于脱机开发和测试。

### 适配到具体 MCU

1. 将 `s_pins[]`, `s_pwm[]`, `s_adc[]` 中的读写替换为 MCU 的 GPIO/PWM/ADC HAL 函数
2. 定时器 Tick: 在主循环或中断中提供毫秒计数，传递给各模块的 `tick` 函数
3. I2C/SPI: 替换 `mhi_bus_*` 函数体为 MCU 的 I2C/SPI 驱动

## 引脚规划

### 直流电机 H 桥接线

```
MCU PWMx  -----> H-Bridge EN (速度)
MCU GPIO1 -----> H-Bridge IN1 (方向)
MCU GPIO2 -----> H-Bridge IN2 (方向)
```

H 桥真值表:
| IN1 | IN2 | 状态 |
|-----|-----|------|
| H | L | 正转 (CW) |
| L | H | 反转 (CCW) |
| H | H | 制动 (Brake) |
| L | L | 滑行 (Coast) |

### BLDC 三相六步驱动

6 PWM 输出 + 3 Hall 输入:

```
UH ──→ 上桥 U 相     UL ──→ 下桥 U 相
VH ──→ 上桥 V 相     VL ──→ 下桥 V 相
WH ──→ 上桥 W 相     WL ──→ 下桥 W 相
Ha, Hb, Hc ──→ Hall 传感器 A/B/C
```

六步换相表 (120° 霍尔传感器):
| Hall (CBA) | 导通相 |
|------------|--------|
| 001 | UH, WL |
| 010 | VH, WL |
| 011 | VH, UL |
| 100 | WH, VL |
| 101 | WH, UL |
| 110 | UH, VL |

### 步进电机 4 线

```
A1 ──→ 线圈 A+
A2 ──→ 线圈 A-
B1 ──→ 线圈 B+
B2 ──→ 线圈 B-
```

全步 (4 拍): A+B+, A+B-, A-B-, A-B+
半步 (8 拍): A+, A+B+, B+, A-B+, A-, A-B-, B-, A+B-

### 舵机

标准信号: 50Hz PWM (周期 20ms)
- 500μs → 0°
- 1500μs → 90° (中位)
- 2500μs → 180°

> 注意: 某些型号可能使用 600–2400μs 范围。使用 `mhi_servo_calibrate` 重新标定。

## 传感器集成

### I2C 传感器示例 (BME280)

```
   VCC ──────────────────── 3.3V
   GND ──────────────────── GND
   SCL ──→ MCU SCL (加 4.7kΩ 上拉至 3.3V)
   SDA ──→ MCU SDA (加 4.7kΩ 上拉至 3.3V)
```

初始化步骤:
```c
mhi_bus_config_t cfg = { .type = MHI_BUS_I2C, .address = 0x76,
                         .speed_hz = 400000 };
mhi_bus_init(&cfg);
// 配置 BME280 寄存器: 0xF2 (ctrl_hum), 0xF4 (ctrl_meas), 0xF5 (config)
// 读取: 0xF7–0xFE (press/temp/hum MSB+LSB+XLSB)
```

### SPI 传感器示例 (加速度计)

```
   MOSI ──→ MCU MOSI
   MISO ──→ MCU MISO
   SCLK ──→ MCU SCLK
   CS   ──→ MCU GPIO (片选)
```

## 信号调理电路

### 惠斯通电桥 (应变片)

1/4 桥接线:
```
          R1 (固定)
   Vex ──┬───┤├───┬─── Vout+
         │        │
         Rg       R2 (固定)
         │        │
   GND ──┴────────┴─── Vout-
```

电压输出: Vout ≈ Vex × GF × ε / 4 (小应变近似)

### 光耦隔离

LED 侧:
```
   MCU_GPIO ──→ R_led ──→ LED(阳极) ─→ LED(阴极) ─→ GND
   R_led = (V_mcu - Vf_led) / If_desired
```

输出侧:
```
   Vcc_output ──→ R_pullup ──→ 集电极 ─→ 发射极 ─→ GND_output
   MCU_GPIO ←── 集电极 (开漏输出)
```

### 抗混叠滤波器 (ADC 前端)

采样率 fs, 信号带宽 B:
- 截止频率 fc ≤ fs / 2 (奈奎斯特)
- 阶数选择取决于阻带衰减需求

一阶 RC: fc = 1/(2πRC), 衰减 -20dB/dec

| fs | fc (推荐) | C=0.1μF 时的 R |
|----|----------|---------------|
| 1 kHz | 500 Hz | ~3.2 kΩ |
| 10 kHz | 5 kHz | ~318 Ω |
| 100 kHz | 50 kHz | ~32 Ω |

### 电平转换 (3.3V ↔ 5V)

电阻分压 (5V→3.3V):
```
   5V信号 ──→ R1 ──┬──→ 3.3V ADC 输入
                    │
                    R2
                    │
                   GND
   Vout = Vin × R2/(R1+R2)
   选 R1=1.7kΩ, R2=3.3kΩ 即可
```

使用 `mhi_level_shift_design` 自动计算:
```c
mhi_level_shift_t ls;
mhi_level_shift_design(&ls, 0.0f, 5.0f, 0.0f, 3.3f, 0.0f);
float vout = mhi_level_shift_apply(&ls, vin);
```

## PID 调参指南

### Ziegler-Nichols 振荡法

1. 设定 Ki=0, Kd=0
2. 逐步增大 Kp 直到输出产生持续等幅振荡
3. 记录此时的:
   - Ku = 临界增益 (ultimate gain)
   - Tu = 振荡周期 (秒, ultimate period)
4. 根据控制类型代入公式:

| 类型 | Kp | Ki = Kp/Ti | Kd = Kp×Td |
|------|-----|------------|------------|
| P | 0.50 Ku | - | - |
| PI | 0.45 Ku | 0.54 Ku / Tu | - |
| PID | 0.60 Ku | 1.20 Ku / Tu | 0.075 Ku × Tu |

### 抗饱和策略

| 方法 | 说明 | 适用场景 |
|------|------|----------|
| CLAMPING | 积分项限制在 [output_min, output_max] | 通用 |
| BACK_CALC | 当输出饱和时，按 Kb 比例减少积分 | 需要快速退饱和 |
| BOTH | 同时使用两种方法 | 高动态系统 |

### 前馈 (Feed-Forward)

利用已知的系统模型提供开环补偿:
```c
mhi_pid_set_feed_forward(&pid, expected_output_from_model);
```

## 执行器驱动

### 软启动配置

直流电机斜坡时间 (soft_start_ramp_s):
- 0.1–0.5s: 小惯量电机
- 0.5–2.0s: 中等惯量
- 2.0s+: 大惯量或重载

BLDC 斜坡时间 (soft_start_ms):
- 100–500ms: 小型无刷
- 500–2000ms: 中型无刷

### 电流限制

通过 `max_current_a` 和 `current_limit_a` 设定:
- 持续电流: current_limit_a
- 峰值电流: max_current_a (用于软启动期间的短时过载)

实际限流需要配合电流采样电路 (分流电阻 + ADC 或霍尔电流传感器)。

### 急停时序

1. 调用 `mhi_emergency_stop_all()` (全局) 或各电机单独的 `emergency_stop` 函数
2. 所有 PWM 输出立即置零
3. H 桥/三相桥所有开关断开
4. 恢复时调用 `mhi_dc_motor_resume()` 或 `mhi_emergency_resume_all()`，软启动重新开始

## 轮询调度策略

### 传感器管理器时间分配

6 个传感器, 各 poll_interval 不同:
- 100ms 间隔: 每秒 10 次
- 200ms: 5 次/秒
- 500ms: 2 次/秒
- 1000ms: 1 次/秒
- 2000ms: 0.5 次/秒

Manager 每次 `service()` 只轮询一个到期传感器。在最坏情况下不会阻塞超过一个传感器读取时间。配合 `mhi_sensor_manager_read_current()` 可强制立即读取。

## 示例接线总览

```
MCU
├── GPIO 4,5   → DC Motor L (IN1,IN2) + PWM 0 → EN
├── GPIO 6,7   → DC Motor R (IN1,IN2) + PWM 1 → EN
├── GPIO 8,9   → Fan (IN1,IN2) + PWM 3 → EN
├── GPIO 12,13 → Heater (IN1,IN2) + PWM 4 → EN
├── PWM 2      → Servo (信号)
├── ADC 0      → Battery voltage (via divider)
├── I2C SDA/SCL → BME280, BH1750, etc.
├── GPIO 10    → Rain gauge (interrupt, tip counter)
├── GPIO 11    → Anemometer (interrupt, pulse counter)
├── GPIO 2     → Button (INPUT_PULLUP)
└── GPIO 13    → LED (OUTPUT)
```

## 编译与烧录

```sh
make          # 构建 libmini-hardware-interface.a
make examples # 构建示例
make demos    # 构建演示
```

链接: `-Llib -lmini-hardware-interface -lm`

对于嵌入式目标, 替换 CC 为目标交叉编译器:
```sh
make CC=arm-none-eabi-gcc CFLAGS="-std=c99 -mcpu=cortex-m4 -mthumb -O2"
```
