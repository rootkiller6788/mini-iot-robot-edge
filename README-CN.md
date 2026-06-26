# Mini IoT Robot Edge（迷你物联网机器人边缘计算）

**从零开始、零依赖的 C 语言实现**，涵盖物联网、机器人、边缘计算和嵌入式系统概念。每个模块以教学级精度建模嵌入式硬件行为 — 从 MCU 架构和 RTOS 内核到边缘 AI 推理、机器人运动规划和工业控制协议。

## 模块总览

| 模块 | 主题 | 参考标准 |
|--------|--------|----------------|
| [mini-mcu-embedded](mini-mcu-embedded/) | MCU 架构（ARM Cortex-M）、GPIO/UART/SPI/I2C、中断控制器（NVIC）、DMA、内存映射 | ARM Cortex-M TRM |
| [mini-rtos](mini-rtos/) | FreeRTOS 内核仿真：任务（优先级抢占）、调度器、队列/信号量/互斥量、Tick 定时器 | FreeRTOS Kernel |
| [mini-embedded-linux](mini-embedded-linux/) | Yocto/Buildroot 模型、设备树 Overlay、内核模块、Initramfs、SquashFS | Linux Kernel, Yocto Docs |
| [mini-hardware-interface](mini-hardware-interface/) | GPIO/ADC/DAC/PWM 仿真、传感器轮询/中断模式、执行器控制、PID 控制器 | Arduino, STM32 HAL |
| [mini-sensor-actuator](mini-sensor-actuator/) | IMU（加速度计/陀螺仪）、温度/湿度、ToF/LiDAR、电机（舵机/步进）、相机（MIPI） | Bosch BMI270, ST VL53L5CX |
| [mini-edge-ai](mini-edge-ai/) | 边缘推理（TFLite/ONNX Runtime）、模型转换（量化）、边缘 TPU/NPU 仿真 | TensorFlow Lite, ONNX |
| [mini-tinyml](mini-tinyml/) | TFLite Micro、模型压缩（剪枝/量化）、唤醒词检测、异常检测 | TinyML Book, TFLite Micro |
| [mini-ota-update](mini-ota-update/) | A/B 固件更新、增量更新（bsdiff/xdelta）、OTA 服务器/客户端、回滚、签名镜像 | UEFI Capsule Update, AOSP A/B |
| [mini-edge-security](mini-edge-security/) | MCU 安全启动、ARM TrustZone-M、PSA 认证、设备证明、安全存储 | ARM PSA, TrustZone-M |
| [mini-robotics](mini-robotics/) | 运动学（正向/逆向）、路径规划（A*、RRT）、SLAM（EKF、粒子滤波）、ROS 仿真 | ROS 2, Probabilistic Robotics |
| [mini-industrial-control](mini-industrial-control/) | PLC 模型（梯形图）、Modbus 协议、OPC-UA 仿真、SCADA 数据采集、安全 PLC | IEC 61131-3, Modbus Spec |

## 设计理念

- **零外部依赖** — 纯 C（C99/C11），仅使用 `libc` 和 `libm`
- **模块自包含** — 每个目录自带 `Makefile`、`include/`、`src/`、`examples/`、`demos/`、`tests/`
- **用户态嵌入式仿真** — 对 MCU 外设、RTOS 内核、机器人和工业协议的教学级建模
- **理论到代码的映射** — 每个模块包含 `docs/` 目录，内有数据手册/标准对齐说明
- **实用演示程序** — FreeRTOS 仿真器、IMU 传感器模型、PID 控制器、SLAM 仿真器、PLC 梯形图引擎等

## 构建方式

每个模块相互独立。进入模块目录后运行：

```bash
cd mini-rtos
make all    # 构建全部
make test   # 运行测试
```

需要 **GCC** 和 **GNU Make**。

## 项目结构

```
mini-iot-robot-edge/
├── mini-mcu-embedded/           # MCU 嵌入式系统
├── mini-rtos/                   # 实时操作系统
├── mini-embedded-linux/         # 嵌入式 Linux
├── mini-hardware-interface/     # 硬件接口
├── mini-sensor-actuator/        # 传感器与执行器
├── mini-edge-ai/                # 边缘 AI
├── mini-tinyml/                 # TinyML 微型机器学习
├── mini-ota-update/             # OTA 固件更新
├── mini-edge-security/          # 边缘安全
├── mini-robotics/               # 机器人
└── mini-industrial-control/     # 工业控制
```

## 许可证

MIT
