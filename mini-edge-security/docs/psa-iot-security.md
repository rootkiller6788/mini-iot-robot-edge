# PSA IoT 安全认证 (PSA Certified IoT Security)

> 深度解析 ARM Platform Security Architecture (PSA) 认证体系：从 Level 1 到 Level 3

---

## 1. PSA 概述 (Platform Security Architecture)

PSA 是 ARM 提出的物联网设备安全架构标准，定义了从硬件到软件的完整安全框架。
PSA 的目标是提供一种标准化的方法来构建安全 IoT 设备，强调四个核心安全功能：

| 核心功能 | 描述 |
|---------|------|
| 安全启动 | 保证启动的每一阶段代码未被篡改 |
| 加密服务 | 标准的加密 API (PSA Crypto API) |
| 安全存储 | 加密存储敏感数据 |
| 认证 | 设备向远程方证明其安全状态 |

---

## 2. PSA Certified 安全等级

### 2.1 Level 1: 基础安全

防护目标：抵御软件攻击和基本网络攻击

要求：
- 安全启动 (验证固件签名)
- PSA Crypto API 完整实现
- 安全存储 (隔离的 KV 存储)
- 设备 UID + 安全生命周期管理

适用场景：
- 室内传感器 (低风险环境)
- 个人可穿戴设备
- 消费电子产品

### 2.2 Level 2: 硬件隔离

防护目标：抵御扩展到硬件层面的攻击 (包括本地攻击者)

要求在 Level 1 的基础上增加：
- 硬件隔离 (ARM TrustZone 或物理内存保护)
- 安全外设 (隔离的安全外设)
- PSA 固件更新 API
- 防侧信道的基本防御

适用场景：
- 智能门锁
- 医疗设备 (连接型)
- 工业传感器

### 2.3 Level 3: 防篡改

防护目标：抵御物理攻击和供应链攻击

要求在 Level 2 的基础上增加：
- 防篡改硬件 (主动屏蔽层)
- 设备认证 (Initial Attestation Token)
- 固件更新回滚保护
- 更高级的侧信道防御
- 安全元件 (如 ATECC608)

适用场景：
- 支付终端
- 电动汽车充电桩
- 关键基础设施

---

## 3. PSA Crypto API

### 3.1 API 分类

```
PSA Crypto API
│
├── PSA Key Management
│   ├── psa_generate_key()
│   ├── psa_import_key()
│   ├── psa_export_public_key()
│   ├── psa_destroy_key()
│   └── psa_get_key_attributes()
│
├── PSA Asymmetric
│   ├── psa_sign_message()
│   ├── psa_verify_message()
│   ├── psa_sign_hash()
│   └── psa_verify_hash()
│
├── PSA Symmetric
│   ├── psa_cipher_encrypt()
│   ├── psa_cipher_decrypt()
│   └── psa_aead_encrypt/decrypt()
│
├── PSA Hashing
│   ├── psa_hash_compute()
│   ├── psa_hash_compare()
│   └── psa_hash_update/finish()
│
└── PSA Key Derivation
    ├── psa_key_derivation_setup()
    ├── psa_key_derivation_input_bytes()
    └── psa_key_derivation_output_bytes()
```

### 3.2 支持的算法

| 非对称 | 哈希 | 对称 | AEAD |
|-------|------|------|------|
| ECDSA P-256 SHA256 | SHA-256 | AES-128-ECB | AES-128-GCM |
| ECDSA P-384 SHA384 | SHA-384 | AES-128-CBC | AES-256-GCM |
| ECDH P-256 | SHA-512 | AES-256-CBC | CCM |
| RSA 2048 PKCS#1.5 | - | AES-128-CTR | CCM-Star |

---

## 4. 安全生命周期 (Security Lifecycle)

```
            ┌─────────────┐
            │  UNKNOWN     │  (芯片出厂默认)
            └──────┬──────┘
                   │ 设备组装
                   v
            ┌─────────────┐
            │  ASSEMBLY    │  (硬件集成阶段)
            └──────┬──────┘
                   │ OTP 熔丝烧录
                   v
            ┌─────────────┐
            │ PROVISIONING │  (注入密钥和证书)
            └──────┬──────┘
                   │ 信任建立
                   v
            ┌─────────────┐
            │  SECURED     │  (正常工作状态)
            └──────┬──────┘
                   │ 设备退役/恢复
                   v
            ┌─────────────┐
            │ RECOVERING   │  (恢复模式)
            └──────┬──────┘
                   │
                   v
            ┌─────────────┐
            │DECOMMISSIONED│  (永久停用)
            └─────────────┘
```

---

## 5. 初始认证 (Initial Attestation)

### 5.1 IAT 令牌内容

```
Entity Attestation Token (EAT / IAT)
│
├── Nonce            (16-64 bytes) — 防重放
├── Client ID        (4 bytes)     — 分区 ID
├── Security Lifecycle              — 设备安全状态
├── Boot Seed        (16 bytes)     — 安全启动随机值
├── Software Components             — 固件组件列表
│   ├── Measurement Type     — 度量类型 (SHA256)
│   ├── Measurement Value    — 度量值 (固件哈希)
│   └── Version              — 固件版本号
├── Implementation ID (16 bytes)    — 实现者标识
├── Certification Reference         — 认证参考
└── Hardware Version                — 硬件版本
```

### 5.2 验证流程

```
1. 设备启动
   → 安全启动验证每一阶段
   → 度量值记录到安全存储
   → 收集系统安全状态

2. 云端发起认证
   → 发送 nonce (随机数)

3. 设备生成 IAT
   → 包含度量值 + nonce + 设备ID
   → ECDSA_Sign(IAT, device_key)

4. 云端验证
   → 验证 ECDSA 签名
   → 验证 nonce 一致
   → 检查度量值是否在受信列表中
   → 允许/拒绝对接
```

---

## 6. 固件更新 API

### 6.1 安全更新流程

```
1. 下载阶段
   → 设备从 OTA 服务器下载固件包
   → 固件包含: fw_image + metadata + signature

2. 验证阶段
   → 验证 metadata 版本 > 当前版本
   → 验证固件签名 (用 fw_update_key)
   → 验证完整性 (SHA256 哈希)

3. 安装阶段
   → 将固件写入备用分区 (Double-bank update)
   → 更新固件版本元数据
   → 验证写入完整性

4. 提交阶段
   → 标记新固件为"已验证可提交"
   → 触发系统重启 → 新固件生效
```

### 6.2 Double-Bank Update

```
Flash Layout:
┌───────────────────────────────┐
│  Bootloader (安全启动)        │ ← 不变
├───────────────────────────────┤
│  Active Bank (当前固件)       │ ← 当前运行
├───────────────────────────────┤
│  Standby Bank (新固件)        │ ← 更新写入位置
├───────────────────────────────┤
│  Data / KV Store (永久存储)    │ ← 跨版本保留
└───────────────────────────────┘
```

---

## 7. 安全模型

### 7.1 隔离模型

```
NORMAL WORLD                    SECURE WORLD
┌─────────────┐                ┌───────────────┐
│  App         │                │  Trusted App   │
│  (NS)        │                │  (S)           │
├─────────────┤                ├───────────────┤
│  OS / RTOS   │                │  TF-M (Trusted │
│  (NS)        │                │  Firmware-M)   │
└──────┬──────┘                └──────┬────────┘
       │                              │
       │  SG (Secure Gateway)         │
       │<---------------------------->│
       │                              │
   ┌───┴──────────────────────────────┴───┐
   │          SAU / IDAU                    │
   │    (Security Attribution Unit)         │
   │    内存区域安全属性定义                 │
   └────────────────────────────────────────┘
```

### 7.2 外设安全

| 外设类型 | NS 访问 | S 访问 | 示例 |
|---------|---------|--------|------|
| GPIO | 允许 | 允许 | 通用 I/O |
| UART (调试) | 禁用 | 允许 | 安全日志 |
| SPI Flash | 限制 | 允许 | 固件存储 |
| Crypto Engine | 禁用 | 允许 | 加密加速 |

---

## 8. 认证与合规

### 8.1 认证路径

```
PSA Certified 是一家独立的第三方认证机构 (Brighest)
1. 开发者自行评估 (Self-assessment)
2. PSA Functional API 测试
3. 实验室安全评估 (Lab assessment, for L2/L3)
4. 颁发 PSA Certified 证书
```

### 8.2 相关标准

| 标准 | 与 PSA 的关系 |
|-----|-------------|
| NIST SP 800-53 | 安全控制框架 (同族) |
| SESIP | 物联网安全评估 (与 PSA L1 等效) |
| Common Criteria | IT 安全认证 (与 PSA L3 同族) |
| ISO 21434 | 汽车网络安全 (PSA 可映射) |

---

## 9. 本模块实现

`psa_certified.h/c` 实现了 PSA 核心概念：

- 安全等级验证 (Level 1-3)
- 安全模型配置 (isolation, secure boot, crypto, attestation)
- PSA Crypto API 模拟 (key gen, sign, verify, encrypt, decrypt)
- 初始认证令牌 (IAT) 生成和验证
- 固件更新验证
- 密钥派生 (HMAC-based)

---

## 参考资料

- ARM PSA Certified Security Model v2.0
- PSA Firmware Update API Specification
- PSA Initial Attestation API
- PSA Crypto API 1.0 Specification
- TF-M (Trusted Firmware-M) Reference Implementation
- NIST Cybersecurity for IoT Program
