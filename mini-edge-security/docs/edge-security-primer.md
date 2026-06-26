# 边缘安全入门 (Edge Security Primer)

> 全面介绍 IoT 边缘设备的安全威胁模型、防护体系和技术栈

---

## 1. 边缘安全威胁模型 (Edge Threat Model)

### 1.1 攻击者分类

| 攻击者级别 | 能力 | 目标 |
|-----------|------|------|
| 远程 (网络) | 通过网络连接攻击 | 数据窃取、DDoS、固件篡改 |
| 本地 (物理) | 持有设备，可拆解 | 提取密钥、固件逆向、故障注入 |
| 供应链 | 在生产/分发环节介入 | 植入后门、克隆设备 |

### 1.2 攻击面分析

```
                    ┌─────────────────────────┐
                    │    云平台 (安全)          │
                    └──────────┬──────────────┘
                               │ 网络通信
                    ┌──────────┴──────────────┐
                    │   边缘网络设备            │
                    │   - 开放端口              │
                    │   - 未加密通信            │
                    │   - 弱认证机制            │
                    └──────────┬──────────────┘
                               │ 本地总线 (SPI/I2C)
                    ┌──────────┴──────────────┐
                    │   MCU / 传感器            │
                    │   - JTAG/SWD 调试接口     │
                    │   - 未加密 Flash          │
                    │   - 侧信道泄露            │
                    │   - 故障注入脆弱性        │
                    └─────────────────────────┘
```

### 1.3 安全属性 CIA+CIA

| 属性 | 描述 | 边缘挑战 |
|-----|------|---------|
| Confidentiality | 数据保密 | 内存加密、安全存储 |
| Integrity | 数据完整性 | 安全启动、固件签名 |
| Availability | 服务可用 | DoS 防护、看门狗 |
| Authentication | 身份认证 | 设备认证、双向 TLS |
| Authorization | 访问控制 | PSA 隔离、TrustZone |
| Auditability | 审计日志 | 安全计数、日志记录 |

---

## 2. 安全启动 (Secure Boot)

### 2.1 信任链逐级验证

```
1. Boot ROM (不可变)
   → 验证 SPL 公钥签名
   → 验证成功 → 执行 SPL

2. SPL (Secondary Program Loader)
   → 验证 U-Boot 公钥签名
   → 验证成功 → 执行 U-Boot

3. U-Boot
   → 验证 Kernel 公钥签名 / FIT Image 哈希
   → 验证成功 → 执行 Kernel
```

### 2.2 版本防回滚

OTP 中存储 min_version，每个启动阶段检查 `current_version >= min_version`。
若版本小于阈值则拒绝启动。

---

## 3. ARM TrustZone-M

### 3.1 安全世界隔离

| 对比项 | 非安全世界 | 安全世界 |
|-------|-----------|---------|
| CPU 模式 | Thread/Handler (NS) | Thread/Handler (S) |
| 内存访问 | 只能访问 NS 内存 | 可访问全部内存 |
| 外设 | 仅 NS 外设 | 安全外设 |
| SAU 属性 | NS / NSC | S |
| 函数调用 | 普通函数 | SG (Secure Gateway) |

### 3.2 TF-M (Trusted Firmware-M)

ARM 开源的 PSA 参考实现：
- PSA RoT (Root of Trust): 密钥管理
- 安全分区: 强制隔离
- 加密服务: PSA Crypto API
- 安全存储: 加密键值
- 认证: 平台初始认证

---

## 4. PSA 认证框架

### 4.1 三个等级

| 等级 | 要求 | 防护场景 |
|-----|------|---------|
| Level 1 | 最小安全要求 | 软件攻击 |
| Level 2 | 硬件隔离 | 扩展到硬件攻击 |
| Level 3 | 防篡改保护 | 物理攻击 |

### 4.2 安全模型组件

```
Level 1:
  ◆ 加密 API (PSA Crypto API)
  ◆ 安全启动

Level 2 (Level 1 +):
  ◆ ARM TrustZone 隔离
  ◆ 安全存储

Level 3 (Level 2 +):
  ◆ 设备认证 (IAT)
  ◆ 固件更新验证
  ◆ 防篡改硬件
```

---

## 5. 设备认证 (Device Attestation)

### 5.1 工厂流程

```
工厂:
  1. 生成设备 UID
  2. 生成/注入设备密钥对 (ATECC608 / TPM)
  3. 设备公钥 → 工厂 CA 签名 → 设备证书
  4. 设备证书烧录到安全存储
  5. 私钥永不可导出

云端:
  1. 接收设备连接 (TLS + device cert)
  2. 验证证书链 (CA 根 → 设备证书)
  3. 检查证书吊销状态
  4. 授权设备访问特定资源
```

---

## 6. 安全存储 (Secure Storage)

### 6.1 加密策略

| 组件 | 实现 |
|-----|------|
| 根密钥 | 芯片唯一密钥 (HW key) |
| 派生方式 | HMAC-based KDF (HKDF) |
| 加密算法 | AES-CTR / AES-GCM |
| 完整性保护 | CRC32 + 防篡改检测 |
| 单调计数器 | Flash 或 NV 计数器 |

### 6.2 安全元件 (ATECC608)

| 特性 | 说明 |
|-----|------|
| ECDH | 密钥交换 (P-256) |
| ECDSA | 签名和验证 |
| RNG | 真随机数生成 |
| 密钥存储 | 16 个槽位 (每个独立) |
| OTP 区域 | 64 字节一次性可编程 |
| 安全启动 | 与主 MCU 协作验证 |

---

## 7. 通信安全

### 7.1 协议层保护

| 协议 | 安全特性 |
|-----|---------|
| MQTT + TLS | 双向认证 (设备证书) + 加密传输 |
| CoAP + DTLS | 受限环境中的 TLS 等效 |
| LwM2M | OMA 轻量级设备管理 + DTLS |
| LoRaWAN | AES 128 会话密钥 (AppSKey + NwkSKey) |

### 7.2 密钥管理最佳实践

- 设备密钥永不离开设备
- 会话密钥: 每次连接重新派生
- 密钥轮换: 定时或事件触发
- 密钥销毁: 检测到攻击时立即擦除

---

## 8. 防御体系总结

```
Layer 1: 物理安全
  - 防篡改硬件
  - 安全元件
  - JTAG 锁定

Layer 2: 启动安全
  - 安全启动 (Secure Boot)
  - 版本防回滚
  - 调试认证

Layer 3: 运行安全
  - TrustZone/TF-M 隔离
  - 安全存储
  - PSA Crypto API

Layer 4: 通信安全
  - 设备认证 (Attestation)
  - TLS/DTLS
  - 单向/双向认证
```

---

## 9. 参考资料

- PSA Certified Security Framework v2.0
- ARM TrustZone Technology for ARMv8-M
- NIST SP 800-57: Recommendation for Key Management
- OWASP IoT Top 10 (2024)
- MIT 6.5950: Hardware Security Course Notes
