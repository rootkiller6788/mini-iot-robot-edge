# mini-embedded-linux — 嵌入式Linux (C 语言实现)

嵌入式Linux系统构建与内核开发库，涵盖Yocto/Buildroot构建框架、设备树覆盖层、内核模块、
initramfs根文件系统、Busybox多调用小程序等核心概念。

## 目录结构

```
mini-embedded-linux/
├── README.md
├── Makefile
├── yocto_buildroot.h          # Yocto/Buildroot 构建框架接口
├── device_tree_overlay.h      # 设备树与覆盖层接口
├── kernel_module.h            # Linux 内核模块接口
├── initramfs_rootfs.h         # initramfs 与根文件系统接口
├── busybox_app.h              # Busybox 多调用小程序接口
├── yocto_buildroot.c          # Yocto/Buildroot 实现
├── device_tree_overlay.c      # 设备树覆盖层实现
├── kernel_module.c            # 内核模块实现
├── initramfs_rootfs.c         # initramfs 根文件系统实现
├── busybox_app.c              # Busybox 小程序实现
├── example_yocto_buildroot.c  # Yocto/Buildroot 示例
├── example_device_tree.c      # 设备树示例
├── example_kernel_module.c    # 内核模块示例
├── demo_embedded_linux.c      # 嵌入式Linux综合演示
├── demo_rootfs_boot.c         # 根文件系统与引导演示
├── doc_yocto_buildroot.txt    # Yocto/Buildroot 文档
└── doc_device_tree.txt        # 设备树文档
```

## 编译

```bash
make          # 编译所有目标
make clean    # 清理构建产物
make run      # 编译并运行演示
```

## 模块概述

| 模块 | 描述 |
|------|------|
| `yocto_buildroot` | Yocto layers/recipes/bitbake 任务执行，Buildroot Config.in/包管理，交叉编译工具链 |
| `device_tree_overlay` | DTS→DTB 编译，DTBO 片段与覆盖层合并，GPIO pin muxing |
| `kernel_module` | LKM init/cleanup，模块参数与依赖，tasklet/workqueue/proc，设备文件创建 |
| `initramfs_rootfs` | 引导序列：Bootloader→Kernel→initramfs→switch_root，Squashfs/UBIFS/Overlayfs |
| `busybox_app` | 单二进制多调用小程序，/etc/inittab 初始化，网络/系统/shell 功能 |

## 许可证

MIT License
