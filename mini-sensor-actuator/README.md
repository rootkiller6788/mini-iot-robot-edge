# mini-sensor-actuator — 传感器与执行器 (C 语言实现)

C99 嵌入式传感器与执行器驱动库，面向微型机器人/无人机边缘计算节点。

## 模块

| 头文件 | 功能 |
|--------|------|
| `imu_sensor.h` | IMU: 加速度计/陀螺仪/磁力计, AHRS 融合, 欧拉角, FIFO, 运动检测 |
| `temp_env.h` | 环境: 温湿度/气压/气体/PM2.5/风速/雨量/紫外线 |
| `tof_lidar.h` | 距离: ToF/超声波/LIDAR 点云/多区 ToF/光流 |
| `motor_control.h` | 电机: 直流/步进/舵机, H桥/驱动芯片, PID, S曲线轨迹 |
| `camera_mipi.h` | 相机: MIPI CSI-2, Bayer→RGB, AE/AWB, ISP 管线 |

## 构建

```
make all
make examples
make demos
make clean
```

## 示例 & 演示

- `example_imu` — IMU 读取与姿态解算
- `example_tof` — ToF 测距与多区扫描
- `example_motor` — 电机 PID 控制与轨迹
- `demo_robot` — 差分驱动机器人完整演示 (250+ 行)
- `demo_drone` — 四旋翼传感器融合演示 (250+ 行)

## 依赖

C99 编译器, `<math.h>`, `<stdint.h>`, 无外部库依赖。

## 许可

MIT
