# mini-ota-update — OTA升级 (C 语言实现)

轻量级嵌入式 OTA（空中升级）固件更新框架，适用于 IoT 边缘设备和 MCU 平台。纯 C99 标准实现，无动态内存依赖（可配置分配器），支持 A/B 双分区、增量更新、签名验证和加密传输。

## 目录结构

```
mini-ota-update/
├── include/
│   ├── ab_update.h       # A/B 双分区更新
│   ├── delta_update.h    # Delta 增量更新 (bsdiff)
│   ├── ota_server.h      # OTA 服务器端
│   ├── ota_client.h      # OTA 客户端
│   └── signed_image.h    # 签名固件镜像
├── src/
│   ├── ab_update.c       # A/B 分区实现
│   ├── delta_update.c    # Delta 算法实现
│   ├── ota_server.c      # 服务端实现
│   ├── ota_client.c      # 客户端实现
│   └── signed_image.c    # 签名/加密实现
├── examples/
│   ├── basic_ota.c       # 基础 A/B OTA 示例
│   ├── delta_ota.c       # Delta 增量 OTA 示例
│   └── server_ota.c      # 服务器端管理示例
├── demos/
│   ├── ab_update_demo/   # A/B 更新演示文档
│   └── signed_ota_demo/  # 签名 OTA 演示文档
├── docs/
│   ├── ota_protocol.md   # OTA 协议规范
│   └── security_model.md # 安全模型文档
├── Makefile
└── README.md
```

## 功能特性

### 1. A/B 双分区更新 (`ab_update.h`)

- 双槽位 (active + standby) 架构
- Bootloader 自动选择可启动槽位
- 槽位元数据: 版本、CRC、可引导标志
- 更新流程: 下载到备用槽 → 校验 → 置为可引导 → 重启
- 回滚机制: 启动失败自动回退到上一版本
- 启动次数限制 (默认 7 次)

### 2. Delta 增量更新 (`delta_update.h`)

- bsdiff 算法变体: 后缀数组 + 差异编码
- 创建补丁: 旧固件 + 新固件 → 增量补丁
- 应用补丁: 旧固件 + 补丁 → 新固件
- 增量压缩 (RLE)
- 应用后哈希校验
- ECDSA 签名支持
- 部分升级支持

### 3. OTA 服务器 (`ota_server.h`)

- 设备注册与认证管理
- 固件版本管理与上传
- 发布频道: dev / beta / stable
- 设备分组与分阶段推送 (0% → 50% → 100%)
- 更新检查 API
- HTTPS 固件下载, 支持断点续传

### 4. OTA 客户端 (`ota_client.h`)

- 周期性更新检查
- 后台下载固件 (校验和验证)
- 安装到备用槽位
- 重启通知
- 更新进度报告
- 断电安全 (断电后恢复)
- 策略控制: 时间窗口 (如 2am-4am)、电量检查

### 5. 签名固件镜像 (`signed_image.h`)

- 镜像格式: 头部 (magic, version, size, signature)
- 密钥管理: 公钥嵌入 bootloader
- 启动前签名验证
- 防回滚: 版本单调计数器 (eFuse 或 TPM)
- AES-CTR 加密: 固件机密性保护

## 编译

```bash
# 编译所有模块
make all

# 编译示例
make examples

# 编译特定示例
make basic_ota
make delta_ota
make server_ota

# 清理
make clean

# 运行所有示例
make run
```

## 使用示例

### A/B 双分区更新

```c
#include "ab_update.h"

ab_update_hal_t hal = {
    .flash_read  = my_flash_read,
    .flash_write = my_flash_write,
    .flash_erase = my_flash_erase,
    .get_boot_count = my_boot_count_read,
    .reboot = my_reboot,
};

ab_update_ctx_t *ab = ab_update_init(&hal);
ab_update_partition_load(ab);

// 选择启动槽位
uint8_t boot_slot;
ab_update_select_boot_slot(ab, &boot_slot);
jump_to(boot_slot);

// ... 应用运行 ...

// 标记启动成功
ab_update_mark_boot_successful(ab);

// 下载新固件到备用槽
uint8_t standby;
ab_update_get_standby_slot(ab, &standby);
ab_update_write_to_standby(ab, fw_data, fw_size, 0);

// 校验并切换
ab_update_verify_standby(ab, expected_hash);
ab_update_set_standby_bootable(ab, 2, 0, 0);
ab_update_switch_and_reboot(ab);
```

### Delta 增量更新

```c
#include "delta_update.h"

// 创建补丁 (服务器端)
delta_create_patch(old_fw, old_size, new_fw, new_size,
                   &patch, &patch_size, &crypto, NULL, NULL);

// 应用补丁 (设备端)
delta_io_t io = {
    .read_old = my_read_old,
    .read_patch = my_read_patch,
    .write_new = my_write_new,
};
delta_apply_patch(&io, &crypto);
```

## 平台适配

本框架设计为平台无关，通过 HAL 接口适配不同 MCU:

```
┌──────────────────────────────────┐
│           应用层 (Application)    │
├──────────────────────────────────┤
│    ab_update / ota_client        │
├──────────────────────────────────┤
│     HAL 接口 (flash/net/...)     │
├──────────────────────────────────┤
│  MCU 驱动 (ESP32 / STM32 / ...)  │
└──────────────────────────────────┘
```

支持的 MCU 平台 (通过 HAL 适配):
- ESP32 / ESP32-S3 / ESP32-C3
- STM32F4 / STM32H7 / STM32L4
- nRF52 / nRF53 / nRF91
- Raspberry Pi Pico (RP2040)
- GD32 / AT32 (国产替代)

## API 设计原则

1. **C99 标准** — 无编译器扩展
2. **无全局变量** — 上下文指针传递
3. **可配置分配器** — `delta_alloc_fn` / `delta_free_fn`
4. **错误码统一** — 每个模块 `result_t` + `result_str()`
5. **HAL 抽象** — 硬件操作通过函数指针注入

## 安全机制

| 层次 | 机制 | 模块 |
|------|------|------|
| 传输 | TLS 1.2+, 证书锁定 | ota_client |
| 完整性 | SHA-256 哈希校验 | signed_image |
| 真实性 | ECDSA / Ed25519 签名 | signed_image |
| 机密性 | AES-256-CTR 加密 | signed_image |
| 防回滚 | 单调计数器 (eFuse/TPM) | signed_image |
| 防砖 | A/B 双槽 + 回滚 | ab_update |
| 防篡改 | Bootloader 公钥校验 | signed_image |

## 许可证

MIT License

## 参考

- [MCUBoot](https://www.mcuboot.com/) — 开源安全 bootloader
- [bsdiff](http://www.daemonology.net/bsdiff/) — 二进制差分算法
- [NIST SP 800-53](https://csrc.nist.gov/publications/detail/sp/800-53/rev-5/final) — 软件完整性
- [IEC 62443-4-2](https://www.isa.org/standards-and-publications/isa-standards/isa-iec-62443-series) — 工业 IoT 安全
