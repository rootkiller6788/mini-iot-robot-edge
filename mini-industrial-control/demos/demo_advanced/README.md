# Demo Advanced: 高级工业控制集成演示

## 概述

本高级演示展示 mini-industrial-control 库在复杂工业场景中的综合应用，涵盖：
- 多设备 Modbus 网络通讯
- OPC UA 服务器模拟与客户端集成
- SCADA 数据采集、报警联动与报表生成
- 安全 PLC 冗余架构与故障诊断
- 工业协议栈的完整数据流

## 架构设计

```
┌─────────────────────────────────────────────────────────┐
│                     SCADA / HMI Layer                     │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐               │
│  │  Alarms   │  │  Trends  │  │ Reports  │               │
│  └─────┬─────┘  └─────┬─────┘  └─────┬─────┘               │
│        └───────────────┼──────────────┘                   │
│                ┌───────┴───────┐                          │
│                │  OPC UA Server │                         │
│                └───────┬───────┘                          │
├────────────────────────┼──────────────────────────────────┤
│              Control / PLC Layer                          │
│  ┌──────────────┐  ┌───┴────────┐  ┌──────────────┐      │
│  │ Standard PLC │  │ Safety PLC │  │ Motion Ctrl  │      │
│  │ (Ladder)     │  │ (SIL3)     │  │              │      │
│  └──────┬───────┘  └─────┬──────┘  └──────────────┘      │
│         └────────────────┼────────────────┘               │
│                   ┌──────┴──────┐                         │
│                   │ Modbus TCP  │                         │
│                   └──────┬──────┘                         │
├──────────────────────────┼────────────────────────────────┤
│                Field I/O Layer                            │
│  ┌──────┐ ┌──────┐ ┌─────┴──┐ ┌──────┐ ┌──────┐         │
│  │ DI   │ │ DO   │ │ AI     │ │ AO   │ │SafeIO│         │
│  └──────┘ └──────┘ └────────┘ └──────┘ └──────┘         │
└──────────────────────────────────────────────────────────┘
```

## 高级 PLC 编程模式

### 模式 1: 状态机实现 (State Machine Pattern)

使用梯形图实现有限状态机 (FSM)：

```
States: IDLE(0), RUNNING(1), PAUSED(2), FAULT(3)
Transitions:
  IDLE ──[Start & !Fault]──→ RUNNING
  RUNNING ──[Pause]──→ PAUSED
  PAUSED ──[Resume]──→ RUNNING
  ANY ──[Fault]──→ FAULT
  FAULT ──[Reset & !Fault]──→ IDLE
```

实现方法：
```c
// Rung N: State transition logic
plc_rung_t *r = plc_add_rung(&plc);
plc_add_contact(r, "State_IDLE", PLC_CONTACT_NO, S_IDLE);
plc_add_contact(r, "Start", PLC_CONTACT_NO, I_START);
plc_add_contact(r, "Fault", PLC_CONTACT_NC, I_FAULT);
plc_add_coil(r, "Next_RUNNING", PLC_COIL_INTERNAL, S_NEXT_RUNNING);
```

### 模式 2: 互锁逻辑 (Interlock Pattern)

确保两个设备不会同时运行：

```
MotorA ──→ 禁止 MotorB 启动
MotorB ──→ 禁止 MotorA 启动
```

```c
// MotorA interlock
plc_rung_t *r1 = plc_add_rung(&plc);
plc_add_contact(r1, "StartA", PLC_CONTACT_NO, I_START_A);
plc_add_contact(r1, "MotorB", PLC_CONTACT_NC, M_B);
plc_add_coil(r1, "MotorA", PLC_COIL_OUTPUT, M_A);
```

### 模式 3: 级联控制 (Cascade Pattern)

设备按顺序启动，按反序停止：

```
Pump1 → Pump2 → Pump3 → MainMotor
```

```c
// Pump1 runs when MainMotor runs
plc_rung_t *r = plc_add_rung(&plc);
plc_add_contact(r, "MainMotor", PLC_CONTACT_NO, M_MAIN);
plc_add_timer(r, "T_Pump1Delay", PLC_TIMER_TON, 2000);
plc_add_coil(r, "Pump1", PLC_COIL_OUTPUT, M_PUMP1);
```

## Modbus 多设备网络

### 网络拓扑

```
┌──────────────────────────────────────────────┐
│               Modbus Master                   │
│          (SCADA / PLC / Gateway)              │
└────┬──────────┬──────────┬──────────┬─────────┘
     │   RTU    │   RTU    │   TCP    │   TCP
┌────┴──┐ ┌─────┴──┐ ┌─────┴──┐ ┌─────┴──┐
│Slave 1│ │Slave 2 │ │Slave 3 │ │Slave 4 │
│Addr=1 │ │Addr=2  │ │IP=.3   │ │IP=.4   │
└───────┘ └────────┘ └────────┘ └────────┘
```

### 轮询策略

```c
typedef struct {
    modbus_device_t devices[32];
    uint8_t num_devices;
    uint8_t current_index;
    uint32_t poll_interval_ms;
    uint32_t last_poll_ms;
    uint32_t timeout_ms;
    uint8_t retry_count;
} modbus_network_t;

void modbus_network_poll(modbus_network_t *net, uint32_t now_ms) {
    if (now_ms - net->last_poll_ms < net->poll_interval_ms) return;

    modbus_device_t *dev = &net->devices[net->current_index];

    // Read holding registers from device
    // Build request, send, parse response, retry on timeout

    net->current_index = (net->current_index + 1) % net->num_devices;
    net->last_poll_ms = now_ms;
}
```

### 地址映射表

| 设备 | 从站地址 | 起始地址 | 长度 | 数据说明 |
|------|---------|---------|------|----------|
| 温度变送器 | 1 | 40001 | 4 | 4通道温度值 |
| 压力变送器 | 2 | 40001 | 2 | 2通道压力值 |
| 流量计 | 3 | 40001 | 6 | 瞬时+累计流量 |
| 电表 | 4 | 40001 | 12 | 电压/电流/功率/电能 |
| VFD 变频器 | 5 | 40001 | 8 | 频率/电流/转速/状态 |
| 安全 PLC | 6 | 40001 | 4 | 安全状态/诊断 |

### TCP 与 RTU 选择指南

| 特性 | RTU | TCP |
|------|-----|-----|
| 物理层 | RS-485 | Ethernet |
| 最大距离 | 1200m | 100m (铜缆) |
| 节点数 | 32 (可扩展) | 无限制 |
| 速率 | 最高 115.2 kbps | 10/100/1000 Mbps |
| 实时性 | 确定性 | 取决于网络负载 |
| 校验 | CRC-16 | TCP checksum |
| 帧开销 | 低 | 高 (MBAP 7字节) |
| 典型应用 | 就地 I/O | 远程监控 |

## OPC UA 高级功能

### 地址空间设计

推荐的 OPC UA 地址空间层次：

```
Root
├── Objects
│   ├── Devices
│   │   ├── PLC1
│   │   │   ├── Inputs
│   │   │   │   ├── I0.0 (Boolean)
│   │   │   │   └── I0.1 (Boolean)
│   │   │   ├── Outputs
│   │   │   │   ├── Q0.0 (Boolean)
│   │   │   │   └── Q0.1 (Boolean)
│   │   │   └── Diagnostics
│   │   │       ├── ScanTime (UInt32)
│   │   │       └── FaultCode (UInt16)
│   │   └── VFD1
│   │       ├── Speed (Double)
│   │       ├── Current (Double)
│   │       └── Status (UInt16)
│   ├── Tags
│   │   ├── Tank1.Temperature (Double)
│   │   ├── Tank1.Pressure (Double)
│   │   └── Conveyor.Speed (Double)
│   └── Alarms
│       ├── ActiveAlarms (UInt32)
│       └── AlarmList[0..N]
└── Types
    ├── DataTypes
    └── ObjectTypes
```

### 变量缓存与变化检测

```c
bool opcua_check_data_change(opcua_address_space_t *as,
                             uint32_t monitored_item_id,
                             opcua_variant_t *new_value) {
    // 1. 获取当前变量值
    // 2. 与上次采样值比较 (memcmp)
    // 3. 如果不同，更新缓存，触发通知
    // 4. 如果相同，返回 false
}
```

### 采样间隔配置

| 应用场景 | 建议采样间隔 | 说明 |
|---------|-------------|------|
| 高速计数 | 10 ms | 需要高实时性 |
| 模拟量趋势 | 250 ms | 标准监控 |
| 温度监测 | 1000 ms | 慢变化过程 |
| 状态监测 | 5000 ms | 开关量状态 |
| 诊断数据 | 30000 ms | 非实时数据 |

## SCADA 高级数据管理

### 死区滤波

死区 (Deadband) 防止因噪声引起的不必要记录：

```
新值 - 上次记录值 > deadband → 记录
否则 → 丢弃
```

对于百分比死区：
```
abs(new - last) > (range × percentage / 100) → 记录
```

### 数据压缩算法

**Swinging Door 算法 (矩形波串):**
```
维护一个"门"的角度范围，新数据点落在门外时才记录
```

**Deadband + 时间戳:**
```
if (|new - last| > deadband) OR (now - last_record_time > max_interval)
    → 记录
```

### 报警联动动作

| 报警级别 | 动作 | 示例 |
|---------|------|------|
| Lo/LoLo | 记录事件、通知操作员 | "液位过低，泵已停止" |
| Hi/HiHi | 记录事件、通知操作员、可能的自动停机 | "温度过高，加热器关闭" |
| ROC | 趋势分析，预警 | "压力上升过快" |
| Deviation | 偏差质量管理 | "产品厚度超标" |

### 报表类型

| 报表 | 频率 | 内容 |
|------|------|------|
| 班报 | 每8小时 | 产量、废品率、OEE |
| 日报 | 每日 | 产量、效率、报警汇总 |
| 周报 | 每周 | 趋势、维护建议 |
| 月报 | 每月 | KPI、停机分析 |

## 安全 PLC 高级架构

### SIL3 冗余架构

```
         ┌──────────────────┐
         │   Safety Relay    │
         │   (强制导向)      │
         └───┬──────────┬────┘
             │          │
    ┌────────┴──┐  ┌────┴──────────┐
    │ Channel A │  │  Channel B    │
    │ Input:    │  │  Input:       │
    │  E-Stop1  │  │  E-Stop2      │
    │  Door1    │  │  Door2        │
    │  Curtain1 │  │  Curtain2     │
    └─────┬─────┘  └──────┬────────┘
          │               │
    ┌─────┴───────────────┴─────┐
    │    Safety CPU (Dual)      │
    │    Cross-check results    │
    └─────────────┬─────────────┘
                  │
    ┌─────────────┴─────────────┐
    │    Safe Outputs           │
    │    (Dual Contactors)      │
    └───────────────────────────┘
```

### 故障模式与影响分析 (FMEA)

| 故障模式 | 影响 | 检测方法 | 安全反应 |
|---------|------|---------|---------|
| 输入短路 | 常 ON | 双通道差异 | 停机+报警 |
| 输入断路 | 常 OFF | 双通道差异 | 停机+报警 |
| 输出触点焊死 | 无法断开 | 触点反馈 | 禁止重启 |
| CPU 故障 | 逻辑错误 | 看门狗+交叉检测 | 安全状态 |
| 通讯中断 | 数据丢失 | 超时检测 | 安全状态 |
| 电源失效 | 全部掉电 | 掉电检测 | 安全状态 |

### 诊断覆盖率 (DC)

| 诊断措施 | 覆盖率 | 适用 SIL |
|---------|--------|---------|
| 双通道比较 | 99% | SIL3 |
| 测试脉冲 | 90% | SIL2 |
| 循环测试 | 60% | SIL1 |
| 无诊断 | 0% | 不适用 |

### 响应时间计算

```
T_response = T_detection + T_processing + T_output

典型值:
  T_detection  = 10 ms   (传感器响应)
  T_processing = 10 ms   (PLC 扫描)
  T_output     = 15 ms   (接触器断开)
  ─────────────────────
  T_response   = 35 ms   (总响应时间)
```

### 双手控制台测试步骤

```
1. 同时按下左右按钮 → 输出 ON
2. 释放任意按钮 → 输出 OFF
3. 重新同时按下 → 输出 ON
4. 先按左，500ms 后按右 → 输出 ON
5. 先按左，600ms 后按右 → 输出 OFF (超时)
6. 用异物固定一个按钮 → 另一按钮无效
7. 单按钮操作 → 无输出
```

## 综合集成示例

### 完整的工业网关

```c
// 工业协议网关，将 Modbus RTU 数据桥接到 OPC UA
typedef struct {
    modbus_device_t       modbus;      // Modbus 从站
    opcua_address_space_t opcua;       // OPC UA 服务器
    scada_collector_t     scada;       // SCADA 采集器
    safety_controller_t   safety;      // 安全 PLC
    bool                  running;
    uint64_t              tick_ms;
} industrial_gateway_t;

void gateway_scan_cycle(industrial_gateway_t *gw) {
    // 1. 读取 Modbus 数据
    for (uint32_t i = 0; i < gw->scada.num_tags; i++) {
        scada_tag_t *tag = &gw->scada.tags[i];
        uint16_t reg_value;
        if (modbus_read_holding_register(&gw->modbus.data_model,
                                          tag->register_address, &reg_value)) {
            double scaled = reg_value * tag->scale_factor + tag->offset;
            scada_write_tag_value(&gw->scada, i, scaled);
        }
    }

    // 2. 更新 OPC UA 变量
    for (uint32_t i = 0; i < gw->opcua.num_nodes; i++) {
        opcua_node_t *node = &gw->opcua.nodes[i];
        if (node->node_class == OPCUA_NODE_VARIABLE) {
            scada_tag_t *tag = scada_find_tag(&gw->scada, node->display_name);
            if (tag) {
                opcua_set_double_value(&gw->opcua, node->identifier,
                                       tag->current_value);
            }
        }
    }

    // 3. 检查报警
    scada_check_alarms(&gw->scada, gw->tick_ms);

    // 4. 评估安全功能
    safety_evaluate(&gw->safety, gw->tick_ms);

    // 5. 检查 OPC UA 订阅通知
    opcua_update_monitored_items(&gw->opcua, gw->tick_ms);
}
```

## 性能优化指南

### PLC 扫描优化

| 优化措施 | 效果 |
|---------|------|
| 减少梯级数量 | 线性减少扫描时间 |
| 使用子程序调用 | 减少主循环条件判断 |
| 预分配梯级数组 | 避免动态内存分配 |
| 批量 I/O 更新 | 减少外设访问次数 |

### Modbus 通讯优化

| 优化措施 | 效果 |
|---------|------|
| 批量读取寄存器 | 减少帧数量 |
| 合理设置超时 | 平衡可靠性与速度 |
| 使用 TCP 而非 RTU | 提高带宽利用率 |
| 多请求流水线 | 提高吞吐量 |

### SCADA 存储优化

| 策略 | 说明 |
|------|------|
| 环形缓冲区 | 固定内存，自动覆盖旧数据 |
| 死区记录 | 减少存储点数 |
| 时间聚合 | 长时间存储用平均值/极值 |
| 数据压缩 | Swing Door 算法 |

## 故障排查指南

### 常见问题

| 问题 | 可能原因 | 解决方法 |
|------|---------|---------|
| Modbus 无响应 | 地址/波特率/校验不匹配 | 检查配置 |
| CRC 校验失败 | 数据线干扰 | 降低波特率/使用屏蔽线 |
| 梯形图逻辑错误 | 触点/线圈地址冲突 | 检查地址分配 |
| 定时器不触发 | 输入条件不满足 | 跟踪功率流 |
| 报警抖动 | 死区设置过小 | 增大死区值 |
| 安全系统误触发 | 差异时间过短 | 增大 SAFETY_DISCREPANCY_MS |

### 调试输出

```c
#ifdef DEBUG
#define PLC_DEBUG(fmt, ...) printf("[PLC] " fmt "\n", ##__VA_ARGS__)
#else
#define PLC_DEBUG(fmt, ...)
#endif
```

## 参考标准

- **IEC 61131** - Programmable controllers (Parts 1-9)
- **IEC 61784** - Industrial communication networks
- **IEC 62541** - OPC Unified Architecture
- **IEC 61508** - Functional safety of E/E/PE systems
- **IEC 61511** - Functional safety for process industry
- **IEC 62061** - Safety of machinery
- **ISO 13849** - Safety-related parts of control systems
- **Modbus Organization** - MODBUS Application Protocol V1.1b3
- **NAMUR NE 43** - Standardization of signal levels
- **ISA-88** - Batch control
- **ISA-95** - Enterprise-control system integration
