# Mini Secure Boot MCU — MCU 安全启动详解

> 深入讲解嵌入式 MCU 安全启动机制：从 Boot ROM 到内核的信任链

---

## 1. 概述 (Overview)

安全启动 (Secure Boot) 是嵌入式设备安全的基石。它在启动过程中逐级验证每一阶段固件
的签名或哈希值，确保设备只运行受信的代码。本模块覆盖从 Boot ROM 第一阶段的不可变验证
到多级信任链的完整流程。

### 核心目标

| 目标 | 描述 |
|-----|------|
| 完整性 | 保证固件未被篡改 |
| 真实性 | 保证固件来自受信的签发者 |
| 版本防回滚 | 防止攻击者刷入旧版本（含已知漏洞） |
| 调试认证 | 安全地启用调试以支持开发/维修 |

---

## 2. 信任链模型 (Chain of Trust)

```
  Boot ROM (不可变，出厂固化)
     │
     │  验证 SPL 签名/哈希
     v
  SPL (Secondary Program Loader)
     │
     │  验证 U-Boot 签名/哈希
     v
  U-Boot (引导加载程序)
     │
     │  验证内核签名/哈希
     v
  Kernel (操作系统内核)
     │
     │  验证应用签名/哈希
     v
  Application (用户应用)
```

### 信任根 (Root of Trust)

信任根必须存储在不可修改的介质中：通常是 Boot ROM (芯片制造时写入)，其哈希值
是公认正确的。公钥哈希存储在 OTP (One-Time Programmable) 熔丝中，一旦写入永不可改。

---

## 3. 镜像签名格式 (Image Signature Format)

```
┌──────────────────────────────────────────────┐
│  Image Header (64 bytes)                     │
│  ┌──────────────────────────────────────────┐ │
│  │ magic[4]      : "SBOT"                   │ │
│  │ version       : 固件版本号               │ │
│  │ image_size    : 负载大小                  │ │
│  │ load_addr     : 加载地址                  │ │
│  │ entry_point   : 入口地址                  │ │
│  │ image_hash    : SHA256 哈希              │ │
│  │ pubkey_hash   : 签发者公钥哈希            │ │
│  │ flags         : 属性标志                  │ │
│  │ rollback_ctr  : 版本计数器                │ │
│  └──────────────────────────────────────────┘ │
├──────────────────────────────────────────────┤
│  Payload (固件负载)                          │
│  (最大 256KB)                                 │
├──────────────────────────────────────────────┤
│  Signature (256 bytes)                       │
│  (ECDSA/RSA 签名)                            │
└──────────────────────────────────────────────┘
```

验证流程：
1. 检查 magic 是否为 "SBOT"
2. 检查 rollback_ctr >= 已记录的最小版本
3. 对 header + payload 计算哈希
4. 用存储的公钥验证签名

---

## 4. SoC 安全启动实现

### 4.1 NXP i.MX — HAB (High Assurance Boot)

| 阶段 | 存储位置 | 验证方式 |
|-----|---------|---------|
| Boot ROM | 芯片内 | 不可变 |
| SPL | SPI Flash / eMMC | HAB 验证 (RSA 2048) |
| U-Boot | 同 SPL | HAB 验证 |
| Kernel | 同 SPL | FIT Image (哈希验证) |

HAB 使用 CSF (Command Sequence File) 描述签名信息。CST (Code Signing Tool)
用于给固件签名。

### 4.2 STM32 — Secure Boot

| 阶段 | 存储位置 | 验证方式 |
|-----|---------|---------|
| Boot ROM | 系统内存 | 不可变 |
| SBSFU | 用户 Flash | ECDSA P-256 |
| App | 用户 Flash | ECDSA P-256 |

STM32 使用 SBSFU (Secure Boot & Secure Firmware Update) 包实现安全启动，
提供两阶段验证 (Loader + App) 和固件回滚保护。

### 4.3 ESP32 — Secure Boot v2

ESP32 Secure Boot v2 使用 RSA-3072，将签名存储在 Flash 中。
Boot ROM 验证 Bootloader，Bootloader 验证 App。

---

## 5. 版本防回滚 (Anti-Rollback)

| 机制 | 实现方式 | 特点 |
|-----|---------|------|
| 版本计数器 | OTP 熔丝中记录最小可接受版本 | 不可逆 |
| 单调计数器 | NV 计数器 (Flash 中) | 可增不可减 |
| eFuse 位 | 每位只能从 0 写为 1 | SoC 硬件支持 |

攻击场景：攻击者拥有老版本的合法签名固件（含已知漏洞），试图刷入设备。

防御：设备检查 `rollback_ctr >= min_version[N]`，若不满足则拒绝启动。

---

## 6. 启动失败策略 (Boot Failure Policy)

| 策略 | 行为 |
|-----|------|
| FAIL_STOP | 停止启动，进入死循环或复位 |
| FAIL_RECOVER | 尝试回退到前一版本或恢复分区 |
| FAIL_REPORT | 记录失败事件并上报安全日志 |
| FAIL_DEBUG | 进入受限的诊断模式（仅特定开发密钥） |

---

## 7. 调试认证 (Debug Authentication)

生产设备默认锁定调试接口 (JTAG/SWD)，攻击者可通过物理攻击提取固件。

调试认证解决了安全锁与开发需求之间的矛盾：

1. 开发者提交公钥给设备制造商
2. 设备 OTP 中烧入开发者公钥哈希
3. 设备发起挑战 (随机数) 发送给开发者
4. 开发者用私钥签名挑战返回响应
5. 设备验证通过 → 解锁调试接口 (可设置超时或下次锁定)

---

## 8. 证书链 (Certificate Chain)

```
根 CA 证书
  │  存储在 OTP 中 (公钥哈希)
  v
中间 CA 证书
  │  验证映像中附带的签名
  v
设备证书 / 固件签名
  │  验证固件哈希
  v
固件启动
```

多个级别的证书链允许固件更新者 (如 OTA 服务) 的密钥与芯片信任根分离，
增加了安全性：即使 OTA 密钥泄露，芯片信任根也不受影响。

---

## 9. 侧信道与攻击面

| 攻击 | 目标 | 缓解 |
|-----|------|------|
| 故障注入 | 跳过签名验证分支 | 双重检查 + 默认拒绝 |
| 时序攻击 | 分析 RMA 比较分支 | 常数时间比较 |
| 物理探测 | 从 Flash 直接读取 | Flash 加密 + RDP |
| 降级攻击 | 刷入旧版固件 | 版本防回滚 |

---

## 10. 本模块实现

本模块 (`secure_boot_mcu.h/c`) 提供了：

- `secure_boot_init()`: 初始化安全启动上下文
- `secure_boot_rom_verify()`: Boot ROM 验证 SPL
- `secure_boot_chain_verify()`: 链式验证任一级跳转
- `secure_boot_verify_image()`: 完整镜像签名验证
- `secure_boot_check_rollback()`: 版本防回滚检查
- `secure_boot_debug_auth()`: 调试认证挑战-响应
- `secure_boot_cert_chain_verify()`: 证书链验证
- `secure_boot_otp_read/write()`: OTP 熔丝操作
- 内嵌 SHA256 哈希实现

---

## 参考资料

- NXP i.MX High Assurance Boot (HAB) Reference Manual
- AN5055: STM32 SBSFU Getting Started
- ESP32 Secure Boot v2 Documentation
- UEFI Secure Boot Specification
- MIT 6.5950: Hardware Security Lecture on Secure Boot
