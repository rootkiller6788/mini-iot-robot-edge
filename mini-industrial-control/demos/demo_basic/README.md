# Demo Basic: 基础工业控制演示

## 概述

本演示展示 mini-industrial-control 库的基本功能，包括：
- PLC 梯形图逻辑（Ladder Logic）编程
- Modbus 通讯协议（RTU/TCP）
- OPC UA 地址空间模拟
- SCADA 数据采集与报警
- 安全 PLC 功能

## 目录结构

```
mini-industrial-control/
├── include/
│   ├── plc_ladder.h       # PLC 梯形图逻辑 API
│   ├── modbus_proto.h     # Modbus 协议 API
│   ├── opc_ua_sim.h       # OPC UA 模拟 API
│   ├── scada_collect.h    # SCADA 采集 API
│   └── safety_plc.h       # 安全 PLC API
├── src/
│   ├── plc_ladder.c       # PLC 梯形图实现
│   ├── modbus_proto.c     # Modbus 协议实现
│   ├── opc_ua_sim.c       # OPC UA 模拟实现
│   ├── scada_collect.c    # SCADA 采集实现
│   └── safety_plc.c       # 安全 PLC 实现
├── examples/
│   ├── example_plc.c
│   ├── example_modbus.c
│   └── example_scada.c
├── Makefile
└── README.md
```

## 构建与运行

### 环境要求

- GCC (支持 C99 标准)
- GNU Make (或兼容的 make 工具)
- Windows / Linux / macOS

### 编译

```bash
# 编译所有库和示例
make all

# 仅编译库
make

# 清理构建文件
make clean
```

### 运行示例

```bash
build\example_plc.exe
build\example_modbus.exe
build\example_scada.exe
```

## 模块说明

### 1. PLC 梯形图逻辑 (`plc_ladder.h`)

实现简化的 IEC 61131-3 梯形图编程模型。

#### 核心概念

**扫描周期 (Scan Cycle):**
```
Inputs Read → Program Execute → Outputs Write → Housekeeping
```

每个扫描周期按顺序：
1. **读入阶段**: 读取所有物理输入状态
2. **程序执行**: 按顺序评估所有梯级 (rung)
3. **写出阶段**: 将计算结果写入物理输出
4. **内务处理**: 更新诊断、通讯等

**梯级结构 (Rung Structure):**
```
左母线 ──[触点1]──[触点2]──...──[线圈1]──[线圈2]──...── 右母线
```

每个梯级从左到右评估：
- 多个触点串联 (AND 逻辑)
- 功率流传递到线圈
- 线圈根据类型设置输出状态

**触点类型 (Contact Types):**

| 类型 | 符号 | 说明 |
|------|------|------|
| NO (Normally Open) | -| |- | 地址为 TRUE 时闭合 |
| NC (Normally Closed) | -|/|- | 地址为 FALSE 时闭合 |

- NO 触点: `evaluate() = input_state` (导通当输入为真)
- NC 触点: `evaluate() = !input_state` (导通当输入为假)

**线圈类型 (Coil Types):**

| 类型 | 说明 |
|------|------|
| OUTPUT | 普通输出，跟随功率流 |
| INTERNAL | 内部继电器，用于中间逻辑 |
| LATCH_SET | 置位线圈，功率流来时锁存为 ON |
| LATCH_RESET | 复位线圈，功率流来时锁存为 OFF |

**定时器 (Timers):**

| 类型 | 说明 | 时序 |
|------|------|------|
| TON | 接通延时 | 输入 ON → 延时 → 输出 ON |
| TOF | 断开延时 | 输入 OFF → 延时 → 输出 OFF |
| TP | 脉冲定时器 | 输入上升沿 → 固定脉宽输出 |

TON (Timer On-Delay):
```
Input:   ___|------------------|_________
                  ← preset →
Output:  ________|----------|_____________
```

TOF (Timer Off-Delay):
```
Input:   ___|------------------|_________
                            ← preset →
Output:  ___|------------------|-----|___
```

**计数器 (Counters):**

| 类型 | 说明 |
|------|------|
| CTU | 增计数器，到达预设值时 done=TRUE |
| CTD | 减计数器，减到 0 时 done=TRUE |
| CTUD | 增减计数器，可双向计数 |

#### 使用示例

```c
#include "plc_ladder.h"

int main() {
    plc_program_t plc;
    plc_init(&plc);

    // 添加梯级: 启动/停止电路
    plc_rung_t *r0 = plc_add_rung(&plc);
    plc_add_contact(r0, "Start", PLC_CONTACT_NO, 0);
    plc_add_contact(r0, "Stop", PLC_CONTACT_NC, 1);
    plc_add_coil(r0, "Motor", PLC_COIL_OUTPUT, 100);

    // 模拟输入
    plc_write_input(&plc, 0, true);   // 按下 Start
    plc_write_input(&plc, 1, false);  // Stop 未按下

    // 执行扫描
    plc_scan_cycle(&plc);

    // 读取输出
    bool motor_on = plc_read_output(&plc, 100);
    printf("Motor: %s\n", motor_on ? "ON" : "OFF");

    return 0;
}
```

### 2. Modbus 通讯协议 (`modbus_proto.h`)

实现 Modbus 应用层协议，支持 RTU 和 TCP 两种传输模式。

#### 帧格式

**RTU 帧:**
```
+---------+---------+------+------+
| SlaveID | FuncCode| Data | CRC  |
+---------+---------+------+------+
|   1 B   |   1 B   | N B  | 2 B  |
+---------+---------+------+------+
```

**TCP 帧 (MBAP 头):**
```
+--------+--------+------+--------+---------+------+
| TransID| ProtoID| Length| UnitID | FuncCode| Data |
+--------+--------+------+--------+---------+------+
|  2 B   |  2 B   | 2 B  |  1 B   |   1 B   | N B  |
+--------+--------+------+--------+---------+------+
```

**MBAP 头字段说明:**
- Transaction ID: 事务标识符，请求与响应配对
- Protocol ID: 协议标识符，Modbus=0x0000
- Length: 后续字节数 (UnitID + FuncCode + Data)
- Unit ID: 单元标识符 (相当于 RTU 的从站地址)

#### 功能码

| 功能码 | 名称 | 说明 |
|--------|------|------|
| 0x01 | Read Coils | 读取线圈状态 |
| 0x02 | Read Discrete Inputs | 读取离散输入 |
| 0x03 | Read Holding Registers | 读取保持寄存器 |
| 0x04 | Read Input Registers | 读取输入寄存器 |
| 0x05 | Write Single Coil | 写单个线圈 |
| 0x06 | Write Single Register | 写单个寄存器 |
| 0x0F | Write Multiple Coils | 写多个线圈 |
| 0x10 | Write Multiple Registers | 写多个寄存器 |

#### 数据模型

| 数据类型 | 地址范围 | 访问方式 | 大小 |
|----------|----------|----------|------|
| Coils | 00001-09999 | 读写 | 1 bit |
| Discrete Inputs | 10001-19999 | 只读 | 1 bit |
| Input Registers | 30001-39999 | 只读 | 16 bit |
| Holding Registers | 40001-49999 | 读写 | 16 bit |

#### 异常码

| 代码 | 名称 | 说明 |
|------|------|------|
| 0x01 | Illegal Function | 功能码不支持 |
| 0x02 | Illegal Data Address | 数据地址无效 |
| 0x03 | Illegal Data Value | 数据值无效 |
| 0x04 | Slave Device Failure | 从站设备故障 |
| 0x05 | Acknowledge | 确认（处理中） |
| 0x06 | Slave Device Busy | 从站设备忙 |

#### CRC16 计算

Modbus RTU 使用 CRC-16 校验，多项式为 0xA001。
算法采用查表法实现，在每个 RTU 帧末尾附加 2 字节 CRC。

#### 使用示例

```c
#include "modbus_proto.h"

int main() {
    modbus_device_t dev;
    modbus_init_device(&dev, MODBUS_RTU, 1);

    // 写入保持寄存器
    modbus_write_single_register(&dev.data_model, 40001, 12345);

    // 读取
    uint16_t value;
    modbus_read_holding_register(&dev.data_model, 40001, &value);
    printf("HR[40001] = %u\n", value);

    // 构建 RTU 请求
    modbus_request_t req = {1, MODBUS_FC_READ_HOLDING_REGISTERS, 0, 10};
    uint8_t buffer[256];
    int len = modbus_build_request_rtu(buffer, sizeof(buffer), &req);

    // CRC 校验
    printf("CRC OK: %s\n", modbus_verify_crc(buffer, len) ? "YES" : "NO");

    return 0;
}
```

### 3. OPC UA 地址空间模拟 (`opc_ua_sim.h`)

模拟 OPC UA (Unified Architecture) 的基本信息模型。

#### 节点类 (Node Classes)

| 类 | 说明 |
|------|------|
| Object | 对象节点，组织层次结构 |
| Variable | 变量节点，存储数据值 |
| Method | 方法节点，可调用操作 |
| View | 视图节点，提供数据子集 |
| DataType | 数据类型节点 |

#### 数据类型

| 类型 | C 语言对应 | 说明 |
|------|-----------|------|
| Boolean | bool | 布尔值 |
| SByte | int8_t | 有符号字节 |
| Byte | uint8_t | 无符号字节 |
| Int16 | int16_t | 16位有符号整数 |
| UInt16 | uint16_t | 16位无符号整数 |
| Int32 | int32_t | 32位有符号整数 |
| UInt32 | uint32_t | 32位无符号整数 |
| Int64 | int64_t | 64位有符号整数 |
| UInt64 | uint64_t | 64位无符号整数 |
| Float | float | 32位浮点数 |
| Double | double | 64位浮点数 |
| String | char[] | 字符串 |

#### 订阅机制

OPC UA 订阅-发布模型：
1. 客户端创建订阅（指定发布间隔）
2. 在订阅中添加监视项（Monitored Items）
3. 每个监视项关联一个变量节点
4. 服务器检测数据变化，触发通知
5. 按发布间隔向客户端发送数据变化通知

#### 使用示例

```c
#include "opc_ua_sim.h"

int main() {
    opcua_address_space_t as;
    opcua_init_address_space(&as);

    // 创建对象层次
    uint32_t device_id = opcua_add_object_node(&as, "Device1", 1);

    // 添加变量
    uint32_t var_id = opcua_add_variable_node(&as, "Temperature",
        device_id, OPCUA_TYPE_DOUBLE, OPCUA_ACCESS_RW);

    // 写入值
    opcua_set_double_value(&as, var_id, 25.5);

    // 读取值
    double value;
    opcua_get_double_value(&as, var_id, &value);
    printf("Temperature: %.1f\n", value);

    // 浏览子节点
    opcua_node_id_t children[16];
    int count = opcua_browse_node(&as, device_id, children, 16);
    printf("Children: %d\n", count);

    return 0;
}
```

### 4. SCADA 数据采集 (`scada_collect.h`)

实现 SCADA (Supervisory Control and Data Acquisition) 的基本数据采集功能。

#### 标签配置

每个标签包含：
- **名称**: 唯一标识符 (e.g., "Tank1.Temperature")
- **类型**: Discrete, Analog, Counter, String
- **设备地址**: Modbus 从站地址
- **寄存器地址**: Modbus 寄存器偏移
- **扫描速率**: 采集间隔 (ms)
- **死区 (Deadband)**: 值变化超过此阈值才记录
- **量程变换**: scale_factor × raw_value + offset

#### 历史数据 (Historian)

环形缓冲区存储时序数据：
```
{ timestamp_ms, value, quality, tag_index }
```

支持时间范围查询，按标签和起止时间过滤。

#### 报警管理

**报警级别:**
| 级别 | 说明 |
|------|------|
| LoLo | 超低报警（紧急） |
| Lo | 低报警 |
| Hi | 高报警 |
| HiHi | 超高报警（紧急） |
| ROC | 变化率报警 |
| Deviation | 偏差报警 |

**报警状态机:**
```
NORMAL ──(value>limit)──→ ACTIVE ──(ack)──→ ACKED
   ↑                          │                   │
   └──(value<limit)── RETURNED ←──────────────────┘
                         │
                    (value>limit again)
                         ↓
                      ACTIVE
```

**滞后 (Hysteresis):** 防止报警抖动，例如：
- Hi 报警限值: 80°C
- 滞后: 2°C
- 激活: > 80°C
- 复位: < 78°C

#### 日报表

| 字段 | 说明 |
|------|------|
| daily_total | 日总产量 |
| daily_average | 日平均值 |
| daily_min/max | 日极值 |
| hourly_rate | 小时产出率 |
| efficiency | 效率百分比 |
| downtime_minutes | 停机时间 |
| runtime_minutes | 运行时间 |
| OEE | 设备综合效率 |
| good_parts | 合格品数 |
| bad_parts | 不合格品数 |

### 5. 安全 PLC (`safety_plc.h`)

实现符合 IEC 61508/IEC 62061 标准的安全功能。

#### SIL 等级

| SIL | PFD (低要求模式) | PFH (高要求/连续模式) | 风险降低因子 |
|-----|------------------|-----------------------|-------------|
| SIL1 | 10⁻² ≤ PFD < 10⁻¹ | 10⁻⁶ ≤ PFH < 10⁻⁵ | 10-100 |
| SIL2 | 10⁻³ ≤ PFD < 10⁻² | 10⁻⁷ ≤ PFH < 10⁻⁶ | 100-1000 |
| SIL3 | 10⁻⁴ ≤ PFD < 10⁻³ | 10⁻⁸ ≤ PFH < 10⁻⁷ | 1000-10000 |
| SIL4 | 10⁻⁵ ≤ PFD < 10⁻⁴ | 10⁻⁹ ≤ PFH < 10⁻⁸ | 10000-100000 |

#### 安全功能

| 功能 | 说明 |
|------|------|
| E-Stop | 紧急停止，立即切断所有输出 |
| Door Interlock | 安全门联锁，开门时停机 |
| Light Curtain | 安全光幕，遮挡时停机 |
| Two-Hand Control | 双手控制，防止单手误操作 |
| Zero Speed | 零速监测，确认停止后允许操作 |
| Overspeed | 超速保护，超出限值时停机 |
| Safe Torque Off | 安全转矩关断 (STO) |
| Safe Position | 安全位置监测 |

#### 双通道冗余

```
Channel A ──→ [Input A] ──→                    ──→ Safe State
                            ⟩ Discrepancy Check ⟩
Channel B ──→ [Input B] ──→                    ──→ Diagnostic
```

**差异检查 (Discrepancy Check):**
- 两个通道的输入应在指定时间内一致
- 如果差异超过容许时间，触发故障
- 容许差异时间通常为 100-500ms

#### 双手控制

状态机：
```
IDLE ──→ LEFT_PRESSED ──→ BOTH_PRESSED ──→ OUTPUT=ON
                (within max_time_ms)    (both held)
              若超时则回到 IDLE
释放任意按钮 → OUTPUT=OFF → IDLE
```

最大允许时间间隔通常为 500ms。

#### 安全通讯

| 协议 | 说明 |
|------|------|
| PROFIsafe | PROFINET 的安全层 |
| CIP Safety | EtherNet/IP 的安全扩展 |
| FailSafe I/O | 故障安全 I/O 系统 |
| Safety over EtherCAT (FSoE) | EtherCAT 安全协议 |

---

## 典型应用场景

### 场景 1: 水泵控制

```
Requirements:
- 液位 > 80%: 启动泵
- 液位 < 20%: 停止泵
- 紧急停止按钮
- Modbus 通讯到 SCADA
```

### 场景 2: 输送带控制

```
Requirements:
- 启动/停止按钮
- 过载保护 (电流 > 额定值 × 1.2 → 停机)
- 速度编码器反馈
- 安全门联锁
- OEE 计算与日报表
```

### 场景 3: 温度控制系统

```
Requirements:
- PID 回路控制加热器
- 高/超高温度报警
- 历史趋势记录
- OPC UA 发布温度值
- SIL2 安全等级
```

---

## 参考资料

- IEC 61131-3: PLC 编程语言标准
- Modbus Application Protocol Specification V1.1b3
- OPC UA Specification Part 1-14 (IEC 62541)
- IEC 61508: 功能安全基本标准
- IEC 62061: 机械安全 - 电气安全相关控制系统
- ISO 13849-1: 机械安全 - 控制系统安全相关部件
