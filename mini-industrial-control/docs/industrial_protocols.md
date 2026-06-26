# 工业通讯协议参考文档

## 1. Modbus 协议详解

### 1.1 协议概述

Modbus 是 Modicon (现施耐德电气) 于 1979 年发布的串行通讯协议，是工业自动化领域应用最广泛的现场总线协议之一。

**协议栈层次:**
```
┌─────────────────────┐
│  Application Layer  │ ← Modbus PDU (Protocol Data Unit)
├─────────────────────┤
│  Data Link Layer    │ ← RTU / ASCII framing, CRC/LRC
├─────────────────────┤
│  Physical Layer     │ ← RS-232 / RS-485 / Ethernet
└─────────────────────┘
```

### 1.2 PDU 格式

**请求 PDU:**
```
+----------------+----------------+
| Function Code  |     Data       |
+----------------+----------------+
|     1 byte     |    N bytes     |
+----------------+----------------+
```

**响应 PDU (正常):**
```
+----------------+----------------+
| Function Code  |     Data       |
+----------------+----------------+
|     1 byte     |    N bytes     |
+----------------+----------------+
```

**异常响应 PDU:**
```
+---------------------+------------------+
| Function Code + 0x80| Exception Code   |
+---------------------+------------------+
|       1 byte         |     1 byte       |
+---------------------+------------------+
```

### 1.3 RTU 帧格式

```
┌──────────┬──────────┬──────────┬──────────┐
│  Start   │ Address  │ Function │   Data   │  CRC    │  End    │
│ ≥3.5char │  8 bits  │  8 bits  │ N×8 bits │ 16 bits │≥3.5char │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
```

- **起始间隔**: ≥ 3.5 字符时间 (在 19200 bps 下 ≈ 1.75 ms)
- **地址域**: 1-247 (0=广播, 248-255=保留)
- **功能码**: 1-255 (128-255 用于异常响应)
- **CRC**: 低字节在前，高字节在后

字符时间计算:
```
T_char = (start_bit + data_bits + parity_bit + stop_bits) / baud_rate
T_3.5 = 3.5 × T_char

示例 (19200, 8N1):
T_char = 10 / 19200 = 0.52 ms
T_3.5 = 1.82 ms
```

### 1.4 TCP 帧格式 (MBAP)

```
┌──────────────────────────────────────────────────────┐
│                  MODBUS TCP/IP ADU                    │
├──────────────┬──────────────┬─────────┬──────────────┤
│ MBAP Header  │ Function Code│  Data   │              │
├──────┬───────┼──────┬───────┤         │              │
│Trans │Proto  │Length│Unit ID│         │              │
│ ID   │ ID    │      │       │         │              │
│2Bytes│2Bytes │2Bytes│1 Byte │ 1 Byte  │   N Bytes    │
└──────┴───────┴──────┴───────┴─────────┴──────────────┘
```

MBAP 头字段详解:

| 字段 | 大小 | 说明 |
|------|------|------|
| Transaction ID | 2 bytes | 客户端生成，服务器原样返回 |
| Protocol ID | 2 bytes | Modbus = 0x0000 |
| Length | 2 bytes | 后续字节数 (UID + FC + Data) |
| Unit ID | 1 byte | 串行链路从站地址或 0xFF |

### 1.5 数据模型寻址

Modbus 使用 4 种数据模型:

| 数据类型 | 对象类型 | 访问 | 地址范围 |
|----------|---------|------|----------|
| Coils | 单个位 | R/W | 00001-09999 |
| Discrete Inputs | 单个位 | R | 10001-19999 |
| Input Registers | 16位字 | R | 30001-39999 |
| Holding Registers | 16位字 | R/W | 40001-49999 |

**实际地址计算:**
```
PDU 地址 = 应用地址 - 偏移
偏移值:
  Coils (0x):          PDU = address - 1
  Holding Regs (4x):   PDU = address - 40001
  Input Regs (3x):     PDU = address - 30001
```

### 1.6 常用功能码详解

#### 0x01 - Read Coils
```
请求:
  FC = 0x01
  StartAddr = 2 bytes (0x0000-0xFFFF)
  Quantity   = 2 bytes (1-2000)

响应:
  FC = 0x01
  ByteCount = 1 byte (N)
  CoilStatus = N bytes (每字节8个线圈)

示例: 读取线圈 20-28 (共9个)
  请求: 01 01 00 13 00 09 CRC16
  响应: 01 01 02 CD 06 CRC16
        CD=1100 1101, 06=0000 0110
        线圈 20-27: 1 0 1 1 0 0 1 1 (CD, bit0=coil20)
        线圈 28:    0 (06 bit0, 仅1位有效)
```

#### 0x03 - Read Holding Registers
```
请求:
  FC = 0x03
  StartAddr = 2 bytes
  Quantity   = 2 bytes (1-125)

响应:
  FC = 0x03
  ByteCount = 1 byte (2×N)
  RegValues = N×2 bytes (每个寄存器高字节在前)

示例: 读取 HR[40108-40110]
  请求: 01 03 00 6B 00 03 CRC16
  响应: 01 03 06 02 2B 00 00 00 64 CRC16
        HR[40108] = 0x022B = 555
        HR[40109] = 0x0000 = 0
        HR[40110] = 0x0064 = 100
```

### 1.7 CRC16 算法

**多项式**: 0xA001 (0x8005 的反转)

**查表法实现:**
```c
static const uint16_t crc_table[256] = { /* 预计算值 */ };

uint16_t crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc_table[(crc ^ data[i]) & 0xFF];
    }
    return crc;
}
```

**校验步骤:**
1. 初始化 CRC 为 0xFFFF
2. 对每个数据字节，查表更新 CRC
3. 将 16 位 CRC 附加到帧尾 (低字节在前)

### 1.8 异常响应格式

```
请求功能码 = 0x03
异常响应功能码 = 0x83 (即 0x03 | 0x80)

异常帧:
  SlaveAddr | 0x83 | ExceptionCode | CRC16
```

## 2. OPC UA 协议

### 2.1 协议架构

OPC UA (Unified Architecture) 是 OPC Foundation 发布的工业通讯标准 (IEC 62541)。

```
┌─────────────────────────────────────┐
│     OPC UA Client/Server Model      │
├─────────────────────────────────────┤
│  Services (Read/Write/Browse/Sub)   │
├─────────────────────────────────────┤
│  Encoding (Binary / XML / JSON)     │
├─────────────────────────────────────┤
│  Security (Sign / Encrypt)          │
├─────────────────────────────────────┤
│  Transport (UA TCP / HTTPS)         │
└─────────────────────────────────────┘
```

### 2.2 地址空间模型

**NodeId 结构:**
```
NodeId = (NamespaceIndex, IdentifierType, Identifier)

IdentifierType:
  0 = Numeric (uint32)
  1 = String
  2 = GUID (16 bytes)
  3 = Opaque (ByteString)
```

**参考类型 (Reference Types):**
```
Organizes          - 层次组织关系
HasComponent        - 组件包含关系
HasProperty         - 属性关系
HasTypeDefinition   - 类型定义关系
HasSubtype          - 子类型关系
```

### 2.3 服务集

| 服务集 | 功能 | 关键服务 |
|--------|------|---------|
| Discovery | 发现服务器 | FindServers, GetEndpoints |
| Session | 会话管理 | CreateSession, ActivateSession |
| NodeManagement | 节点管理 | AddNodes, DeleteNodes |
| View | 浏览查询 | Browse, BrowseNext |
| Attribute | 属性读写 | Read, Write |
| Method | 方法调用 | Call |
| MonitoredItem | 数据监测 | CreateMonitoredItems |
| Subscription | 订阅管理 | CreateSubscription, Publish |

### 2.4 数据变化通知机制

```
Client                          Server
  │                               │
  │──CreateSubscription──→        │  (PublishingInterval=500ms)
  │←──SubscriptionId──           │
  │                               │
  │──CreateMonitoredItems──→      │  (SamplingInterval=250ms)
  │←──MonitoredItemIds──         │
  │                               │
  │──Publish──→                   │  (Client polls for notifications)
  │←─NotificationMessage──       │  (DataChange: values changed)
  │──Publish──→                   │
  │←─KeepAlive──                 │  (No data changed, keep alive)
```

### 2.5 变量属性

每个变量节点包含的属性:

| 属性ID | 属性名 | 说明 |
|--------|--------|------|
| 1 | NodeId | 节点唯一标识 |
| 2 | NodeClass | 节点类别 |
| 3 | BrowseName | 浏览名称 (QualifiedName) |
| 4 | DisplayName | 显示名称 |
| 13 | Value | 当前值及时间戳/品质 |
| 14 | DataType | 数据类型 |
| 26 | AccessLevel | 访问级别 |

### 2.6 UA Binary 编码

```
基本类型编码:
  Boolean:   1 byte  (0/1)
  Byte:      1 byte
  Int16:     2 bytes, little-endian
  UInt16:    2 bytes, little-endian
  Int32:     4 bytes, little-endian
  Float:     4 bytes, IEEE 754
  Double:    8 bytes, IEEE 754
  String:    Int32(length) + UTF-8 bytes
```

## 3. PROFIsafe 安全通讯

### 3.1 安全层架构

```
┌───────────────────────────┐
│   Safety Application      │ ← 安全功能块
├───────────────────────────┤
│   PROFIsafe Layer         │ ← 安全报文处理
├───────────────────────────┤
│   Standard Protocol       │ ← PROFINET / PROFIBUS
├───────────────────────────┤
│   Physical Layer          │ ← Ethernet / RS-485
└───────────────────────────┘
```

### 3.2 安全措施

PROFIsafe 实现以下安全措施 (满足 SIL3):

| 措施 | 检测内容 | 残余错误概率 |
|------|---------|-------------|
| 连续编号 | 报文序列错误 | < 10⁻⁷ |
| 超时检测 | 报文丢失/延迟 | < 10⁻⁹ |
| 代码名检查 | 收发方配对 | < 10⁻⁸ |
| CRC 校验 | 报文损坏 | < 10⁻¹⁰ |
| 地址检查 | 设备身份 | < 10⁻⁷ |

### 3.3 残余错误率

```
综合残余错误率 = Σ(各措施残余错误率)
               = 10⁻⁷ + 10⁻⁹ + 10⁻⁸ + 10⁻¹⁰ + 10⁻⁷
               ≈ 2.1 × 10⁻⁷
满足 SIL3 要求 (< 10⁻⁷/hr)
```

---

## 参考标准

- **IEC 61158** - Industrial communication networks - Fieldbus specifications
- **IEC 61784** - Industrial communication networks - Profiles
- **IEC 62541** - OPC Unified Architecture
- **MODBUS Application Protocol V1.1b3**
- **PROFIsafe System Description V2.6**
- **IEC 61508** - Functional safety
