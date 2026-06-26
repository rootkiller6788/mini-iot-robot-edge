# mini-rtos API 参考手册

## 1. 任务调度 (task_scheduler.h)

### 全局常量

| 宏 | 值 | 说明 |
|----|-----|------|
| `TASK_NAME_MAX_LEN` | 16 | 任务名最大长度 |
| `TASK_PRIORITY_MAX` | 32 | 最大优先级数 |
| `TASK_PRIORITY_IDLE` | 0 | 空闲任务优先级 |
| `TASK_STACK_MIN` | 64 | 任务最小栈 (bytes) |
| `TASK_STACK_WATERMARK` | 0xA5A5A5A5 | 栈水印填充值 |

### 类型

```c
typedef enum { TASK_READY, TASK_RUNNING, TASK_BLOCKED, TASK_SUSPENDED } task_state_t;
typedef void (*task_entry_t)(void *param);
typedef struct tcb { ... } tcb_t;
```

### API

| 函数 | 说明 |
|------|------|
| `void scheduler_init(void)` | 初始化调度器，创建 idle 任务 |
| `void scheduler_start(void)` | 启动调度器（永不返回） |
| `tcb_t *task_create(entry, name, stack, param, prio)` | 创建任务，返回 TCB 指针 |
| `void task_delete(tcb_t *task)` | 删除任务，释放资源 |
| `void task_suspend(tcb_t *task)` | 挂起任务 |
| `void task_resume(tcb_t *task)` | 恢复挂起的任务 |
| `void task_delay(uint32_t ticks)` | 阻塞当前任务指定 tick 数 |
| `void task_delay_until(uint32_t *last, uint32_t period)` | 精确周期延迟 |
| `uint32_t task_get_tick_count(void)` | 获取系统 tick 计数 |
| `tcb_t *task_get_current(void)` | 获取当前运行任务 TCB |
| `void task_yield(void)` | 主动让出 CPU |
| `void task_enter_critical(void)` | 进入临界区 |
| `void task_exit_critical(void)` | 退出临界区 |
| `uint32_t task_stack_free(tcb_t *task)` | 查询任务剩余栈空间 |
| `void task_stack_watermark_check(tcb_t *task)` | 检查栈水印是否完整 |

### 中断服务

- `scheduler_tick_handler()` — SysTick ISR 中调用
- `pendsv_handler()` — PendSV ISR 中调用

---

## 2. IPC 队列 (ipc_queue.h)

### API

| 函数 | 说明 |
|------|------|
| `queue_t *queue_create(item_size, max_items, name)` | 创建队列 |
| `void queue_delete(queue_t *q)` | 删除队列并释放阻塞任务 |
| `int32_t queue_send(q, data, timeout)` | 发送消息，超时返回 |
| `int32_t queue_receive(q, buf, timeout)` | 接收消息，超时返回 |
| `int32_t queue_send_from_isr(q, data, &woken)` | ISR 中发送（非阻塞） |
| `int32_t queue_receive_from_isr(q, buf, &woken)` | ISR 中接收（非阻塞） |
| `uint32_t queue_messages_waiting(q)` | 队列中消息数 |
| `uint32_t queue_spaces_available(q)` | 队列可用空间数 |
| `void queue_reset(queue_t *q)` | 清空队列 |
| `void queue_registry_add(q)` | 注册到队列全局表 |
| `void queue_registry_remove(q)` | 从全局表移除 |
| `uint32_t queue_registry_count(void)` | 全局注册队列数 |

### 队列集合

| 函数 | 说明 |
|------|------|
| `queue_set_t *queue_set_create(max_queues)` | 创建队列集合 |
| `void queue_set_add(set, q)` | 添加队列到集合 |
| `void queue_set_remove(set, q)` | 从集合移除队列 |
| `queue_t *queue_set_select(set, timeout)` | 等待集合中任一队列有数据 |

### 返回值

- `1` — 成功
- `0` — 队列满/空（非阻塞模式）
- `-1` — 参数错误
- `-2` — 超时

---

## 3. 信号量与互斥锁 (semaphore_mutex.h)

### 信号量 API

| 函数 | 说明 |
|------|------|
| `semaphore_create_binary(name)` | 创建二值信号量 (初值 1) |
| `semaphore_create_counting(max, initial, name)` | 创建计数信号量 |
| `void semaphore_delete(sem)` | 删除信号量 |
| `int32_t semaphore_take(sem, timeout)` | 获取信号量（P 操作） |
| `int32_t semaphore_give(sem)` | 释放信号量（V 操作） |
| `int32_t semaphore_give_from_isr(sem, &woken)` | ISR 中释放 |
| `uint32_t semaphore_get_count(sem)` | 查询当前计数 |

### 互斥锁 API

| 函数 | 说明 |
|------|------|
| `mutex_create(name)` | 创建互斥锁 |
| `void mutex_delete(m)` | 删除互斥锁 |
| `int32_t mutex_lock(m, timeout)` | 加锁（支持优先级继承） |
| `int32_t mutex_unlock(m)` | 解锁（归还继承优先级） |
| `int32_t mutex_lock_recursive(m, timeout)` | 递归加锁 |
| `int32_t mutex_unlock_recursive(m)` | 递归解锁 |
| `tcb_t *mutex_get_owner(m)` | 查询锁持有者 |

### Gatekeeper 模式

| 函数 | 说明 |
|------|------|
| `int32_t gatekeeper_call(mutex, fn, param, timeout)` | 在互斥锁保护下执行函数 |

---

## 4. 软件定时器 (software_timer.h)

### 类型

```c
typedef enum { TIMER_TYPE_ONE_SHOT, TIMER_TYPE_AUTO_RELOAD } timer_type_t;
typedef void (*timer_callback_t)(void *param);
```

### API

| 函数 | 说明 |
|------|------|
| `void timer_daemon_init(stack, priority)` | 初始化定时器守护任务 |
| `sw_timer_t *timer_create(name, cb, param, period, type)` | 创建定时器 |
| `int32_t timer_start(t, delay)` | 启动定时器 |
| `int32_t timer_stop(t)` | 停止定时器 |
| `int32_t timer_reset(t, delay)` | 重置定时器 |
| `int32_t timer_change_period(t, new_period)` | 修改周期 |
| `void timer_delete(t)` | 删除定时器 |
| `int32_t timer_start_from_isr(t, delay, &woken)` | ISR 中启动 |
| `int32_t timer_stop_from_isr(t, &woken)` | ISR 中停止 |
| `int32_t timer_reset_from_isr(t, delay, &woken)` | ISR 中重置 |
| `uint32_t timer_is_active(t)` | 查询定时器是否活动 |
| `uint32_t timer_get_id(t)` | 获取定时器 ID |
| `uint32_t timer_get_remaining(t)` | 剩余 tick 数 |

---

## 5. 内存管理 (memory_heap.h)

### 堆算法

| 宏 | 算法 |
|----|------|
| `HEAP_TYPE_1` | 简单分配（无释放） |
| `HEAP_TYPE_2` | 最佳适配 (Best Fit) |
| `HEAP_TYPE_3` | 最佳适配 + 空闲块合并 |
| `HEAP_TYPE_4` | 首次适配 + 合并（默认） |
| `HEAP_TYPE_5` | 多区域 Heap_4 |

### API

| 函数 | 说明 |
|------|------|
| `void heap_init(start, size, type)` | 初始化堆 |
| `void heap_add_region(start, size)` | 添加堆区域（heap_5） |
| `void *malloc_rtos(size)` | 分配内存 |
| `void free_rtos(ptr)` | 释放内存 |
| `void *calloc_rtos(num, size)` | 清零分配 |
| `void *realloc_rtos(ptr, size)` | 重新分配 |

### 统计与诊断

| 函数 | 说明 |
|------|------|
| `heap_stats_t heap_get_stats(void)` | 获取堆统计信息 |
| `void heap_dump(void)` | 转储堆状态 |
| `uint32_t heap_get_free_size(void)` | 空闲总大小 |
| `uint32_t heap_get_min_free(void)` | 历史最小空闲 |
| `void heap_malloc_failed_hook(void)` | 分配失败钩子（弱符号） |

### 栈溢出检测

| 函数 | 说明 |
|------|------|
| `void stack_overflow_init(tcb_t *task)` | 填充水印 |
| `uint32_t stack_overflow_check(tcb_t *task)` | 检查水印完整性 |
| `void stack_overflow_hook(tcb_t *task)` | 溢出钩子（弱符号） |

### MPU 宏

```c
#define MPU_PROTECT_STACK(task)   /* 保护任务栈 */
#define MPU_UNPROTECT_STACK(task) /* 解除栈保护 */
```

需定义 `MPU_ENABLED` 和 `CORTEX_M` 编译宏启用。

---

## 数据结构

### heap_stats_t

```c
typedef struct {
    uint32_t total_size;
    uint32_t free_size;
    uint32_t allocated_size;
    uint32_t largest_free_block;
    uint32_t alloc_count;
    uint32_t free_count;
    uint32_t min_free_ever;
} heap_stats_t;
```

---

## 中断安全

所有以 `_from_isr` 结尾的函数均可在 ISR 中调用。它们是非阻塞的，
若操作无法立即完成则返回 `0`。如果 API 导致更高优先级任务就绪，
则将 `*woken` 置为 `1`，ISR 退出后由调度器决定是否切换。

```c
int32_t woken = 0;
semaphore_give_from_isr(my_sem, &woken);
/* ISR 退出: portEND_SWITCHING_ISR(woken); */
```
