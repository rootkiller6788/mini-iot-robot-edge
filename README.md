# Mini IoT Robot Edge

**From-scratch, zero-dependency C implementations** of IoT, robotics, edge computing, and embedded systems concepts. Each module models embedded hardware behavior at educational fidelity — from MCU architecture and RTOS kernels to edge AI inference, robot motion planning, and industrial control protocols.

## Modules

| Module | Topics | Key References |
|--------|--------|----------------|
| [mini-mcu-embedded](mini-mcu-embedded/) | MCU arch (ARM Cortex-M), GPIO/UART/SPI/I2C, interrupt controller (NVIC), DMA, memory map | ARM Cortex-M TRM |
| [mini-rtos](mini-rtos/) | FreeRTOS kernel sim: tasks (priority preemptive), scheduler, queue/semaphore/mutex, tick timer | FreeRTOS Kernel |
| [mini-embedded-linux](mini-embedded-linux/) | Yocto/Buildroot model, device tree overlay, kernel module, initramfs, squashfs | Linux Kernel, Yocto Docs |
| [mini-hardware-interface](mini-hardware-interface/) | GPIO/ADC/DAC/PWM sim, sensor polling/interrupt mode, actuator control, PID controller | Arduino, STM32 HAL |
| [mini-sensor-actuator](mini-sensor-actuator/) | IMU (accel/gyro), temp/humidity, ToF/lidar, motor (servo/stepper), camera (MIPI) | Bosch BMI270, ST VL53L5CX |
| [mini-edge-ai](mini-edge-ai/) | Edge inference (TFLite/ONNX Runtime), model conversion (quantization), edge TPU/NPU | TensorFlow Lite, ONNX |
| [mini-tinyml](mini-tinyml/) | TFLite Micro, model compression (prune/quantize), wake-word detection, anomaly detection | TinyML Book, TFLite Micro |
| [mini-ota-update](mini-ota-update/) | A/B firmware update, delta update (bsdiff/xdelta), OTA server/client, rollback, signed images | UEFI Capsule Update, AOSP A/B |
| [mini-edge-security](mini-edge-security/) | Secure boot on MCU, ARM TrustZone-M, PSA Certified, device attestation, secure storage | ARM PSA, TrustZone-M |
| [mini-robotics](mini-robotics/) | Kinematics (forward/inverse), path planning (A*, RRT), SLAM (EKF, particle), ROS sim | ROS 2, Probabilistic Robotics |
| [mini-industrial-control](mini-industrial-control/) | PLC model (ladder logic), Modbus, OPC-UA sim, SCADA data acquisition, safety PLC | IEC 61131-3, Modbus Spec |

## Design Philosophy

- **Zero external dependencies** — pure C (C99/C11), only `libc` and `libm`
- **Self-contained modules** — each directory has its own `Makefile`, `include/`, `src/`, `examples/`, `demos/`, `tests/`
- **Embedded simulation in user-space** — educational models of MCU peripherals, RTOS kernels, robotics, and industrial protocols
- **Theory-to-code mapping** — every module includes `docs/` with datasheet/standard-alignment notes
- **Practical demos** — FreeRTOS simulator, IMU sensor model, PID controller, SLAM simulator, PLC ladder-logic engine, and more

## Building

Each module is standalone. Navigate to a module directory and run:

```bash
cd mini-rtos
make all    # build everything
make test   # run tests
```

Requires **GCC** and **GNU Make**.

## Project Structure

```
mini-iot-robot-edge/
├── mini-mcu-embedded/           # MCU Embedded Systems
├── mini-rtos/                   # Real-Time Operating Systems
├── mini-embedded-linux/         # Embedded Linux
├── mini-hardware-interface/     # Hardware Interfaces
├── mini-sensor-actuator/        # Sensors & Actuators
├── mini-edge-ai/                # Edge AI
├── mini-tinyml/                 # TinyML
├── mini-ota-update/             # OTA Firmware Updates
├── mini-edge-security/          # Edge Security
├── mini-robotics/               # Robotics
└── mini-industrial-control/     # Industrial Control
```

## License

MIT
