# Sensor Selection Guide — mini-sensor-actuator

Selecting the right sensors for your embedded robotics or IoT project.

## Quick Selection Matrix

| Application | Required Sensors | Recommended Model | Budget Option |
|-------------|-----------------|-------------------|---------------|
| Indoor robot nav | ToF + IMU + Encoder | VL53L1X + ICM-20948 | HC-SR04 + MPU-6050 |
| Outdoor robot | LiDAR + GPS + IMU | RPLIDAR A1 + NEO-M8N | TF-Luna + BN-220 |
| Drone flight | IMU + Baro + Mag + ToF | ICM-20948 + BMP390 | MPU-9250 + BMP280 |
| Weather station | Temp + Hum + Pressure + Wind | BME680 + anemometer | DHT22 + BMP180 |
| Air quality monitor | PM2.5 + Gas + Temp/Hum | PMS5003 + SGP30 + BME280 | PMS3003 + CCS811 |
| Gesture control | ToF multi-zone + IMU | VL53L5CX + ICM-20948 | APDS-9960 |
| Obstacle avoidance | Ultrasonic array + ToF | JSN-SR04T × 6 + VL53L1X | HC-SR04 × 3 |
| Camera robot | MIPI Camera + IMU | OV5640 + ICM-20948 | OV2640 + MPU-6050 |

## IMU Selection

### Key Specifications

| Parameter | What It Means | Good Value |
|-----------|--------------|------------|
| Gyro Full Scale | Max rotation rate measurable (±dps) | ±2000 dps for drones |
| Accel Full Scale | Max acceleration measurable (±g) | ±16g for general use |
| Gyro Noise Density | Random noise floor (mdps/√Hz) | <10 mdps/√Hz is excellent |
| Accel Noise Density | Random noise floor (µg/√Hz) | <100 µg/√Hz is excellent |
| Bias Instability | Long-term drift (deg/hr) | <10 deg/hr for navigation |
| Output Data Rate | Sample rate (Hz) | ≥200 Hz for drones |
| Interface | Communication protocol | I2C/SPI (SPI faster, I2C simpler) |

### Comparison Table

| IMU | Gyro Range | Accel Range | Mag? | Noise | Interface | Typical Use |
|-----|-----------|-------------|------|-------|-----------|-------------|
| ICM-20948 | ±2000 | ±16g | Yes | Low | I2C/SPI | Best all-around 9-DOF |
| MPU-9250 | ±2000 | ±16g | Yes | Medium | I2C/SPI | Good budget 9-DOF |
| MPU-6050 | ±2000 | ±16g | No | Medium | I2C | Budget 6-DOF (very common) |
| BNO055 | ±2000 | ±16g | Yes | Low | I2C/UART | Built-in fusion (easiest) |
| ICM-42688-P | ±4000 | ±32g | No | Ultra-Low | I2C/SPI | High-end 6-DOF |
| BMI270 | ±2000 | ±16g | No | Low | I2C/SPI | Low power, good for wearables |
| LSM6DSO32 | ±4000 | ±32g | No | Low | I2C/SPI | High-g vibration tolerant |
| ADIS16470 | ±2000 | ±40g | Yes | Ultra-Low | SPI | Industrial/professional |

### Selection Guidelines

1. **For drones**: ICM-20948 or ICM-42688-P. Need low noise and high ODR. Magnetometer helps with heading lock.
2. **For wheeled robots**: MPU-6050 or BMI270. 6-DOF sufficient, use wheel odometry for heading.
3. **For VR/AR**: BNO055 for low latency built-in fusion.
4. **For industrial**: ADIS16470 with temperature calibration and low bias instability.
5. **For wearables**: BMI270 for ultra-low power consumption.

## Distance Sensor Selection

### Technology Comparison

| Technology | Principle | Range | Pros | Cons |
|-----------|-----------|-------|------|------|
| ToF (IR laser) | Time of flight | 0.04-8m | Accurate, immune to color/texture | Sunlight interference, narrow FOV |
| LiDAR | Laser triangulation/ToF | 0.1-40m | 360° scanning, mapping | Expensive, mechanical wear |
| Ultrasonic | Sound echo | 0.02-4m | Cheap, works outdoors, waterproof options | Low resolution, slow, temperature dependent |
| Structured Light | IR pattern projection | 0.3-5m | Dense 3D, good for indoors | Sunlight blinds it, expensive |
| Stereo Vision | Dual camera parallax | 0.5-20m | Color + depth, passive | Computation heavy, needs texture |
| mmWave Radar | Radio wave reflection | 0.5-200m | Weather-immune, long range | Low resolution, expensive |

### Recommended Models by Range

| Range | ToF | LiDAR | Ultrasonic |
|-------|-----|-------|------------|
| 0-0.5m (close) | VL53L0X | N/A | HC-SR04 |
| 0.5-2m (short) | VL53L1X | N/A | JSN-SR04T |
| 2-8m (medium) | TF-Luna | RPLIDAR A1 | MB1240 |
| 8-20m (long) | TF02-Pro | YDLIDAR X4 | MB7360 |
| 20m+ (very long) | N/A | RPLIDAR S2 | MB1260 |

## Environmental Sensor Selection

### Temperature & Humidity

| Sensor | Temp Accuracy | Humidity Accuracy | Interface | Power | Notes |
|--------|--------------|-------------------|-----------|-------|-------|
| BME280 | ±1.0°C | ±3% | I2C/SPI | 3.6µA | Also has pressure |
| BME680 | ±1.0°C | ±3% | I2C/SPI | 3.7µA | + Gas (VOC) sensor |
| DHT22 | ±0.5°C | ±2% | 1-Wire | 1.5mA | Cheapest, slow (2s) |
| SHT31 | ±0.3°C | ±2% | I2C | 0.5µA | Industrial grade, excellent |
| SHT40 | ±0.2°C | ±1.8% | I2C | 0.4µA | Next-gen SHT, best accuracy |
| HDC1080 | ±0.4°C | ±2% | I2C | 0.7µA | Good budget accuracy |
| AHT20 | ±0.3°C | ±2% | I2C | 0.5µA | Good value alternative |

### Air Quality / Gas

| Sensor | Targets | Range | Interface | Warm-up | Notes |
|--------|---------|-------|-----------|---------|-------|
| SGP30 | VOC, eCO2 | 0-60000 ppb | I2C | 15s | Needs baseline, good for IAQ |
| SGP40 | VOC index | 0-500 | I2C | 60s | No calibration needed |
| CCS811 | eCO2, VOC | 400-8192 ppm | I2C | 20min | Needs burn-in, 48hr baseline |
| BME688 | VOC, gases | AI-based | I2C/SPI | 5min | AI gas scanning, unique fingerprinting |
| MQ-135 | NH3, NOx, CO2 | Analog | ADC | 24hr+ | Very cheap, high power, drift |
| ZMOD4410 | IAQ, VOC | 0-500 IAQ | I2C | 5min | Auto-baseline, industrial |

### Particulate Matter (PM)

| Sensor | PM1.0 | PM2.5 | PM10 | Interface | Fan Life | Notes |
|--------|-------|-------|------|-----------|----------|-------|
| PMS5003 | Yes | Yes | Yes | UART | 3 years | Gold standard, widely used |
| PMS7003 | Yes | Yes | Yes | UART | 3 years | Compact version of PMS5003 |
| PMS3003 | No | Yes | Yes | UART | 3 years | Budget version |
| SDS011 | No | Yes | Yes | UART | 8000h | Good budget option |
| SPS30 | Yes | Yes | Yes | I2C/UART | 10 years | Industrial, auto-cleaning, best |

### Pressure / Altitude

| Sensor | Accuracy | Noise | Interface | Best For |
|--------|----------|-------|-----------|----------|
| BMP390 | ±3 Pa (25cm) | 0.5 Pa | I2C/SPI | High precision altitude |
| BMP280 | ±12 Pa (1m) | 1.3 Pa | I2C/SPI | General use |
| MS5611 | ±10 Pa (80cm) | 1.2 Pa | I2C/SPI | Drone altitude hold |
| LPS22HB | ±10 Pa | 0.75 Pa | I2C/SPI | Waterproof package option |
| DPS310 | ±6 Pa (50cm) | 0.5 Pa | I2C/SPI | High-speed pressure sensing |

## Power Budget Planning

### Typical Sensor Power Consumption

| Sensor | Active | Sleep | Notes |
|--------|--------|-------|-------|
| IMU (ICM-20948) | 2.5 mA | 8 µA | 1.8V core, 3.3V I/O |
| ToF (VL53L1X) | 20 mA | 5 µA | Includes VCSEL driver |
| BME280 | 3.6 µA | 0.1 µA | Ultra-low duty cycle possible |
| PMS5003 | 100 mA | <200 µA | Fan draws most power |
| RPLIDAR A1 | 350 mA | N/A | Motor + laser spinning |
| Camera (OV2640) | 125 mA | 20 µA | Active streaming power |
| GPS (NEO-M8N) | 67 mA | 8 µA | Cold start: 10-30s |
| WiFi (ESP32) | 130 mA | 5 µA | TX at 802.11n |

### Power Saving Strategies

1. **Duty cycling**: Sleep sensors between readings. Example: read BME280 at 1Hz = 3.6µA average.
2. **FIFO burst reading**: Let IMU fill FIFO at high rate, read in bursts.
3. **Adaptive sampling**: Reduce frequency when values are stable.
4. **Sensor gating**: Power off unused sensors (e.g., GPS indoors).
5. **Interrupt-driven**: Use sensor interrupt pins instead of polling.

## Interface Considerations

### I2C Bus Planning

- Maximum devices on bus: 127 (practical limit ~8-10 due to capacitance)
- Check for address conflicts. Common conflicts:
  - BH1750 and BME280 both default to 0x76
  - VL53L0X and VL53L1X both default to 0x29
- Use an I2C multiplexer (TCA9548A) for >4 sensors
- Keep bus speed appropriate: 100kHz for long runs, 400kHz for short

### SPI Bus Planning

- Each device needs a unique CS (chip select) pin
- Faster than I2C (up to 10+ MHz), better for IMUs and displays
- Shared MOSI/MISO/SCLK lines, individual CS pins
- Limited by available GPIOs

### UART Planning

- Each sensor needs a dedicated UART (or shared if sensor supports address mode)
- SoftwareSerial can be used but is less reliable at high baud rates
- Many PM/PMS particle sensors use UART at 9600 bps

## Mechanical Integration

### Mounting Considerations

1. **IMU**: Mount at center of mass. Use vibration-damping standoffs. Align sensor axes with robot axes.
2. **ToF/LiDAR**: Mount with clear optical path. Consider protective glass (AR-coated for IR).
3. **Camera**: Rigid mount to avoid vibration blur. Consider OIS/active alignment.
4. **Ultrasonic**: Avoid mounting near edges (ringing). Use acoustic foam for isolation.
5. **Pressure sensor**: Avoid direct airflow. Use a small enclosure with a pinhole vent.
6. **GPS**: Clear sky view. Keep away from EMI sources (motors, ESCs).

### Environmental Protection

- **Indoor**: Basic dust cover
- **Outdoor**: IP65 or better. Conformal coating on PCBs.
- **Underwater**: IP68, pressure-compensated housing
- **High vibration**: Thread-locking compound on fasteners, strain relief on wires
