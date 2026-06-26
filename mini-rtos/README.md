# mini-rtos — 实时操作系统 (C 语言实现)

mini-rtos 是一个轻量级实时操作系统，使用 C99 编写，面向 ARM Cortex-M 系列 MCU。
支持优先级抢占调度、IPC 消息队列、信号量与互斥锁、软件定时器、动态内存管理等核心功能。

## 特性

| 模块 | 功能 |
|------|------|
| **任务调度** | 优先级抢占 + 同优先级轮转, PendSV 上下文切换, 空闲任务, SysTick 滴答 |
| **IPC 队列** | 环形缓冲消息队列, 超时阻塞, FromISR 变体, 队列集合多路接收 |
| **信号量与互斥** | 二值/计数信号量, 互斥锁优先级继承, 递归互斥锁, Gatekeeper 模式 |
| **软件定时器** | 单次/自动重载, 定时器命令队列, 守护任务回调, 延迟中断处理 |
| **内存管理** | heap_1~heap_5 五种算法, 空闲链表, 栈溢出检测水印, MPU 栈保护 |

## 目录结构

```
mini-rtos/
├── inc/                    # 头文件
│   ├── task_scheduler.h
│   ├── ipc_queue.h
│   ├── semaphore_mutex.h
│   ├── software_timer.h
│   └── memory_heap.h
├── src/                    # 源文件
│   ├── task_scheduler.c
│   ├── ipc_queue.c
│   ├── semaphore_mutex.c
│   ├── software_timer.c
│   └── memory_heap.c
├── examples/               # 入门示例
│   ├── example_blinky.c
│   ├── example_ipc.c
│   └── example_semaphore.c
├── demos/                  # 综合演示
│   ├── demo_priority_preemptive.c
│   └── demo_producer_consumer.c
├── docs/                   # 文档
│   ├── API_REFERENCE.md
│   └── PORTING_GUIDE.md
├── Makefile
└── README.md
```

## 快速开始

```bash
make          # 编译所有目标
make examples # 仅编译示例
make demos    # 仅编译演示
make clean    # 清理
```

## 最小系统示例

```c
#include "task_scheduler.h"

void task_a(void *param) {
    while (1) { toggle_led(); task_delay(500); }
}

int main(void) {
    scheduler_init();
    task_create(task_a, "task_a", 256, NULL, 1);
    scheduler_start();
    return 0;
}
```

## 移植要求

- ARM Cortex-M3/M4/M7 架构
- SysTick 定时器提供时基
- PendSV 异常用于上下文切换
- 需要实现 `port.c` 中的底层汇编：`port_start_first_task()`, `port_enable_faults()`, PendSV_Handler, SysTick_Handler

详见 `docs/PORTING_GUIDE.md`。

## 许可

MIT License
