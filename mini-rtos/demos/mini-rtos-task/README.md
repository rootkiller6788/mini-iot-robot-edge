# mini-rtos-task — 优先级抢占与轮转调度综合演示

## 概述

`demo_priority_preemptive.c` 综合演示 mini-rtos 任务调度器的核心能力：优先级抢占调度、同优先级时间片轮转、空闲任务、PendSV 上下文切换、基于互斥锁的优先级继承、延时执行、周期性任务、软件定时器回调以及看门狗监控。

该演示创建 **10 个用户任务 + 定时器守护任务 + 空闲任务**，在一个共享计数器的竞争环境中验证调度器的正确性和稳定性。

## 调度器架构

### 优先级模型

| 优先级值 | 含义 | 宏定义 |
|---------|------|--------|
| 0 | 最低优先级（空闲任务） | `TASK_PRIORITY_IDLE` |
| 1 ~ 30 | 用户任务优先级 | — |
| 31 | 最高优先级 | `TASK_PRIORITY_HIGHEST` |

数值越大优先级越高。调度器总是选择就绪队列中**优先级最高**的任务运行。同优先级任务以 **Round-Robin** 方式分配时间片。

### 上下文切换路径

```
SysTick_Handler ──> scheduler_tick_handler()
                       │
                       ├── 更新系统滴答计数器
                       ├── 递减阻塞任务剩余延时
                       ├── 唤醒到期任务
                       ├── 检查时间片耗尽
                       └── 触发 PendSV
                              │
                  PendSV_Handler ──> pendsv_handler()
                                        │
                                        ├── 保存当前任务上下文 (R4-R11, LR, PSP)
                                        ├── 选择下一个就绪任务
                                        └── 恢复目标任务上下文
```

### 空闲任务

当没有用户任务就绪时，调度器运行内部空闲任务（优先级 0）。空闲任务执行 `WFI` 指令使 CPU 进入低功耗休眠，被 SysTick 中断唤醒后重新检查就绪队列。

## 演示任务拓扑

```
优先级 4  ──► task_watchdog         (看门狗, 5s周期)
优先级 3  ──► task_high_priority    (高优先级, 抢占+互斥锁竞争)
优先级 2  ──► task_medium_priority  (med0, 中优先级计算密集型)
            ├─ task_medium_priority (med1, 中优先级计算密集型)
            ├─ task_periodic_worker (pwork0, 周期性增量)
            └─ task_periodic_worker (pwork1, 周期性增量)
优先级 1  ──► task_low_priority     (低优先级, 持有互斥锁)
            ├─ task_monitor         (监控统计)
            ├─ task_auxiliary_worker(aux0, 辅助工作)
            └─ task_auxiliary_worker(aux1, 辅助工作)
```

## 核心场景验证

### 场景 1: 优先级继承防止反转

这是 RTOS 中最经典的优先级反转场景：

```
时序:
  T0: 低优先级任务获取互斥锁 counter_mutex
  T1: 中优先级任务就绪, 抢占低优先级任务运行
      ▸ 低优先级任务被挂起, 但仍持有锁
  T2: 高优先级任务就绪并尝试获取 counter_mutex
      ▸ 高优先级阻塞在互斥锁上
  T3: mini-rtos 检测到优先级反转, 将低优先级任务
      临时提升到高优先级任务的优先级 (优先级继承)
  T4: 低优先级(继承后)抢回CPU, 完成临界区并释放锁
  T5: 高优先级立即获得锁, 继续执行
  T6: 低优先级恢复到原始优先级
```

验证标志:
- `priority_inherited` — 低优先级持有锁期间是否检测到被提升
- `priority_restored` — 释放锁后是否恢复原始优先级
- `low_prio_blocked` — 低优先级被抢占后是否处于阻塞状态
- `high_prio_activated` — 高优先级是否成功运行

### 场景 2: 同优先级轮转调度

两个中优先级任务 (`med0`, `med1`) 共享相同的优先级 2, 各配置不同的 CPU 突发长度 (50000 / 30000 周期)。调度器在时间片耗尽时切换到另一个同优先级任务, 确保公平性。

```c
// 源码验证逻辑
med_prio_running = 1;
simulate_cpu_work(w->burst_count);  // 模拟计算工作
med_prio_running = 0;
total_context_switches++;           // 记录切换次数
```

### 场景 3: 精确周期性执行

周期性任务使用 `task_delay_until()` 实现固定频率执行, 避免累积漂移:

```c
uint32_t last_wake = task_get_tick_count();
while (1) {
    // ... 执行工作 ...
    task_delay_until(&last_wake, 100);  // 每 100 ticks 精确执行
}
```

与普通 `task_delay()` 不同的是, `task_delay_until()` 从 **上次预期的唤醒时刻** 计算下次唤醒, 消除了执行时间波动对周期的影响。

### 场景 4: 软件定时器守护任务

演示创建了一个自动重载软件定时器, 由独立的定时器守护任务管理:

```c
timer_daemon_init(512, 4);                    // 守护任务栈512B, 优先级4
stats_timer = timer_create("stats_tmr",
                           timer_stats_callback,
                           NULL,
                           1000,              // 周期1000 ticks
                           TIMER_TYPE_AUTO_RELOAD);
timer_start(stats_timer, 1000);              // 首次延迟1000 ticks后启动
```

定时器回调在守护任务的上下文中执行, 而不是中断上下文, 因此可以使用阻塞 API。命令队列 (`timer_cmd_t`) 解耦了中断生产者 (SysTick) 和定时器消费者 (守护任务)。

### 场景 5: 看门狗死锁检测

独立看门狗任务每 5 秒采样共享计数器, 若连续两次采样值相同则判定为死锁:

```c
if (shared_counter == last_total) {
    deadlock_detected = 1;  // 无进展, 疑似死锁
}
```

## 关键 API 使用

### 调度器初始化与启动

```c
scheduler_init();    // 初始化就绪链表、优先级位图、SysTick
// ... 创建任务 ...
scheduler_start();   // 设置 PendSV, 启动第一个任务, 永不返回
```

### 任务创建

```c
tcb_t *task_create(task_entry_t entry,    // 入口函数
                   const char *name,      // 任务名称 (<16字节)
                   uint32_t stack_size,   // 栈大小(字)
                   void *param,           // 入口参数 (或 NULL)
                   uint32_t priority);    // 优先级 0-31
```

### 延时与让步

| API | 行为 |
|-----|------|
| `task_delay(ticks)` | 阻塞当前任务指定 tick 数, 调度下一个就绪任务 |
| `task_delay_until(&last, period)` | 阻塞到 `last + period`, 自动更新 `last` |
| `task_yield()` | 主动放弃剩余时间片, 同优先级轮转 |
| `task_get_tick_count()` | 获取系统滴答计数器当前值 |

### 临界区保护

```c
task_enter_critical();   // 屏蔽 PendSV/SysTick 中断
// ... 原子操作 ...
task_exit_critical();    // 恢复中断
```

仅在极短的操作中使用临界区。长时间互斥应使用互斥锁或信号量。

### 互斥锁操作

```c
mutex_t *mutex_create(const char *name);                // 创建互斥锁
int32_t  mutex_lock(mutex_t *m, uint32_t timeout);      // 获取锁 (可超时)
int32_t  mutex_unlock(mutex_t *m);                      // 释放锁
tcb_t   *mutex_get_owner(const mutex_t *m);             // 查询当前持有者
int32_t  mutex_lock_recursive(mutex_t *m, uint32_t t);  // 递归获取
int32_t  mutex_unlock_recursive(mutex_t *m);            // 递归释放
```

`timeout` 参数: `0` 表示不阻塞立即返回, `0xFFFFFFFF` 表示无限等待。

## 栈溢出检测

每个任务创建时栈空间被填充水印值 `0xA5A5A5A5`。可调用以下函数检查栈使用情况:

```c
void     task_stack_watermark_check(tcb_t *task); // 检查水印完整性
uint32_t task_stack_free(tcb_t *task);            // 剩余栈空间 (字节)
```

若定义了 `MPU_ENABLED`, 还可使用 MPU 保护任务栈的底部区域, 越界访问将触发 MemManage 异常。

## 性能计数观测

演示维护了以下全局观测变量, 可通过调试器或串口输出监控:

| 变量 | 含义 |
|------|------|
| `total_context_switches` | 累积上下文切换次数 |
| `shared_counter` | 受互斥锁保护的共享计数器 |
| `deadlock_detected` | 看门狗检测到疑似死锁 |
| `priority_inherited` | 发生过优先级继承 |
| `priority_restored` | 优先级已恢复 |

## 构建与运行

```bash
# 在工作区根目录 (mini-rtos/)
make demos
```

生成的 ELF 文件位于 `build/demo_priority_preemptive.elf`。使用 GDB + OpenOCD / J-Link 加载到目标硬件:

```bash
arm-none-eabi-gdb build/demo_priority_preemptive.elf \
  -ex "target remote :3333" \
  -ex "monitor reset halt" \
  -ex "load" \
  -ex "continue"
```

## 调试要点

1. **观察 PendSV**: 在 `pendsv_handler()` 设置断点, 每次上下文切换触发
2. **检查优先级位图**: 调用 `task_list_debug()` 打印所有任务状态
3. **栈溢出检测**: 周期性调用 `task_stack_watermark_check()` 确认水印完整
4. **优先级继承验证**: 在 `mutex_lock` 内设断点, 观察 `owner_original_priority` 字段变化
5. **时间片验证**: 观察两个同优先级任务的 `runtime_ticks` 累计情况

## 预期行为

- `shared_counter` 持续单调递增
- `deadlock_detected` 始终保持 0
- `priority_inherited` 在运行初期短暂置位后清除
- `total_context_switches` 持续增长
- 无栈溢出、无硬错误 (HardFault)
- 空闲任务在无就绪任务时进入 WFI

## 相关文档

- [任务调度器 API 参考](../docs/API_REFERENCE.md) — 完整 API 说明
- [移植指南](../docs/PORTING_GUIDE.md) — PendSV / SysTick 移植
- 头文件: `inc/task_scheduler.h`, `inc/semaphore_mutex.h`, `inc/software_timer.h`
- 源文件: `src/task_scheduler.c`, `src/semaphore_mutex.c`, `src/software_timer.c`
