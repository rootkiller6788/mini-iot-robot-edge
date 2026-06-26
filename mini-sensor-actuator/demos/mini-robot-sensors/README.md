# Mini Robot Sensors — ToF/LiDAR, Camera & Motor Encoder Integration

## Overview

The mini-robot-sensors demo integrates Time-of-Flight (ToF) depth sensors, LiDAR scanning, MIPI camera image processing, and motor encoder feedback to provide a complete sensor suite for autonomous mobile robots. This demo demonstrates sensor fusion for obstacle avoidance, SLAM-adjacent mapping, and visual navigation.

## Sensor Architecture

```
+-------------------+     +------------------+     +-------------------+
|   ToF / LiDAR     |     |   MIPI Camera    |     |   Motor Encoders  |
| (Distance/Point)  |     |  (Vision/Color)  |     | (Position/Speed)  |
+--------+----------+     +--------+---------+     +---------+---------+
         |                         |                         |
         v                         v                         v
+------------------------------------------------------------------+
|                       Sensor Fusion Layer                         |
|  - Obstacle detection & distance                                  |
|  - Visual odometry / SLAM features                                |
|  - Wheel odometry (dead reckoning)                                |
|  - Combined pose estimation                                       |
+------------------------------------------------------------------+
         |
         v
+------------------------------------------------------------------+
|                     Robot Control System                          |
+------------------------------------------------------------------+
```

## Hardware Setup

### ToF Sensor (VL53L1X / VL53L0X)

```
VL53L1X ToF Sensor  ->  MCU
  VIN  -----------------  3.3V
  GND  -----------------  GND
  SCL  -----------------  I2C SCL
  SDA  -----------------  I2C SDA
  XSHUT ---------------  GPIO 4 (shutdown pin for multi-sensor)
  INT  -----------------  GPIO 3 (interrupt, optional)

Default I2C address: 0x29
Multiple sensors can share I2C bus by using XSHUT to assign addresses
```

### LiDAR Module (RPLIDAR A1 / YDLIDAR X4)

```
LiDAR Module      ->  MCU
  VCC  --------------  5V (power supply, ~350mA peak)
  GND  --------------  GND
  TX   --------------  UART RX (MCU receive)
  RX   --------------  UART TX (MCU transmit)
  MOTOCTL -----------  GPIO 12 (motor PWM control)

Communication: 115200 bps, 8N1 UART
```

### MIPI Camera (OV2640 / OV5640)

```
Camera Module     ->  MCU with MIPI CSI-2
  VCC  --------------  3.3V / 2.8V / 1.5V (check module requirements)
  GND  --------------  GND
  SCL  --------------  I2C SCL (for register config)
  SDA  --------------  I2C SDA
  MDP0 --------------  CSI D0+ (differential pair)
  MDN0 --------------  CSI D0-
  MDP1 --------------  CSI D1+
  MDN1 --------------  CSI D1-
  MCK  --------------  MIPI Clock+
  MCN  --------------  MIPI Clock-
  XCLK --------------  Master Clock Input (24MHz typically)
  PWDN --------------  GPIO (Power Down, active high)
  RST  --------------  GPIO (Reset, active low)
```

### Motor Encoder

```
Encoder       ->  MCU
  A phase  ------  GPIO (interrupt-capable pin for pulse counting)
  B phase  ------  GPIO (optional, for direction detection)
  VCC       ------  3.3V or 5V
  GND       ------  GND

Typical CPR (Counts Per Revolution): 64 - 1024
```

## ToF/LiDAR Distance Sensing

### Sensor Comparison

| Sensor | Type | Range | FOV | Resolution | Max Freq | Best Use |
|--------|------|-------|-----|------------|----------|----------|
| VL53L0X | ToF | 50-2000mm | 25° | Single zone | 50 Hz | Short obstacle avoidance |
| VL53L1X | ToF | 40-4000mm | 27° | 4x4 / ROI | 50 Hz | Multi-zone detection |
| VL53L5CX | ToF | 40-4000mm | 63° | 8x8 (64 zones) | 60 Hz | 3D gesture, people counting |
| TF-Luna | ToF | 0.2-8m | 2° | Single point | 250 Hz | Long-range rangefinder |
| RPLIDAR A1 | LiDAR | 0.15-12m | 360° | 0.9° angular | 5.5 Hz | SLAM, mapping |
| YDLIDAR X4 | LiDAR | 0.12-10m | 360° | 0.5° angular | 8 Hz | Fast mapping, navigation |
| HC-SR04 | Ultrasonic | 2-400cm | 15° | Single point | 25 Hz | Budget obstacle sensing |
| JSN-SR04T | Ultrasonic | 25-450cm | 50° | Single point | 25 Hz | Waterproof, outdoor |

### Multi-Zone ToF Usage

```c
tof_multizone_t zones;
tof_multizone_init(&zones, 8, 8);

tof_range_t ranges[64];
uint8_t i;
for (i = 0; i < 64; i++) {
    ranges[i] = read_tof_zone(i);
    tof_multizone_update(&zones, i, ranges[i].distance_mm);
}

float min_dist = tof_multizone_min(&zones);
float avg_dist = tof_multizone_avg(&zones);

printf("Nearest obstacle: %.0f mm\n", min_dist);
tof_multizone_print(&zones);
```

### LiDAR Point Cloud Processing

```c
lidar_scan_t scan;
lidar_scan_init(&scan);

for (float angle = 0; angle < 360.0f; angle += 1.0f) {
    float distance = read_lidar_distance_at_angle(angle);
    lidar_scan_to_cartesian(&scan, angle, distance);
}

lidar_scan_filter_outliers(&scan, 8000.0f);

uint16_t labels[2048];
uint16_t clusters = lidar_scan_cluster(&scan, 150.0f, labels);
printf("Detected %u obstacle clusters\n", clusters);

float nearest = lidar_scan_min_distance(&scan);
float farthest = lidar_scan_max_distance(&scan);
printf("Range: %.0f mm to %.0f mm\n", nearest, farthest);
```

### Obstacle Avoidance Strategy

```
1. Read all distance sensors
2. Filter outliers (median filter, 5-sample window)
3. Compute minimum distance in each sector (front-left, front, front-right)
4. Apply emergency stop if any distance < safety_threshold
5. Adjust trajectory proportional to distance in each sector
6. Use LiDAR cluster labels to differentiate obstacles from walls
```

```c
#define SAFETY_STOP_MM   100.0f
#define SLOW_DOWN_MM     300.0f
#define SECTOR_ANGLES    3  /* Left, Center, Right */

float sector_min[3] = {9999.0f, 9999.0f, 9999.0f};
for (int i = 0; i < scan.point_count; i++) {
    float angle = scan.points[i].angle_deg;
    int sector = (angle < 45.0f) ? 0 : (angle > 315.0f) ? 0 :
                 (angle < 135.0f) ? 1 : (angle > 225.0f) ? 1 : 2;
    if (scan.points[i].distance_mm < sector_min[sector])
        sector_min[sector] = scan.points[i].distance_mm;
}

if (sector_min[1] < SAFETY_STOP_MM) emergency_stop();
if (sector_min[0] < SLOW_DOWN_MM) turn_right();
if (sector_min[2] < SLOW_DOWN_MM) turn_left();
```

## Camera & Vision Pipeline

### Bayer Demosaic

The camera sensor outputs RAW Bayer pattern data (each pixel has only one color channel: R, G1, G2, or B). The demosaic algorithm reconstructs a full-color RGB image.

**Bayer Pattern Types:**

```
RGGB (most common):    GRBG:          GBRG:          BGGR:
R G R G               G R G R        G B G B        B G B G
G B G B               B G B G        R G R G        G R G R
```

**Demosaic Methods:**

| Method | Quality | Speed | Artifacts |
|--------|---------|-------|-----------|
| Nearest Neighbor | Low | Very Fast | Heavy color artifacts, blocky |
| Bilinear (implem.) | Medium | Fast | Smooth but soft edges |
| VNG | High | Slow | Good edge preservation |
| AHD / AMaZE | Best | Very Slow | Minimal artifacts, best edges |

### Auto Exposure Pipeline

```
RAW Frame -> [Brightness Measurement] -> [Exposure Error] -> [Gain Adjustment] -> Next Frame
                ^                                                            |
                |           Exposure Time + Analog Gain + Digital Gain       |
                +------------------------------------------------------------+
```

**Exposure Modes:**
- **Center-Weighted**: Weights center region more heavily (good for general use)
- **Spot**: Measures only a small central region (good for backlit subjects)
- **Matrix**: Full-frame average (good for outdoor, evenly lit scenes)

### Auto White Balance Pipeline

```
RGB Frame -> [Compute RGB Averages] -> [Gray World: make R_avg = G_avg = B_avg]
                or
          -> [White Patch: scale to max channel]
                |
                v
           [Convergence Filter] -> [Apply RGB Gains] -> Color-Corrected Output
```

### ISP Pipeline Flow

```
RAW Bayer Data
    |
    v
[Black Level Subtraction]
    |
    v
[Lens Shading Correction]
    |
    v
[White Balance]              <-- Auto White Balance (AWB) feedback
    |
    v
[Demosaic]                   <-- Bayer -> RGB conversion
    |
    v
[Color Correction Matrix]    <-- CCM 3x3 transform
    |
    v
[Gamma Correction]           <-- Gamma LUT (typically 2.2)
    |
    v
[Contrast / Saturation / Brightness]
    |
    v
[Output: RGB Frame or JPEG]
```

## Motor Encoder Odometry

### Differential Drive Odometry

```
For a two-wheel differential drive robot:

Left wheel distance:   dL = (ticks_L / tpr) * 2 * π * wheel_radius
Right wheel distance:  dR = (ticks_R / tpr) * 2 * π * wheel_radius

Linear displacement:   d = (dL + dR) / 2
Angular displacement:  θ = (dR - dL) / wheel_base

Position update:
  x(t+1) = x(t) + d * cos(θ_current + θ/2)
  y(t+1) = y(t) + d * sin(θ_current + θ/2)
  θ(t+1) = θ(t) + θ
```

```c
dc_motor_t left_motor, right_motor;
dc_motor_init(&left_motor, DC_MOTOR_DRIVER_L298N, 30.0f, 64);
dc_motor_init(&right_motor, DC_MOTOR_DRIVER_L298N, 30.0f, 64);

float wheel_radius_mm = 32.5f;
float wheel_base_mm = 150.0f;
float pos_x = 0.0f, pos_y = 0.0f, heading = 0.0f;

while (1) {
    dc_motor_update_encoder(&left_motor, read_encoder_ticks(LEFT), get_time_ms());
    dc_motor_update_encoder(&right_motor, read_encoder_ticks(RIGHT), get_time_ms());

    float dL = (float)left_motor.encoder.position_ticks /
               left_motor.ticks_per_rev * 2.0f * M_PI * wheel_radius_mm;
    float dR = (float)right_motor.encoder.position_ticks /
               right_motor.ticks_per_rev * 2.0f * M_PI * wheel_radius_mm;

    float d = (dL + dR) * 0.5f;
    float dtheta = (dR - dL) / wheel_base_mm;

    pos_x += d * cosf(heading + dtheta * 0.5f);
    pos_y += d * sinf(heading + dtheta * 0.5f);
    heading += dtheta;
}
```

## Sensor Fusion: Full Robot Pipeline

### Combined Odometry + IMU (Loose Coupling)

```c
imu_sensor_t imu;
imu_init(&imu, IMU_AHRS_MADGWICK, 200.0f);

float smooth_heading = 0.0f;
float alpha = 0.02f;  /* Complementary filter coefficient */

while (1) {
    /* IMU Update */
    imu_sample_t imu_sample = read_imu_sample();
    imu_update_all(&imu, &imu_sample);

    /* Wheel Odometry */
    update_wheel_odometry(&left_motor, &right_motor);
    float odo_heading = compute_odo_heading();

    /* Fuse heading */
    smooth_heading = alpha * odo_heading + (1.0f - alpha) * imu.angles.yaw;
}
```

### Distance + Visual Fusion

```c
camera_sensor_t cam;
camera_mipi_init(&cam, CAMERA_RES_720P, CAMERA_FORMAT_RAW8, 2);
camera_mipi_start_stream(&cam);

tof_sensor_t tof;
tof_init(&tof, TOF_VL53L1X);

while (1) {
    capture_frame(&cam.raw);
    camera_isp_pipeline(&cam);

    float brightness = camera_ae_compute_brightness(
        cam.rgb.rgb_data, cam.rgb.width, cam.rgb.height, cam.ae.mode);

    tof_range_t range = tof_range_continuous(&tof);

    if (range.distance_mm < 200.0f) {
        dc_motor_stop(&left_motor);
        dc_motor_stop(&right_motor);
        camera_mipi_set_resolution(&cam, CAMERA_RES_VGA);
        printf("Obstacle at %.0fmm! Snapping close-up photo.\n",
               range.distance_mm);
    }
}
```

## Performance & Tuning

### Sensor Update Rates for Different Robot Types

| Robot Type | IMU Rate | Distance Rate | Camera Rate | Motor PID Rate |
|------------|----------|--------------|-------------|----------------|
| Small indoor | 100 Hz | 30 Hz | 15 FPS | 100 Hz |
| Medium outdoor | 200 Hz | 20 Hz | 30 FPS | 200 Hz |
| High-speed drone | 400 Hz | 50 Hz | 60 FPS | 400 Hz |
| Precision arm | 100 Hz | 10 Hz | N/A | 1000 Hz |

### LiDAR Scanning Optimization

1. **Skip angles**: For 360° LiDAR, you can skip angles that face upward/downward
2. **Distance clipping**: Filter points beyond reliable range
3. **Downsampling**: Take every Nth point for large scans to reduce CPU load
4. **ROI scanning**: Only scan the forward-facing 180° for navigation
5. **Cluster merging**: Merge nearby clusters if they likely represent the same object

### Memory Budget (Typical Robot)

| Component | RAM Usage | Notes |
|-----------|-----------|-------|
| IMU state | 7 KB | FIFO + calibration |
| ToF state | 0.5 KB | Sensor config + multi-zone |
| LiDAR scan | 24 KB | 2048 points × 12 bytes/point |
| Camera RAW buffer | 2074 KB | 1920×1080 (if in RAM) |
| Camera RGB buffer | 6221 KB | 1920×1080×3 (if in RAM) |
| Motor control | 1 KB | 2× DC motor + PID states |
| **Total** | **~8.3 MB** | With 720p camera; 1080p is ~35MB |

### Motor PID Tuning Guide

| Term | Effect of Increase | Effect of Decrease |
|------|-------------------|---------------------|
| Kp (proportional) | Faster response, may overshoot | Slower response, less overshoot |
| Ki (integral) | Eliminates steady-state error, may oscillate | Error takes longer to eliminate |
| Kd (derivative) | Reduces overshoot, dampens oscillations | More overshoot, faster settling |
| **DC Motor (typical)** | Kp=1.5-3.0, Ki=0.1-0.5, Kd=0.01-0.05 |
| **Position servo** | Kp=2.0-5.0, Ki=0.01-0.1, Kd=0.02-0.1 |

## Troubleshooting

| Symptom | Likely Cause | Solution |
|---------|-------------|----------|
| ToF always returns -1 | Invalid measurement | Check ambient light, clean lens, increase timing budget |
| LiDAR erratic readings | Power supply noise | Add capacitor (1000uF) near LiDAR power input |
| Camera frame drops | MIPI lane configuration | Reduce lanes, lower frame rate, check PCB routing |
| Motor oscillation | PID gains too high | Halve Kp and double Kd, then retune |
| Encoder missing ticks | Interrupt latency | Use hardware pin interrupts, not polling |
| ToF cross-talk | Multiple sensors on same bus | Stagger measurement timing, use XSHUT addressing |
| IR interference | Sunlight or IR LEDs | Use 940nm ToF (less solar interference) |
