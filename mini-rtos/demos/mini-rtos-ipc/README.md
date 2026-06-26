# mini-rtos-ipc — 队列、信号量、互斥锁综合演示

## 概述

`demo_producer_consumer.c` 综合演示 mini-rtos 的进程间通信 (IPC) 子系统：消息队列、计数信号量、互斥锁 (含优先级继承)、内存堆管理、软件定时器回调以及看门狗监控。

该演示构建了经典的 **多生产者-多消费者** 模型: 5 个生产者向 32 槽环形队列写入数据, 3 个消费者从中读取数据。计数信号量控制缓冲区空/满槽位, 互斥锁保护管道和统计数据的原子访问。软件定时器定期报告系统健康状况和资源利用率。

## IPC 子系统架构

### 消息队列 — 环形缓冲

```
queue_t 结构:
┌──────────────┐
│ buffer       │ ──► ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
│ item_size    │     │ 0 │ 1 │ 2 │ 3 │ 4 │ 5 │...│...│...│...│...│31 │
│ max_items=32 │     └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘
│ head         │          ▲                       ▲
│ tail         │          │ tail (写入位置)        │ head (读取位置)
│ count        │
│ blocked_senders    ──► 当 count == max_items 时阻塞发送者
│ blocked_receivers  ──► 当 count == 0       时阻塞接收者
│ isr_safe           ──► 中断安全标志
└──────────────┘
```

队列特性:
- **环形缓冲区**, O(1) 入队/出队
- **按值拷贝**: `queue_send` 将数据 memcpy 到内部缓冲区
- **超时阻塞**: 发送方满队列时阻塞等待, 接收方空队列时阻塞等待
- **FromISR 变体**: `queue_send_from_isr` / `queue_receive_from_isr` 不阻塞, 通过 `*woken` 通知是否需要上下文切换
- **队列集合**: `queue_set_t` 支持从多个队列中多路接收

### 计数信号量 — 生产者-消费者同步

```
semaphore_t:
  count = 空闲槽位数量 (初始 max_count)
  max_count = 缓冲区容量

生产者: semaphore_take(empty_slots, timeout)  // count--
         queue_send(data_queue, &item)
         semaphore_give(filled_slots)          // count++

消费者: semaphore_take(filled_slots, timeout)  // count--
         queue_receive(data_queue, &item)
         semaphore_give(empty_slots)           // count++
```

信号量作为流量控制机制:
- `empty_slots` 初始值 = `BUFFER_SIZE`, 表示可写入的空位数
- `filled_slots` 初始值 = 0, 表示可读取的已填充数
- 快速生产者减缓写入, 慢速消费者被唤醒拉取

### 互斥锁 — 临界区保护

两个互斥锁保护不同粒度的共享数据:

| 互斥锁 | 保护对象 | 类型 |
|--------|---------|------|
| `pipe_mutex` | 队列入队/出队操作的原子性 | 普通互斥锁 |
| `stats_mutex` | 生产者/消费者统计数组 | 普通互斥锁 |

互斥锁支持优先级继承: 当高优先级任务阻塞在互斥锁上, 而低优先级任务持有该锁时, 低优先级任务的优先级被临时提升到高优先级, 避免优先级反转。

### 内存堆 — 动态分配

演示同时验证了内存堆子系统。`heap_stats_t` 结构提供运行时内存信息:

```c
heap_stats_t heap = heap_get_stats();
// heap.total_size      — 堆总大小
// heap.free_size       — 当前空闲大小
// heap.allocated_size  — 已分配大小
// heap.largest_free_block — 最大连续空闲块
// heap.alloc_count     — 累计分配次数
// heap.free_count      — 累计释放次数
// heap.min_free_ever   — 历史最低空闲量
```

支持 heap_1 ~ heap_5 五种算法: 最基本的 heap_1 不允许释放, heap_2 首次适配, heap_3 合并相邻空闲块, heap_4 首次适配+合并, heap_5 多区域堆。

## 演示任务拓扑

```
优先级 5  ──► 定时器守护任务 (timer_daemon_init)
              │
              ├── health_timer  (5s周期)  ─── timer_health_check()
              └── resource_timer(10s周期) ─── timer_resource_report()

优先级 4  ──► task_watchdog         (看门狗, 完成条件检测)
优先级 3  ──► task_producer [4个]   (生产者, 每批200项)
            ├─ task_fast_burst_producer (快速突发生产者)
优先级 2  ──► task_consumer [3个]   (消费者, 持续消费)
优先级 1  ──► task_statistics_reporter (统计报告, 2s周期)
            ├─ task_memory_monitor     (内存监控, 5s周期)
```

## 数据流

### 数据项结构

```c
typedef struct {
    uint32_t id;            // (producer_id << 24) | production_num
    uint32_t production_id; // 本生产者内的序号
    uint32_t timestamp;     // task_get_tick_count() 时间戳
    uint32_t checksum;      // 完整性校验和
    uint8_t  payload[16];   // 伪载荷数据 (基于 producer_id/prod_num)
} data_item_t;
```

校验和算法:

```c
checksum = id ^ production_id ^ timestamp
         + ∑[i=0..15] payload[i] * (i + 1)
```

消费者接收后重新计算校验和并与 `item.checksum` 比对以检测数据损坏。

### 生产者流程

```
produce_item() 构造数据项
    │
    ▼
semaphore_take(empty_slots, timeout=500)
    │ 失败: overflow_count++, task_delay(10), 重试
    ▼ 成功
mutex_lock(pipe_mutex, timeout=100)
    │
    ▼
queue_send(data_queue, &item, timeout=100)
    │ 失败: semaphore_give(empty_slots), overflow_count++
    ▼ 成功
semaphore_give(filled_slots)
更新 prod_stats[prod_id]
更新 total_produced, peak_queue_depth
    │
    ▼
mutex_unlock(pipe_mutex)
    │
    ▼
task_delay(5 + prod_id * 3)  // 变化的生产速率
```

### 消费者流程

```
semaphore_take(filled_slots, timeout=300)
    │ 失败: underflow_count++, task_delay(20), 重试
    ▼ 成功
mutex_lock(pipe_mutex, timeout=100)
    │
    ▼
queue_receive(data_queue, &item, timeout=100)
    │ 失败: semaphore_give(filled_slots), underflow_count++
    ▼ 成功
semaphore_give(empty_slots)
    │
    ▼
validate_item(&item)
    │ 通过: cons_stats[cons_id] 更新, total_consumed++
    │ 失败: checksum_errors++
    │
    ▼
mutex_unlock(pipe_mutex)
    │
    ▼
task_delay(8 + cons_id * 2)  // 变化的消费速率
```

## IPC API 参考

### 队列操作

| API | 说明 | 阻塞 | ISR安全 |
|-----|------|------|---------|
| `queue_create(item_size, max_items, name)` | 创建队列 | — | — |
| `queue_send(q, data, timeout)` | 发送数据 | 是 | 否 |
| `queue_receive(q, buf, timeout)` | 接收数据 | 是 | 否 |
| `queue_send_from_isr(q, data, &woken)` | 中断中发送 | 否 | 是 |
| `queue_receive_from_isr(q, buf, &woken)` | 中断中接收 | 否 | 是 |
| `queue_messages_waiting(q)` | 查询队列中消息数 | 否 | 是 |
| `queue_spaces_available(q)` | 查询剩余空间 | 否 | 是 |
| `queue_reset(q)` | 清空队列 | 否 | 否 |

`timeout` 参数:
- `0` — 不阻塞, 若失败立即返回 0
- `>0` — 阻塞等待指定 tick 数
- `0xFFFFFFFF` — 无限等待

### 信号量操作

| API | 说明 |
|-----|------|
| `semaphore_create_binary(name)` | 创建二值信号量 (0/1) |
| `semaphore_create_counting(max, init, name)` | 创建计数信号量 |
| `semaphore_take(sem, timeout)` | P 操作 (获取) |
| `semaphore_give(sem)` | V 操作 (释放) |
| `semaphore_give_from_isr(sem, &woken)` | 中断中释放 |
| `semaphore_get_count(sem)` | 查询当前计数值 |

### 互斥锁操作

| API | 说明 |
|-----|------|
| `mutex_create(name)` | 创建互斥锁 |
| `mutex_lock(m, timeout)` | 获取锁 (阻塞) |
| `mutex_unlock(m)` | 释放锁 |
| `mutex_lock_recursive(m, timeout)` | 递归获取 (同一任务可多次加锁) |
| `mutex_unlock_recursive(m)` | 递归释放 (释放到计数为0) |
| `mutex_get_owner(m)` | 查询锁持有者 TCB 指针 |
| `gatekeeper_call(m, fn, param, timeout)` | 门卫模式: 在持有锁时安全调用函数 |

### 内存管理操作

| API | 说明 |
|-----|------|
| `heap_init(start, size, heap_type)` | 初始化堆, 指定算法类型 1-5 |
| `malloc_rtos(size)` | 线程安全分配 |
| `free_rtos(ptr)` | 线程安全释放 |
| `calloc_rtos(num, size)` | 零初始化分配 |
| `realloc_rtos(ptr, new_size)` | 调整已分配块大小 |
| `heap_get_stats()` | 获取堆统计信息 |
| `heap_get_free_size()` | 快速获取剩余空闲量 |
| `heap_get_min_free()` | 历史最低空闲量 |
| `heap_malloc_failed_hook()` | 分配失败钩子 (弱符号, 可重写) |

### 队列集合多路接收

队列集合允许单任务监听多个队列, 任一队列有数据即可返回:

```c
queue_set_t *set = queue_set_create(3);       // 最多监听3个队列
queue_set_add(set, q1);
queue_set_add(set, q2);
queue_set_add(set, q3);

while (1) {
    queue_t *active = queue_set_select(set, portMAX_DELAY);
    if (active == q1) { /* 处理 q1 */ }
    if (active == q2) { /* 处理 q2 */ }
}
```

## 全局观测变量

| 变量 | 类型 | 含义 |
|------|------|------|
| `total_produced` | volatile uint32_t | 累积生产总数 |
| `total_consumed` | volatile uint32_t | 累积消费总数 |
| `overflow_count` | volatile uint32_t | 队列满时发送失败次数 |
| `underflow_count` | volatile uint32_t | 队列空时接收失败次数 |
| `checksum_errors` | volatile uint32_t | 数据校验失败次数 |
| `peak_queue_depth` | volatile uint32_t | 队列历史最大深度 |
| `alloc_failures` | volatile uint32_t | 内存分配失败次数 |
| `system_running` | volatile uint32_t | 系统运行标志 (0=停止) |

## 定时器回调

### 健康检查 (5s 周期)

```c
timer_health_check():
  if total_consumed < total_produced && both > 0:
    // 已生产但未消费的项目数 = total_produced - total_consumed
  if checksum_errors > 0:
    // 数据完整性告警
```

### 资源报告 (10s 周期)

```c
timer_resource_report():
  空槽位数 = semaphore_get_count(empty_slots)
  已填充数 = semaphore_get_count(filled_slots)
  堆统计  = heap_get_stats()
```

## 完成条件

看门狗任务 (`task_watchdog`) 每 10 秒检查 `total_produced >= ITEM_COUNT` (200), 满足后设置 `system_running = 0`, 所有生产/消费任务检测到该标志后退出循环。在没有实时串口输出的嵌入式环境中, 可通过调试器观察该变量确认演示完成。

## 构建与运行

```bash
make demos
```

生成文件: `build/demo_producer_consumer.elf`

```bash
# J-Link 加载示例
arm-none-eabi-gdb build/demo_producer_consumer.elf \
  -ex "target remote :2331" \
  -ex "monitor reset" \
  -ex "load" \
  -ex "break task_statistics_reporter" \
  -ex "continue"
```

在 `task_statistics_reporter` 设断点, 每个报告周期 (2s) 检查统计变量。

## 调试要点

1. **验证队列完整性**: 在 `queue_send` 和 `queue_receive` 设断点, 观察 `head/tail/count` 变化
2. **信号量同步**: 检查 `empty_slots->count` 是否始终在 `[0, BUFFER_SIZE]` 范围内
3. **数据校验**: 若 `checksum_errors` 非零, 则 `compute_checksum()` 与 `validate_item()` 之间存在不匹配
4. **溢出/下溢**: `overflow_count` 持续增长说明消费者太慢; `underflow_count` 持续增长说明生产者太慢
5. **内存泄漏检测**: 周期性检查 `heap_get_stats().alloc_count - heap_get_stats().free_count` 是否稳定在初始化后的基线值
6. **峰值队列深度**: `peak_queue_depth` 反映生产-消费速率最不匹配时刻的缓冲区压力

## 场景扩展建议

基于此演示可以扩展验证以下场景:

1. **中断驱动生产**: 将 UART/SPI 接收中断连接到 `queue_send_from_isr`, 验证 ISR 上下文队列操作
2. **队列集合多路**: 创建多个优先级队列, 使用 `queue_set_select` 实现带优先级的消息分发
3. **Gatekeeper 模式**: 将统计报告改为 `gatekeeper_call(stats_mutex, report_fn, NULL, 100)`, 验证门卫模式正确性
4. **递归互斥锁**: 在嵌套函数调用中使用 `mutex_lock_recursive/mutex_unlock_recursive`
5. **内存压力测试**: 减少堆大小, 观察 `heap_malloc_failed_hook` 钩子触发

## 相关文档

- [API 参考](../docs/API_REFERENCE.md) — 完整 IPC / 堆 API 说明
- [移植指南](../docs/PORTING_GUIDE.md) — 中断安全与临界区保护
- 头文件: `inc/ipc_queue.h`, `inc/semaphore_mutex.h`, `inc/memory_heap.h`, `inc/software_timer.h`, `inc/task_scheduler.h`
- 源文件: `src/ipc_queue.c`, `src/semaphore_mutex.c`, `src/memory_heap.c`, `src/software_timer.c`, `src/task_scheduler.c`
