# Mini Device Attestation — 设备认证与云端注册详解

> 深入讲解 IoT 设备身份认证机制：从工厂注入到云端验证的完整流程

---

## 1. 概述 (Overview)

设备认证 (Device Attestation) 是 IoT 设备安全连接到云平台的前置条件。
它回答了 "这台设备真的是它声称的那台吗？" 这个问题。
通过在设备制造时注入唯一密钥或证书，设备可以在首次启动时向云平台证明其身份。

### 认证三要素

| 要素 | 说明 |
|-----|------|
| 你是谁 (Identity) | 唯一设备标识符 + 密钥/证书 |
| 你是什么状态 (Integrity) | 固件哈希 + 安全启动状态 |
| 你能做什么 (Authorization) | 设备在云端的权限策略 |

---

## 2. 工厂注入 (Factory Injection)

### 2.1 设备唯一密钥

每个设备出厂时拥有唯一的密钥对 (公钥+私钥)：
- 私钥存储于安全元件 (ATECC608 / TPM) 或安全存储中
- 公钥由工厂 CA 签名，形成设备证书
- 密钥永不离开设备

```
工厂生产线:
  ┌────────────┐      ┌──────────────┐      ┌────────────┐
  │ 安全元件    │ ---->│  密钥注入工具 │ ---->│ 空中烧录    │
  │ (ATECC608) │      │  (Client SW) │      │ (OTA Cert) │
  └────────────┘      └──────────────┘      └────────────┘
                              │
                              v
                    ┌──────────────────┐
                    │ 工厂 CA (签名)    │
                    │  设备证书 = Sign( │
                    │    设备UID,        │
                    │    设备公钥)       │
                    └──────────────────┘
```

### 2.2 安全元件密钥生成

安全元件在首次使用时内部生成密钥：
1. 生成随机数 → 私钥
2. 计算公钥 → ECDH
3. 将公钥导出供工厂 CA 签名
4. 私钥永不可导出

---

## 3. 认证令牌 (Attestation Token)

### 3.1 令牌结构

```
┌───────────────────────────────────────────────────┐
│  Attestation Token (IAT - Initial Attestation)     │
│                                                    │
│  ┌────────────────────────────────────────────────┐ │
│  │ Header                                           │ │
│  │  "ATTEST" (6 bytes)  - 令牌标识               │ │
│  └────────────────────────────────────────────────┘ │
│  ┌────────────────────────────────────────────────┐ │
│  │ Device ID (32 bytes)   - 设备唯一标识符         │ │
│  └────────────────────────────────────────────────┘ │
│  ┌────────────────────────────────────────────────┐ │
│  │ Firmware Hash (32 bytes) - SHA256 固件哈希     │ │
│  └────────────────────────────────────────────────┘ │
│  ┌────────────────────────────────────────────────┐ │
│  │ Boot State (8 bytes)   - 安全启动状态           │ │
│  │  [0] = secure_boot_OK (0/1)                    │ │
│  │  [1..3] = firmware_version                    │ │
│  └────────────────────────────────────────────────┘ │
│  ┌────────────────────────────────────────────────┐ │
│  │ Signature (64 bytes)   - ECDSA 签名            │ │
│  └────────────────────────────────────────────────┘ │
└───────────────────────────────────────────────────┘
```

---

## 4. 云平台集成

### 4.1 AWS IoT Core — JITP (Just-In-Time Provisioning)

```
  设备                          AWS IoT Core                   AWS Lambda
  ┌────┐                       ┌─────────────┐              ┌──────────┐
  │设备│                       │ Device Gway  │              │ JITP Fn  │
  └──┬─┘                       └──────┬──────┘              └────┬─────┘
     │                                │                          │
     │ 1. TLS Connect (device cert)   │                          │
     │───────────────────────────────>│                          │
     │                                │ 2. 检查证书是否已注册    │
     │                                │──────────────────────────>│
     │                                │                          │
     │                                │ 3. 证书未注册 → 创建 Thing│
     │                                │<──────────────────────────│
     │                                │                          │
     │ 4. CONNECT ACK                │                          │
     │<───────────────────────────────│                          │
```

JITP 流程：
1. 设备用工厂证书 (已由 CA 签名) 连接 AWS IoT
2. AWS IoT 检查证书注册状态
3. 若未注册 → 触发 Lambda 自动创建 Thing + 附加策略
4. 生成了唯一的 Thing Name

### 4.2 Azure DPS (Device Provisioning Service)

```
  设备                          Azure DPS                     Azure IoT Hub
  ┌────┐                       ┌───────────┐                ┌────────────┐
  │设备│                       │Azure DPS   │                │ IoT Hub    │
  └──┬─┘                       └─────┬─────┘                └──────┬─────┘
     │                              │                               │
     │ 1. 注册请求(RegID + 签名)     │                               │
     │────────────────────────────ĺ>│                               │
     │                              │ 2. 验证设备密钥/证书           │
     │                              │ 3. 分配 IoT Hub 端点          │
     │<─────────────────────────────│                               │
     │                              │                               │
     │ 4. 连接到分配的 IoT Hub      │                               │
     │──────────────────────────────────────────────────────────────>│
```

Azure DPS 支持两种认证方式：
- **TPM**: 设备有 TPM + 背书密钥 (EK)
- **证书**: 设备有 X.509 设备证书

---

## 5. 挑战-响应认证 (Challenge-Response)

### 5.1 协议流程

```
云平台 (Verifier)                            设备 (Prover)
     │                                            │
     │ 1. 生成随机 nonce                          │
     │    Challenge = nonce || server_id          │
     │───────────────────────────────────────────>│
     │                                            │
     │                          2. 组合数据:      │
     │                             msg = nonce || device_id || fw_hash
     │                          3. 签名:          │
     │                             sig = ECDSA_Sign(device_key, msg)
     │                          4. 构建响应:      │
     │                             Response = msg || sig
     │<───────────────────────────────────────────│
     │                                            │
     │ 5. 验证 nonce 正确 (防重放)                 │
     │ 6. 验证签名                                │
     │ 7. 验证 fw_hash 匹配白名单                  │
     │ 8. 返回结果                                │
```

---

## 6. PSA Initial Attestation (IAT)

ARM PSA 定义了标准的初始认证令牌格式：

| 字段 | 描述 |
|-----|------|
| Nonce | 16-64 字节随机数 (从云端收到) |
| Client ID | 设备分区 ID |
| Security Lifecycle | 安全生命周期状态 |
| SW Components | 固件哈希 + 版本号 |
| Boot Seed | 安全启动的随机种子 |
| Implementation ID | 实现者唯一 ID |
| Certification Reference | 认证参考令牌 |

---

## 7. X.509 设备证书

### 7.1 证书格式

```
Certificate ::= SEQUENCE {
    tbsCertificate      TBSCertificate,
    signatureAlgorithm  AlgorithmIdentifier,
    signatureValue      BIT STRING
}

TBSCertificate ::= SEQUENCE {
    version            [0] EXPLICIT INTEGER DEFAULT v1,
    serialNumber       INTEGER,
    signature          AlgorithmIdentifier,
    issuer             Name,
    validity           SEQUENCE { notBefore, notAfter },
    subject            Name,
    subjectPublicKeyInfo   SubjectPublicKeyInfo,
    extensions         [3] EXPLICIT Extensions OPTIONAL
}
```

### 7.2 证书类型

| 类型 | 用途 | 签发者 |
|-----|------|--------|
| 设备证书 | 识别单个设备 | 工厂 CA |
| 中间 CA | 签发设备证书 | 根 CA |
| 根 CA | 信任根 | 自签名 |
| JITP 证书 | AWS IoT 自动注册 | 工厂 CA |

---

## 8. 本模块实现

本模块 (`device_attest.h/c`) 提供了：

- `device_attest_init()`: 初始化认证上下文
- `device_identity_generate()`: 由 UID 生成设备身份
- `device_identity_inject_factory()`: 工厂注入密钥+证书
- `device_attest_token_generate()`: 生成认证令牌
- `device_attest_challenge()`: 接收云端挑战
- `device_attest_verify_cloud()`: 云端验证设备
- `device_attest_protocol_aws()`: AWS IoT 协议绑定
- `device_attest_protocol_azure()`: Azure DPS 协议绑定
- `device_attest_jitp_cert()`: 生成 JITP 证书
- `device_attest_sign_challenge()`: 签名挑战响应
- `device_attest_validate_token()`: 验证令牌有效性
- `device_attest_derive_session_key()`: 派生会话密钥
- 内嵌 SHA256 和 ECDSA 模拟实现

---

## 参考资料

- PSA Certified Initial Attestation Specification
- AWS IoT Core Developer Guide: JITP
- Azure DPS Documentation: Device Attestation
- TCG Device Identity Attestation Specification
- IETF RFC 9334: Remote ATtestation procedureS (RATS)
