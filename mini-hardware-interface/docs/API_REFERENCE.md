# mini-hardware-interface API 参考手册

## 模块索引

1. [GPIO / PWM / ADC — gpio_pwm_adc.h](#1-gpio--pwm--adc)
2. [传感器轮询 — sensor_polling.h](#2-传感器轮询)
3. [PID 控制器 — pid_controller.h](#3-pid-控制器)
4. [执行器驱动 — actuator_driver.h](#4-执行器驱动)
5. [信号调理 — signal_cond.h](#5-信号调理)

---

## 1. GPIO / PWM / ADC

### GPIO

| 函数 | 说明 |
|------|------|
| `mhi_gpio_init(pin, mode)` | 初始化引脚，设置模式 (INPUT/OUTPUT/INPUT_PULLUP 等) |
| `mhi_gpio_deinit(pin)` | 释放引脚资源 |
| `mhi_gpio_write(pin, level)` | 数字输出: HIGH 或 LOW |
| `mhi_gpio_read(pin)` | 数字输入: 返回 HIGH 或 LOW |
| `mhi_gpio_toggle(pin)` | 翻转输出电平 |
| `mhi_gpio_set_irq(pin, trigger, cb, user_data)` | 设置 GPIO 中断 (RISING/FALLING/BOTH) |
| `mhi_gpio_clear_irq(pin)` | 清除中断配置 |

**枚举:**
- `mhi_gpio_mode_t`: INPUT, OUTPUT, INPUT_PULLUP, INPUT_PULLDOWN, OPEN_DRAIN
- `mhi_gpio_level_t`: LOW, HIGH
- `mhi_gpio_irq_t`: NONE, RISING, FALLING, BOTH

### PWM

| 函数 | 说明 |
|------|------|
| `mhi_pwm_init(ch, freq, res)` | 初始化 PWM 通道，频率 + 分辨率 (8/16 bit) |
| `mhi_pwm_deinit(ch)` | 释放 PWM 通道 |
| `mhi_pwm_set_duty(ch, percent)` | 设置占空比 (0.0–100.0%) |
| `mhi_pwm_set_duty_raw(ch, raw)` | 原始值设置占空比 |
| `mhi_pwm_get_duty(ch)` | 获取当前占空比百分比 |
| `mhi_pwm_set_frequency(ch, freq)` | 修改 PWM 频率 |
| `mhi_pwm_get_frequency(ch)` | 获取当前频率 |
| `mhi_pwm_start(ch)` / `mhi_pwm_stop(ch)` | 启动/停止 PWM 输出 |
| `mhi_pwm_is_running(ch)` | 查询运行状态 |

### 舵机

| 函数 | 说明 |
|------|------|
| `mhi_servo_init(cfg)` | 初始化舵机 (pwm_ch, 脉宽范围, 角度范围, 周期) |
| `mhi_servo_deinit(ch)` | 释放舵机 |
| `mhi_servo_set_angle(ch, deg)` | 设置角度 |
| `mhi_servo_get_angle(ch)` | 获取当前角度 |
| `mhi_servo_calibrate(ch, min_us, max_us, min_deg, max_deg)` | 重新标定 |

默认: 50Hz, 500us→0°, 2500us→180°

### ADC

| 函数 | 说明 |
|------|------|
| `mhi_adc_init(ch, res, vref)` | 初始化 ADC 通道, 分辨率 (10/12/16bit), 参考电压 |
| `mhi_adc_deinit(ch)` | 释放 ADC 通道 |
| `mhi_adc_read_single(ch, *raw, *voltage)` | 单次采样 |
| `mhi_adc_read_continuous_start(ch, interval_ms, cb, user_data)` | 启动连续采样 |
| `mhi_adc_read_continuous_stop(ch)` | 停止连续采样 |
| `mhi_adc_read_differential(ch+, ch-, gain, *raw, *voltage)` | 差分采样 |
| `mhi_adc_raw_to_voltage(raw, vref, max_raw)` | 原始值→电压 |
| `mhi_adc_voltage_to_raw(voltage, vref, max_raw)` | 电压→原始值 |

辅助宏: `mhi_map_float(x, in_min, in_max, out_min, out_max)` — 线性映射

---

## 2. 传感器轮询

### 总线抽象

| 函数 | 说明 |
|------|------|
| `mhi_bus_init(cfg)` | 初始化 I2C/SPI 总线 |
| `mhi_bus_deinit()` | 释放总线 |
| `mhi_bus_write_reg(reg, data, len)` | 写寄存器 |
| `mhi_bus_read_reg(reg, data, len)` | 读寄存器 |
| `mhi_bus_write_read(tx, tx_len, rx, rx_len)` | SPI 全双工传输 |

### 传感器描述符

`mhi_sensor_t` 包含: id, type, name, bus, ctx, init/read 函数指针, poll_interval_ms, raw/filtered values

### 轮询定时器

| 函数 | 说明 |
|------|------|
| `mhi_poll_timer_init(timer, interval_ms, cb, user_data, auto_reload)` | 初始化定时器 |
| `mhi_poll_timer_start(timer)` | 启动 |
| `mhi_poll_timer_stop(timer)` | 停止 |
| `mhi_poll_timer_tick(timer, now_ms)` | 主循环中调用, 到达间隔时触发回调 |

### 数据平滑

**移动平均:**
- `mhi_ma_init(ma, buffer, size)`
- `mhi_ma_update(ma, value)` → 滤波后值
- `mhi_ma_reset(ma)`

**中值滤波:**
- `mhi_median_init(mf, buffer, sorted, window)`
- `mhi_median_update(mf, value)` → 中值
- `mhi_median_reset(mf)`

**卡尔曼 1D:**
- `mhi_kalman_1d_init(kf, init_value, process_noise, measurement_noise)`
- `mhi_kalman_1d_update(kf, measurement)` → 状态估计
- `mhi_kalman_1d_reset(kf, value)`

### 标定

- `mhi_calibration_init(cal)`
- `mhi_calibration_set_zero(cal, raw_val, ref_val)` — 设置零点
- `mhi_calibration_set_span(cal, raw_val, ref_val)` — 设置量程
- `mhi_calibration_apply(cal, raw)` → 标定后的值

### 轮询管理

- `mhi_sensor_manager_init(mgr, sensors, count)`
- `mhi_sensor_manager_service(mgr, now_ms)` — 轮询一个到期传感器
- `mhi_sensor_manager_read_current(mgr)` — 强制读取当前传感器

---

## 3. PID 控制器

### 单回路 PID

| 函数 | 说明 |
|------|------|
| `mhi_pid_init(pid, config)` | 初始化 PID |
| `mhi_pid_reset(pid)` | 重置积分/历史 |
| `mhi_pid_compute(pid, setpoint, measurement, now_ms)` | 计算控制输出 |
| `mhi_pid_set_gains(pid, kp, ki, kd)` | 在线修改增益 |
| `mhi_pid_set_output_limits(pid, min, max)` | 输出限幅 |
| `mhi_pid_set_feed_forward(pid, ff)` | 设置前馈项 |
| `mhi_pid_get_output(pid)` | 获取当前输出 |
| `mhi_pid_get_p_term / i_term / d_term(pid)` | 获取各分量 |
| `mhi_pid_is_saturated(pid)` | 查询输出饱和状态 |

**配置结构:**
- `mhi_pid_config_t`: kp, ki, kd, setpoint, sample_time_s, output_min/max
- `form`: POSITIONAL / VELOCITY
- `anti_windup`: NONE / CLAMPING / BACK_CALC / BOTH
- `direction`: DIRECT / REVERSE
- `back_calc_gain`: 反算增益 Kb
- `derivative_filter_a`: D 项低通滤波系数 (0–1)

### Ziegler-Nichols 自整定

`mhi_zn_tune(type, ku, tu, result)`

| Type | Kp | Ki | Kd |
|------|----|----|-----|
| P | 0.50 Ku | 0 | 0 |
| PI | 0.45 Ku | 0.54 Ku/Tu | 0 |
| PID | 0.60 Ku | 1.20 Ku/Tu | 0.075 Ku·Tu |

### 级联 PID

| 函数 | 说明 |
|------|------|
| `mhi_cascaded_pid_init(cpid, outer_cfg, inner_cfg)` | 初始化双回路 |
| `mhi_cascaded_pid_reset(cpid)` | 重置 |
| `mhi_cascaded_pid_compute(cpid, outer_sp, outer_meas, inner_meas, now_ms)` | 计算 |
| `mhi_cascaded_pid_set_output_limits(cpid, min, max)` | 输出限幅 |

外环输出 → 内环设定值

---

## 4. 执行器驱动

### 直流电机 (H 桥)

| 函数 | 说明 |
|------|------|
| `mhi_dc_motor_init(motor, pwm_ch, in1, in2)` | 初始化 |
| `mhi_dc_motor_set_speed(motor, speed)` | 设置速度 (-1.0 ~ 1.0) |
| `mhi_dc_motor_set_direction(motor, dir)` | CW/CCW/BRAKE/COAST |
| `mhi_dc_motor_brake(motor)` | 短路制动 |
| `mhi_dc_motor_coast(motor)` | 惯性滑行 |
| `mhi_dc_motor_emergency_stop(motor)` | 急停 |
| `mhi_dc_motor_resume(motor)` | 恢复 |
| `mhi_dc_motor_tick(motor, now_ms)` | 软启动斜坡更新 |

### 无刷直流电机 (BLDC)

| 函数 | 说明 |
|------|------|
| `mhi_bldc_motor_init(motor, pins)` | 初始化 (6 PWM + 3 Hall) |
| `mhi_bldc_motor_set_speed(motor, speed)` | 设置目标速度 |
| `mhi_bldc_motor_set_duty(motor, duty)` | 设置 PWM 占空比 |
| `mhi_bldc_motor_commutate(motor)` | 执行换相 (6-step) |
| `mhi_bldc_motor_emergency_stop(motor)` | 急停 |
| `mhi_bldc_motor_tick(motor, now_ms)` | 更新 |

### 步进电机

| 函数 | 说明 |
|------|------|
| `mhi_stepper_motor_init(motor, pins, steps_per_rev)` | 初始化 |
| `mhi_stepper_motor_set_mode(motor, mode)` | FULL/HALF/MICRO_4/8/16/32 |
| `mhi_stepper_motor_move_to(motor, position)` | 绝对定位 |
| `mhi_stepper_motor_move_steps(motor, steps)` | 相对位移 |
| `mhi_stepper_motor_set_speed(motor, steps_per_s)` | 速度设置 |
| `mhi_stepper_motor_emergency_stop(motor)` | 急停 |
| `mhi_stepper_motor_tick(motor, now_us)` | 更新 (微秒) |

### 舵机

| 函数 | 说明 |
|------|------|
| `mhi_servo_motor_init(servo, pwm_ch)` | 初始化 |
| `mhi_servo_motor_set_angle(servo, deg)` | 设置角度 |
| `mhi_servo_motor_get_angle(servo)` | 获取角度 |
| `mhi_servo_motor_calibrate(servo, min_us, max_us, min_deg, max_deg)` | 标定 |
| `mhi_servo_motor_disable(servo)` | 禁用 |

### 全局急停

`mhi_emergency_stop_all()` / `mhi_emergency_resume_all()`

---

## 5. 信号调理

### 运算放大器

| 函数 | 说明 |
|------|------|
| `mhi_opamp_init(opamp, cfg, vcc, vee)` | 初始化 |
| `mhi_opamp_inverting(opamp, vin)` | 反相放大 |
| `mhi_opamp_non_inverting(opamp, vin)` | 同相放大 |
| `mhi_opamp_differential(opamp, vp, vn)` | 差分放大 |
| `mhi_opamp_instrumentation(opamp, vp, vn)` | 仪表放大器 |
| `mhi_opamp_compute(opamp, vp, vn)` | 按拓扑自动计算 |
| `mhi_opamp_clamp(opamp, vout)` | 饱和限幅 |

### RC 滤波器

| 函数 | 说明 |
|------|------|
| `mhi_rc_filter_init(f, type, r, c, sample_time_s)` | 初始化 |
| `mhi_rc_filter_update(f, input)` → output | 迭代更新 |
| `mhi_rc_filter_reset(f)` | 重置 |
| `mhi_rc_filter_cutoff_freq(f)` | 截止频率 fc = 1/(2πRC) |
| `mhi_rc_design_c(cutoff_hz, r_ohm)` | 由 R 计算 C |
| `mhi_rc_design_r(cutoff_hz, c_farad)` | 由 C 计算 R |

### 二阶有源滤波器 (Sallen-Key)

- `mhi_filter_2nd_init(f, type, cutoff_hz, q_factor, sample_hz)`
- `mhi_filter_2nd_update(f, input)` → output
- `mhi_filter_2nd_reset(f)`

Q = 0.707 对应 Butterworth 响应

### 噪声滤波

- `mhi_noise_simple_average(buffer, len)` → 简单均值
- `mhi_noise_exponential(current, prev, alpha)` → 指数平滑
- `mhi_noise_spike_removal(value, prev, max_delta)` → 尖峰抑制
- `mhi_noise_threshold_deadband(value, prev, threshold)` → 死区

### 电平转换

- `mhi_level_shift_init(ls, r1, r2, vref)`
- `mhi_level_shift_apply(ls, vin)` → Vout
- `mhi_level_shift_design(ls, vin_min, vin_max, vout_min, vout_max, vref)` — 自动设计 R1/R2

### 惠斯通电桥

- `mhi_wheatstone_init(wb, config, v_excitation, gauge_factor, nominal_r)`
- `mhi_wheatstone_output_voltage(wb, strain)` → Vout
- `mhi_wheatstone_strain_from_voltage(wb, v_out)` → strain (ε)
- `mhi_wheatstone_resistance_change(wb, strain)` → ΔR

支持 1/4, 1/2, 全桥

### 光电耦合器

- `mhi_optocoupler_init(oc, ctr, led_vf, led_if_max_ma, vcc, r_pullup)`
- `mhi_optocoupler_led_resistor(vin, desired_if_ma, led_vf)` → R_led
- `mhi_optocoupler_output_voltage(oc, led_if_ma)` → Vout
- `mhi_optocoupler_output_logic(oc, led_if_ma, vih_threshold)` → bool

### 抗混叠滤波器设计

- `mhi_anti_alias_design(aa, sample_rate_hz, cutoff_hz, order)`
- `mhi_anti_alias_attenuation(aa, freq_hz)` → dB
- `mhi_anti_alias_design_rc(aa, r_ohm)` — 固定 R 计算 C

---

## 常量 / 辅助

- `MHI_MAX_PINS` = 64
- `MHI_MAX_PWM_CH` = 16
- `MHI_MAX_ADC_CH` = 16
- `mhi_map_float(x, in_min, in_max, out_min, out_max)` — 线性插值映射

## 数据类型一览

| 类型 | 用途 |
|------|------|
| `mhi_gpio_mode_t` | 引脚模式 |
| `mhi_gpio_level_t` | 数字电平 |
| `mhi_pwm_config_t` | PWM 配置 |
| `mhi_servo_config_t` | 舵机配置 |
| `mhi_adc_config_t` | ADC 配置 |
| `mhi_bus_config_t` | I2C/SPI 总线配置 |
| `mhi_sensor_t` | 传感器描述符 |
| `mhi_moving_average_t` | 移动平均滤波器状态 |
| `mhi_median_filter_t` | 中值滤波器状态 |
| `mhi_kalman_1d_t` | 一维卡尔曼滤波器状态 |
| `mhi_calibration_t` | 标定参数 |
| `mhi_pid_config_t` | PID 配置 |
| `mhi_pid_t` | PID 状态 |
| `mhi_cascaded_pid_t` | 级联 PID |
| `mhi_zn_result_t` | Ziegler-Nichols 结果 |
| `mhi_dc_motor_t` | 直流电机 |
| `mhi_bldc_motor_t` | 无刷直流电机 |
| `mhi_stepper_motor_t` | 步进电机 |
| `mhi_servo_motor_t` | 舵机 |
| `mhi_opamp_t` | 运放仿真 |
| `mhi_rc_filter_t` | RC 滤波器 |
| `mhi_filter_2nd_t` | 二阶有源滤波器 |
| `mhi_level_shift_t` | 电平转换 |
| `mhi_wheatstone_t` | 惠斯通电桥 |
| `mhi_optocoupler_t` | 光耦 |
| `mhi_anti_alias_t` | 抗混叠滤波器 |
