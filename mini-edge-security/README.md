# mini-edge-security — 边缘安全 (C 语言实现)

> 参考 ARM PSA Certified, MIT 6.5950 Hardware Security, NIST IoT Security

## Module Status: COMPLETE ✅

- **include/ + src/ 总行数**: 3126 行 (≥ 3000)
- **make test**: 43/43 测试通过 ✅
- **L1-L6**: Complete ✅
- **L7**: Complete ✅ (AWS IoT, Azure DPS, JITP, Fleet Provisioning, CRL checking)
- **L8**: Partial ✅ (Constant-time memcmp, Montgomery ladder, secure zero, key wrapping)
- **L9**: Partial ✅ (Post-quantum readiness, binary delta updates, PSA Level 3 attestation)

## 模块-课程映射 (Module-Course Mapping)

| 模块 | 英文名 | 功能 | 对应标准 |
|-----|--------|------|---------|
| `secure_boot_mcu` | MCU 安全启动 | 信任链, 镜像签名, 防回滚, 调试认证, 度量启动, HSM | PSA Level 2/3, NIST SP 800-193 |
| `trustzone_arm` | ARM TrustZone-M | 安全/非安全世界, SAU/IDAU, TF-M, MPU, 安全存储, 故障处理 | ARM PSA, TF-M Spec |
| `psa_certified` | PSA 认证 | Level 1-3, 安全模型, Crypto API, IAT, FW更新, 密钥派生 | PSA Certified v2.0 |
| `device_attest` | 设备认证 | 设备ID, 认证令牌, X.509, AWS JITP, Azure DPS, DICE, TPM PCR, RATS | PSA Attestation, IETF RATS, TCG DICE |
| `secure_storage` | 安全存储 | 加密Flash, KDF, 文件系统, 防篡改, ATECC608, CRC32, 单调计数器 | PSA Secure Storage, NIST SP 800-57 |
| `edge_crypto` | 边缘密码学 | AES-128/CTR/GCM, SHA-256, HMAC-SHA256, HKDF, ECDSA, ECDH, 常数时间运算 | FIPS 197, FIPS 180-4, FIPS 186-4, RFC 5869 |
| `edge_ota` | 安全固件更新 | 双Bank架构, 清单验证, Delta更新, 防回滚, Golden Image, AWS IoT Jobs | NIST SP 800-193, PSA Firmware Update |
| `edge_key_mgmt` | 密钥管理 | PKI, 证书链, 密钥生成/导入/轮换/吊销, CRL, CSR, AES Key Wrap | RFC 5280, RFC 3394, NIST SP 800-57 |

## 目录树 (Directory Tree)

```
mini-edge-security/
├── include/
│   ├── secure_boot_mcu.h      # MCU 安全启动 (104行)
│   ├── trustzone_arm.h        # ARM TrustZone-M (109行)
│   ├── psa_certified.h        # PSA Certified (118行)
│   ├── device_attest.h        # 设备认证 (92行)
│   ├── secure_storage.h       # 安全存储 (113行)
│   ├── edge_crypto.h          # 边缘密码学引擎 (147行)
│   ├── edge_ota.h             # 安全固件更新 (99行)
│   └── edge_key_mgmt.h        # 密钥管理 (123行)
├── src/
│   ├── secure_boot_mcu.c      # 安全启动+度量启动+HSM (314行)
│   ├── trustzone_arm.c        # TrustZone+MPU+故障处理 (273行)
│   ├── psa_certified.c        # PSA认证+Crypto+Token (254行)
│   ├── device_attest.c        # 设备认证+DICE+RATS+TPM (298行)
│   ├── secure_storage.c       # 安全存储+安全元件 (265行)
│   ├── edge_crypto.c          # AES/GCM/SHA/HMAC/HKDF/ECDSA/ECDH (312行)
│   ├── edge_ota.c             # OTA双Bank+Delta+清单验证 (246行)
│   └── edge_key_mgmt.c        # PKI+证书+密钥轮换+CRL (259行)
├── tests/
│   └── test_core.c            # 43个单元测试, 全通过
├── examples/
│   ├── secure_boot_demo.c     # 安全启动8阶段演示
│   ├── attestation_demo.c     # 设备认证10步骤演示
│   └── secure_storage_demo.c  # 安全存储10测试演示
├── README.md                   # 本文件
└── Makefile                    # make test 一键通过
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

## 九层知识覆盖 (Knowledge Level Coverage)

| Level | 名称 | 状态 | 关键实现 |
|-------|------|------|---------|
| **L1** | Definitions | **Complete** | 30+ structs/typedefs: SecureBootCtx, TrustZoneCore, EdgeCryptoCtx, OtaUpdateCtx, EdgeKeyMgmtCtx 等 |
| **L2** | Core Concepts | **Complete** | Secure Boot Chain, TrustZone Dual-World, PSA Level 1-3, DICE Attestation, GCM auth encryption |
| **L3** | Engineering Structures | **Complete** | Double-Bank Flash, SAU/IDAU配置, MPU区域, Key Hierarchy (Root→CA→Device), TF-M分区 |
| **L4** | Standards/Theorems | **Complete** | FIPS 197 (AES), FIPS 180-4 (SHA-256), FIPS 186-4 (ECDSA), FIPS 198-1 (HMAC), NIST SP 800-38A/D, NIST SP 800-56A, NIST SP 800-193, RFC 5280, RFC 5869, RFC 3394 |
| **L5** | Algorithms/Methods | **Complete** | AES-128 (SubBytes/ShiftRows/MixColumns), SHA-256, HMAC-SHA256, HKDF Extract/Expand, GCM-GHASH, ECDSA sign/verify, ECDH, Montgomery ladder, CRC32 |
| **L6** | Canonical Problems | **Complete** | Secure Boot Chain (ROM→SPL→U-Boot→Kernel), OTA Update Pipeline (download→verify→apply→commit), Certificate Chain Validation, Measured Boot with PCR |
| **L7** | Applications | **Complete** | AWS IoT JITP (Just-In-Time Provisioning), Azure DPS Symmetric Key Attestation, AWS Fleet Provisioning, CRL checking, CSR export |
| **L8** | Advanced Topics | **Partial+** | Constant-time memcmp (side-channel resistant), Montgomery ladder (SPA protection), Secure memory zeroing, AES Key Wrap (RFC 3394), TF-M fault handling |
| **L9** | Industry Frontiers | **Partial** | Binary delta updates (bsdiff-like), Post-quantum readiness indicator, PSA Level 3 attestation token, DICE layered attestation |

## 核心定理列表 (Core Theorems)

| 定理 | 标准 | 实现 |
|------|------|------|
| AES block cipher security (Rijndael) | FIPS 197 | `aes_encrypt_block()` — 10-round AES-128 |
| SHA-256 collision resistance | FIPS 180-4 | `edge_sha256()` — Merkle-Damgard construction |
| HMAC pseudorandomness | FIPS 198-1 | `edge_hmac_sha256()` — nested hash |
| GCM authenticated encryption | NIST SP 800-38D | `edge_gcm_encrypt/decrypt()` — CTR+GHASH |
| CTR mode IND-CPA | NIST SP 800-38A | `edge_aes_ctr_encrypt()` — counter-based keystream |
| ECDSA existential unforgeability | FIPS 186-4 | `edge_ecdsa_sign/verify()` — P-256 curve |
| ECDH key agreement | NIST SP 800-56A | `edge_ecdh_compute_shared()` |
| HKDF key derivation | RFC 5869 | `edge_hkdf_extract/expand()` |
| Constant-time comparison (DPA resistance) | Cryptography Engineering | `edge_constant_time_memcmp()` |

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
