# mini-edge-security — 边缘安全 (C 语言实现)

> 参考 ARM PSA Certified, MIT 6.5950 Hardware Security, NIST IoT Security

## 模块-课程映射 (Module-Course Mapping)

| 模块 | 英文名 | 功能 | 对应标准 |
|-----|--------|------|---------|
| `secure_boot_mcu` | MCU 安全启动 | 信任链, 镜像签名, 防回滚, 调试认证 | PSA Level 2/3, NIST SP 800-193 |
| `trustzone_arm` | ARM TrustZone-M | 安全/非安全世界, SAU/IDAU, TF-M, 安全存储 | ARM PSA, TF-M Spec |
| `psa_certified` | PSA 认证 | Level 1-3, 安全模型, Crypto API, IAT, FW更新 | PSA Certified v2.0 |
| `device_attest` | 设备认证 | 设备ID, 认证令牌, X.509, AWS JITP, Azure DPS | PSA Attestation, IETF RATS |
| `secure_storage` | 安全存储 | 加密Flash, KDF, 文件系统, 防篡改, ATECC608 | PSA Secure Storage, NIST SP 800-57 |

## 目录树 (Directory Tree)

```
mini-edge-security/
├── include/
│   ├── secure_boot_mcu.h      # MCU 安全启动 (ROM->SPL->U-Boot->Kernel)
│   ├── trustzone_arm.h        # ARM TrustZone-M (SAU/IDAU, TF-M, S存储)
│   ├── psa_certified.h        # PSA Certified (Level 1-3, IAT, Crypto)
│   ├── device_attest.h        # 设备认证 (设备ID, X.509, JITP, Azure DPS)
│   └── secure_storage.h       # 安全存储 (加密Flash, KDF, ATECC608)
├── src/
│   ├── secure_boot_mcu.c      # 安全启动实现 (200+ 行, SHA256)
│   ├── trustzone_arm.c        # TrustZone 实现 (180+ 行)
│   ├── psa_certified.c        # PSA 认证实现 (170+ 行)
│   ├── device_attest.c        # 设备认证实现 (170+ 行)
│   └── secure_storage.c       # 安全存储实现 (200+ 行, CRC32)
├── examples/
│   ├── secure_boot_demo.c     # 安全启动演示
│   ├── attestation_demo.c     # 设备认证演示 (AWS+Azure+PSA)
│   └── secure_storage_demo.c  # 安全存储演示 (Flash+安全元件)
├── demos/
│   ├── mini-secure-boot/      # MCU 安全启动详解 (250+ 行)
│   │   └── README.md
│   └── mini-device-attest/    # 设备认证详解 (250+ 行)
│       └── README.md
├── docs/
│   ├── edge-security-primer.md    # 边缘安全入门
│   └── psa-iot-security.md        # PSA IoT 安全认证
├── README.md                   # 本文件
└── Makefile                    # 构建文件
```

## 构建与测试 (Build & Test)

```bash
# 编译所有库源文件
make all

# 编译并运行单独演示
make secure_boot_demo
make attestation_demo
make secure_storage_demo

# 运行所有演示
make test

# 清理构建产物
make clean
```

## 核心概念 (Core Concepts)

### MCU 安全启动 (Secure Boot)

- **信任链**: ROM → SPL → U-Boot → Kernel → App, 每一阶段验证下一阶段
- **镜像签名**: Header + Payload + ECDSA/RSA 签名
- **防回滚**: OTP 熔丝存储最小版本号, 启动时检查
- **SoC 实现**: NXP i.MX HAB, STM32 Secure Boot, ESP32 Secure Boot v2
- **调试认证**: 挑战-响应解锁 JTAG/SWD

### ARM TrustZone-M

- **双世界**: Secure World (S) / Non-Secure World (NS)
- **SAU/IDAU**: 硬件属性单元定义内存和外设的安全性
- **SG 调用**: 非安全世界通过 Secure Gateway 进入安全世界
- **TF-M**: ARM 开源的 PSA 参考实现, 管理安全分区
- **NV 计数器**: 单调递增, 用于防回滚

### PSA Certified

- **三个等级**: L1 (软件), L2 (硬件隔离), L3 (防篡改)
- **安全模型**: isolation + secure_boot + crypto + attestation
- **Crypto API**: 生成密钥, 签名/验证, 加密/解密
- **IAT**: 初始认证令牌 (设备ID+固件哈希+签名)
- **FW更新**: 新固件签名验证 + Double-bank 更新

### 设备认证 (Device Attestation)

- **设备ID密钥**: 每个设备唯一, 工厂注入, 安全元件保护
- **认证令牌**: 挑战-响应, 防重放 nonce
- **X.509 证书**: 工厂 CA 签发的设备证书链
- **AWS IoT Core**: JITP (Just-In-Time Provisioning) 自动注册
- **Azure DPS**: TPM 或 X.509 证书方式注册设备

### 安全存储 (Secure Storage)

- **加密 Flash 分区**: AES-CTR 加密, 完整性保护
- **HMAC-KDF**: 从设备密钥派生分区密钥
- **文件系统**: 加密分区上的简易文件系统
- **访问控制**: 每区域独立 R/W/X/D 权限
- **防篡改**: CRC 校验 + 单调计数器
- **安全元件**: ATECC608 (ECDH, ECDSA, RNG, 密钥存储)

## 参考资料 (References)

- ARM PSA Certified Security Model v2.0
- ARM TrustZone Technology for ARMv8-M
- TF-M (Trusted Firmware-M) Reference
- PSA Crypto API 1.0 Specification
- NIST SP 800-193: Platform Firmware Resiliency
- NIST SP 800-57: Key Management
- IETF RFC 9334: RATS (Remote Attestation ProcedureS)
- AWS IoT Core JITP Documentation
- Azure DPS Device Attestation
- Microchip ATECC608A Datasheet
- MIT 6.5950: Hardware Security Lectures
