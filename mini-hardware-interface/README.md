# mini-hardware-interface — 硬件接口 (C 语言实现)

嵌入式硬件抽象层库，提供 GPIO / PWM / ADC、传感器轮询、PID 控制、执行器驱动、信号调理等模块的 C99 标准实现。

## 模块结构

| 模块 | 头文件 | 功能 |
|------|--------|------|
| GPIO / PWM / ADC | `gpio_pwm_adc.h` | 数字 IO、模拟输入、PWM 输出、舵机控制 |
| 传感器轮询 | `sensor_polling.h` | I2C/SPI 寄存器读写、周期轮询、中断、数据平滑、校准 |
| PID 控制器 | `pid_controller.h` | 位置/增量 PID、抗饱和、前馈、自整定、级联 |
| 执行器驱动 | `actuator_driver.h` | 直流电机、无刷电机、步进电机、舵机、软启动、急停 |
| 信号调理 | `signal_cond.h` | 运放、滤波器、噪声滤波、电平转换、惠斯通电桥、隔离 |

## 构建

```sh
make          # 构建静态库 libmini-hardware-interface.a
make examples # 构建示例程序
make demos    # 构建演示程序
make clean    # 清理构建产物
```

## 标准

C99 (ISO/IEC 9899:1999)，无外部依赖。

## 文件清单

```
include/
  gpio_pwm_adc.h        - GPIO / PWM / ADC / 舵机接口
  sensor_polling.h      - 传感器轮询与数据滤波接口
  pid_controller.h      - PID 控制器接口
  actuator_driver.h     - 执行器驱动接口
  signal_cond.h         - 信号调理接口
src/
  gpio_pwm_adc.c        - GPIO / PWM / ADC / 舵机实现
  sensor_polling.c      - 传感器轮询与数据滤波实现
  pid_controller.c      - PID 控制器实现
  actuator_driver.c     - 执行器驱动实现
  signal_cond.c         - 信号调理实现
examples/
  example_gpio_servo.c  - GPIO + PWM + 舵机示例
  example_sensor.c      - 传感器读取与滤波示例
  example_pid_motor.c   - PID 电机控制示例
demos/
  demo_robot_control.c  - 差速驱动机器人完整控制系统
  demo_env_monitor.c    - 多传感器环境监测站
docs/
  API_REFERENCE.md      - API 参考手册
  HARDWARE_GUIDE.md     - 硬件集成指南
Makefile
README.md
```
