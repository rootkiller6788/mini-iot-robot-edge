# Mini IMU Fusion — Inertial Measurement Unit Sensor Fusion Demo

## Overview

The mini-imu-fusion demo demonstrates real-time 9-axis inertial measurement unit (IMU) sensor fusion using the Madgwick and Mahony AHRS (Attitude and Heading Reference System) algorithms. It combines accelerometer, gyroscope, and magnetometer data to produce stable orientation estimates in quaternion and Euler angle form.

## Hardware Configuration

### Recommended IMU Chips

| Chip | Accel | Gyro | Mag | Interface | Max ODR | Notes |
|------|-------|------|-----|-----------|---------|-------|
| ICM-20948 | 16g | 2000dps | 4900uT | I2C/SPI | 1.125kHz | 9-axis, best all-round |
| MPU-9250 | 16g | 2000dps | 4800uT | I2C/SPI | 4kHz | Legacy, widely available |
| BNO055 | 16g | 2000dps | 1300uT | I2C/UART | 100Hz | Built-in fusion |
| ICM-42688-P | 32g | 4000dps | N/A | I2C/SPI | 32kHz | 6-axis, ultra-low noise |
| LSM6DSO32 | 32g | 4000dps | N/A | I2C/SPI | 6.66kHz | High-g, vibration robust |

### Wiring Guide

```
IMU Module (ICM-20948)  ->  MCU
  VIN  ----------------------  3.3V (or 5V if onboard LDO)
  GND  ----------------------  GND
  SCL  ----------------------  I2C SCL (GPIO 22)
  SDA  ----------------------  I2C SDA (GPIO 21)
  (Optional SPI connection)
  CS   ----------------------  GPIO 5
  SCLK ----------------------  SPI SCLK (GPIO 18)
  MOSI ----------------------  SPI MOSI (GPIO 23)
  MISO ----------------------  SPI MISO (GPIO 19)
```

### Sensor Coordinate Frame

```
       +Z (Up)
        |
        |
        +---- +Y (Right)
       /
      /
    +X (Forward - out of screen)

Roll  = rotation around X axis
Pitch = rotation around Y axis
Yaw   = rotation around Z axis (heading)
```

## Algorithm Details

### Madgwick AHRS Filter

The Madgwick algorithm uses gradient descent to correct gyroscope drift using accelerometer and magnetometer data. It computes a quaternion derivative that is integrated to update the orientation estimate.

**Key Parameters:**

| Parameter | Symbol | Typical Value | Description |
|-----------|--------|---------------|-------------|
| Beta | beta | 0.04 - 0.15f | Gyroscope measurement error (rad/s). Higher = more accelerometer/mag trust |
| Sample Freq | fs | 100 - 400 Hz | Sensor sampling frequency |
| Initial Q | q0 | (1,0,0,0) | Initial quaternion (identity) |

**Algorithm Steps:**

1. Read sensor data: accelerometer (ax, ay, az), gyroscope (gx, gy, gz in rad/s), magnetometer (mx, my, mz)
2. Normalize accelerometer and magnetometer vectors
3. Compute the objective function gradient using the reference direction of gravity and magnetic field
4. Compute quaternion angular rates from gyroscope data: `q_dot = 0.5 * q ⊗ [0, gx, gy, gz]`
5. Apply gradient descent correction: `q_dot = q_dot - beta * gradient_normalized`
6. Integrate: `q = q + q_dot * dt`
7. Normalize quaternion

```
q_dot = 0.5 * q ⊗ ω - β * ∇f / ||∇f||

where:
  q      = orientation quaternion
  ω      = [0, gx, gy, gz]  (gyroscope readings in rad/s)
  β      = filter gain (gyroscope measurement error)
  ∇f     = gradient of the objective function
```

### Mahony Filter

The Mahony filter uses a Proportional-Integral (PI) controller to correct gyroscope drift. It is simpler and computationally less expensive than Madgwick but may converge slightly slower.

**Key Parameters:**

| Parameter | Symbol | Typical Value | Description |
|-----------|--------|---------------|-------------|
| Kp | beta | 0.5 - 2.0f | Proportional gain for error correction |
| Ki | - | 0.0f | Integral gain (not used in this implementation) |

**Algorithm Steps:**

1. Read sensor data and normalize accelerometer and magnetometer
2. Compute the estimated gravity direction from current quaternion
3. Compute the error between estimated gravity and measured gravity (cross product)
4. If magnetometer data is present, compute the error for the magnetic field direction
5. Apply PI correction to gyroscope readings: `g_corrected = g + 2 * Kp * error`
6. Integrate using corrected gyroscope data
7. Normalize quaternion

### Quaternion to Euler Conversion

```
roll  = atan2(2*qw*qx + 2*qy*qz, 1 - 2*qx^2 - 2*qy^2)
pitch = asin(2*qw*qy - 2*qz*qx)
yaw   = atan2(2*qw*qz + 2*qx*qy, 1 - 2*qy^2 - 2*qz^2)

Note: Output in radians, convert to degrees = rad * 180/π
```

### Gimbal Lock Avoidance

Quaternions intrinsically avoid gimbal lock, unlike Euler angles. The pitch angle is clamped to [-90, 90] degrees when converting from quaternion to Euler to handle the asin() function domain.

## Build & Run

### Prerequisites
- GCC or Clang (C99 compatible)
- GNU Make
- libm (math library, -lm flag)

### Build

```bash
cd mini-sensor-actuator
make all
make examples
```

### Run

```bash
./example_imu
```

### Expected Output

```
=== mini-sensor-actuator: IMU Sensor Fusion Demo ===

IMU initialized: AHRS=Madgwick, SampleRate=100Hz, Beta=0.10

Calibration applied: az_bias = -0.0200

Running sensor fusion simulation (200 iterations)...

Step   Time(s)      Accel (ax,ay,az)                           Euler (Roll, Pitch, Yaw)
----   -------      -----------------------                    ------------------------
0      0.00         (   0.00,    0.00,    9.80)     (     0.00     0.00     0.00 )
40     0.40         (   0.00,    0.00,    9.80)     (    -2.34     0.12     5.67 )
80     0.80         (   0.00,    0.00,    9.80)     (    -3.89    -0.01     8.90 )
120    1.20         (   0.00,    0.00,    9.80)     (    -2.01    -0.05    10.23 )
160    1.60         (   0.00,    0.00,    9.80)     (     1.23     0.03     8.45 )

=== Final Orientation ===
Madgwick:    Roll:    -0.42   Pitch:     0.01   Yaw:     9.15
  Quaternion: w=0.9966 x=-0.0034 y=0.0001 z=0.0795

=== Compare: Mahony Filter ===
Mahony:      Roll:    -0.38   Pitch:     0.02   Yaw:     9.12
  Quaternion: w=0.9966 x=-0.0031 y=0.0002 z=0.0795

=== Motion Detection ===
Freefall test (accel=0.3g): FREEFALL DETECTED

=== FIFO Buffer Test ===
FIFO count before flush: 73
FIFO count after flush:  0

=== Bias Estimation ===
Estimated gyro biases: gx=0.0000 gy=0.0000 gz=0.0000

=== Quaternion Operations ===
q1 * q2 = (0.4998, 0.5007, 0.4995, 0.4998)
From q:      Roll:     0.02   Pitch:    89.99   Yaw:    89.98
```

## Integration Examples

### Minimal IMU Read Loop

```c
#include "imu_sensor.h"
#include <stdio.h>

int main(void) {
    imu_sensor_t imu;
    imu_init(&imu, IMU_AHRS_MADGWICK, 200.0f);

    while (1) {
        imu_sample_t sample;

        /* Read from actual hardware sensors */
        read_accel_i2c(&sample.accel.ax, &sample.accel.ay, &sample.accel.az);
        read_gyro_i2c(&sample.gyro.gx, &sample.gyro.gy, &sample.gyro.gz);
        read_mag_i2c(&sample.mag.mx, &sample.mag.my, &sample.mag.mz);
        sample.timestamp_ms = get_system_time_ms();

        imu_update_all(&imu, &sample);

        printf("Roll:%.2f Pitch:%.2f Yaw:%.2f\n",
               imu.angles.roll, imu.angles.pitch, imu.angles.yaw);
    }
    return 0;
}
```

### Calibration Procedure

1. Place IMU stationary on a flat surface for ~5 seconds
2. Collect N samples (recommend 200-500 samples at 100Hz)
3. Compute gyroscope bias = mean of all gyro readings
4. Compute accelerometer bias = mean of accel readings, with Z axis adjusted for gravity
5. Set calibration with `imu_set_calibration()`

```c
imu_sample_t calib_samples[500];
uint16_t i;
for (i = 0; i < 500; i++) {
    /* Read sensor and fill sample */
    calib_samples[i] = read_sensor_sample();
}
imu_estimate_bias(&imu, calib_samples, 500);
```

### Motion Detection Usage

```c
/* Freefall detection (parachute release, crash detection) */
imu_motion_evt_t evt = imu_detect_freefall(&imu.accel, 0.5f);
if (evt == IMU_MOTION_FREEFALL) {
    deploy_parachute();
}

/* Tap detection (gesture input) */
imu_motion_evt_t tap = imu_detect_tap(&imu);
if (tap == IMU_MOTION_DOUBLE_TAP) {
    toggle_led();
}
```

## Performance Notes

### Computational Cost

| Operation | Approx Cycles (Cortex-M4F) | Notes |
|-----------|---------------------------|-------|
| Madgwick update | ~2500 | Full 9-axis fusion per sample |
| Mahony update | ~1800 | 9-axis fusion per sample |
| Madgwick (6-axis) | ~1200 | Accel + Gyro only |
| Mahony (6-axis) | ~800 | Accel + Gyro only |
| Quat to Euler | ~200 | Trigonometric conversion |
| FIFO push | ~50 | Circular buffer operation |

### Memory Usage

| Component | Size (bytes) | Notes |
|-----------|-------------|-------|
| imu_sensor_t | ~6240 | Includes FIFO buffer of 128 samples |
| imu_sample_t | 52 | Per-sample struct |
| imu_fifo_t | ~6144 | 128 × 48 bytes per sample |

### Tuning Tips

1. **Increase Beta** if the filter is slow to converge to true orientation
2. **Decrease Beta** if you experience high-frequency jitter in the output
3. **Use Madgwick** for highest accuracy with 9-axis sensors (recommended)
4. **Use Mahony** for lower CPU usage or when magnetometer is unavailable
5. For **drone applications**, use 200-400Hz sample rate with Madgwick beta=0.04
6. For **robot navigation**, 100Hz is sufficient with beta=0.1
7. **Recalibrate gyro bias** whenever temperature changes by >10°C

## Troubleshooting

| Symptom | Likely Cause | Solution |
|---------|-------------|----------|
| Yaw drifting rapidly | Uncalibrated gyro Z bias | Run bias estimation at startup |
| Pitch/Roll drifting | Gyro bias not zeroed | Place on flat surface, recalibrate |
| Yaw always 0 | No magnetometer data | Check sensor wiring or IMU model |
| Quaternion NaN | Numerical instability | Check for NaN in input, ensure normalize is called |
| Slow convergence | Beta too low | Increase beta to 0.2 - 0.5 |
| Jittery output | Beta too high | Decrease beta to 0.03 - 0.08 |

## References

- Madgwick, S. "An efficient orientation filter for inertial and inertial/magnetic sensor arrays", 2010
- Mahony, R. "Nonlinear Complementary Filters on the Special Orthogonal Group", 2008
- Kuipers, J.B. "Quaternions and Rotation Sequences", 1999
- IEEE Std 2700-2017 - Sensor Performance Parameter Definitions
