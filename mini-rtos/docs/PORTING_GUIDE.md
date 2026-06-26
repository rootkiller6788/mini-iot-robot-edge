# mini-rtos 移植指南

本文档说明如何将 mini-rtos 移植到目标硬件平台（ARM Cortex-M）。

## 1. 架构要求

| 项目 | 要求 |
|------|------|
| 处理器 | ARM Cortex-M3 / M4 / M7 / M33 |
| 编译器 | arm-none-eabi-gcc (或支持 C99 的交叉编译器) |
| 时基 | SysTick 定时器（24位递减） |
| 上下文切换 | PendSV 异常（优先级最低） |
| 临界区 | FAULTMASK 寄存器（或 BASEPRI） |

## 2. 移植步骤

### 2.1 创建 port.c

在项目中创建 `src/port.c` 文件，实现以下函数和 ISR：

```c
#include <stdint.h>

void port_start_first_task(void);
void port_save_context(void);
void port_restore_context(void);
void SysTick_Handler(void);
void PendSV_Handler(void);
void SVC_Handler(void);
```

### 2.2 实现 `port_start_first_task()`

```asm
.section .text
.global port_start_first_task
.type   port_start_first_task, %function

port_start_first_task:
    // 1. 从当前任务 TCB 获取栈指针
    ldr  r0, =current_task
    ldr  r0, [r0]
    ldr  sp, [r0]

    // 2. 弹出 R4-R11
    pop  {r4-r11}

    // 3. 弹出 R0-R3, R12, LR, PC, xPSR
    pop  {r0-r3}
    pop  {r12}
    add  sp, #4    // skip LR
    pop  {lr}
    add  sp, #4    // skip xPSR (loaded by exception return)

    // 4. 异常返回，进入任务
    bx   lr
```

### 2.3 实现 PendSV_Handler

```asm
.global PendSV_Handler
.type   PendSV_Handler, %function

PendSV_Handler:
    // 保存当前上下文
    mrs  r0, PSP
    // R4-R11 自动保存，R0-R3,R12,LR,PC,xPSR 需手动
    stmdb r0!, {r4-r11}
    // 保存 PSP 到 current_task->sp
    ldr  r1, =current_task
    ldr  r1, [r1]
    str  r0, [r1]

    // 调用 C 层 pendsv_handler 选择 next_task
    push {lr}
    bl   pendsv_handler
    pop  {lr}

    // 恢复新任务的上下文
    ldr  r0, =current_task
    ldr  r0, [r0]
    ldr  r0, [r0]    // R0 = new PSP

    ldmia r0!, {r4-r11}
    msr  PSP, r0

    // 异常返回
    bx   lr
```

### 2.4 实现 SysTick_Handler

```c
// 在 C 文件中：

void SysTick_Handler(void)
{
    scheduler_tick_handler();
}
```

### 2.5 初始化 SysTick

```c
// 在 main() 调用 scheduler_init() 之后：

void systick_init(uint32_t tick_rate_hz)
{
    uint32_t ticks = SystemCoreClock / tick_rate_hz;
    SysTick->LOAD  = ticks - 1;
    SysTick->VAL   = 0;
    SysTick->CTRL  = SysTick_CTRL_CLKSOURCE_Msk |
                     SysTick_CTRL_TICKINT_Msk   |
                     SysTick_CTRL_ENABLE_Msk;
}
```

## 3. 链接脚本要求

链接脚本需要定义堆栈区域，供 `heap_init()` 使用：

```ld
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 256K
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
}

_estack = ORIGIN(SRAM) + LENGTH(SRAM);

SECTIONS
{
    .text : { *(.text*) } > FLASH
    .data : { *(.data*) } > SRAM AT > FLASH
    .bss  : { *(.bss*) }  > SRAM

    // 堆起始于 BSS 末端
    _heap_start = .;
    _heap_end   = ORIGIN(SRAM) + LENGTH(SRAM) - 4K; // 保留 4K 给系统栈
}
```

## 4. 中断优先级

```c
// 在 main() 中设置：

NVIC_SetPriority(PendSV_IRQn,    0xFF);  // 最低优先级
NVIC_SetPriority(SysTick_IRQn,   0x00);  // 可按需调整
```

关键规则：
- PendSV 必须是 **最低优先级**，确保上下文切换在上层 ISR 完成后执行
- SysTick 优先级应 **低于** 需要被抢占的高优先级 ISR

## 5. 编译器选项

```
CFLAGS = -std=c99 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
         -ffunction-sections -fdata-sections -O2
         -Wall -Wextra -Werror
```

对于 FPU 支持（Cortex-M4F/M7），添加：

```
-mfloat-abi=hard -mfpu=fpv4-sp-d16
```

并在上下文切换时保存/恢复 FPU 寄存器（S0-S31, FPSCR）。

## 6. 验证移植

### 6.1 最小测试

```c
volatile int test_counter = 0;

void test_task(void *param) {
    while (1) {
        test_counter++;
        task_delay(100);
    }
}

int main(void) {
    hardware_init();
    systick_init(1000);
    scheduler_init();
    heap_init(heap_start, heap_size, HEAP_TYPE_4);
    task_create(test_task, "test", 256, NULL, 1);
    scheduler_start();
}
```

### 6.2 检查点

- [ ] SysTick 中断触发 `scheduler_tick_handler()`
- [ ] `test_counter` 持续递增
- [ ] PendSV 不发生 HardFault
- [ ] idle 任务正确运行
- [ ] 多任务创建和切换正常
- [ ] 任务栈水印完整（无溢出）

## 7. 常见问题

### 7.1 HardFault 在启动时
- 检查链接脚本 `_estack` 是否正确
- 检查 `port_start_first_task` 栈弹出顺序

### 7.2 任务不切换
- 确认 PendSV 优先级为最低 (0xFF)
- 确认 SysTick 已使能并产生中断

### 7.3 栈溢出
- 增大 `task_create()` 的 stack_size 参数
- 启用 `stack_overflow_check()` 定期检查
- 检查 `heap_init()` 区域是否与任务栈重叠

### 7.4 内存耗尽
- 增大 `heap_init()` 分配的堆大小
- 使用 `heap_get_min_free()` 监控历史最小空闲
- 检查是否有内存泄漏

## 8. 平台相关宏

以下宏在 `task_scheduler.h` / `memory_heap.h` 中定义，可按需覆盖：

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `TASK_PRIORITY_MAX` | 32 | 最大优先级数量 |
| `TASK_STACK_MIN` | 64 | 最小任务栈字节数 |
| `HEAP_ALIGNMENT` | 8 | 堆分配对齐（8字节） |
| `HEAP_MIN_BLOCK_SIZE` | 32 | 最小空闲块大小 |
| `HEAP_MAX_REGIONS` | 8 | heap_5 最大区域数 |
