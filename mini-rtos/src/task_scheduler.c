#include "task_scheduler.h"
#include "memory_heap.h"
#include <string.h>

#define PORT_PENDSV_TRIGGER()  (*(volatile uint32_t *)0xE000ED04 = (1U << 28))
#define PORT_FAULT_MASK()      __asm volatile("cpsid f" ::: "memory")
#define PORT_UNFAULT_MASK()    __asm volatile("cpsie f" ::: "memory")

static tcb_t  *task_list[TASK_PRIORITY_MAX] = {NULL};
static tcb_t  *current_task = NULL;
static tcb_t  *next_task = NULL;
static tcb_t   idle_task_tcb;
static uint32_t idle_stack[TASK_STACK_MIN / 4];
static volatile uint32_t tick_count = 0;
static volatile uint32_t scheduler_running = 0;
static volatile uint32_t context_switch_pending = 0;
static uint32_t critical_nesting = 0;
static uint32_t next_task_id = 1;

static tcb_t sentinel_tcb;

void port_start_first_task(void);
void port_save_context(void);
void port_restore_context(void);

static void idle_entry(void *param)
{
    (void)param;
    while (1) {
        __asm volatile("wfi");
    }
}

static void task_stack_init(tcb_t *task)
{
    uint32_t *sp = (uint32_t *)((uint32_t)((uint8_t *)task->stack_start + task->stack_size));
    uint32_t i;
    for (i = 0; i < task->stack_size / 4; i++) {
        task->stack_start[i] = TASK_STACK_WATERMARK;
    }
    sp -= 16;
    sp[14] = (uint32_t)task->entry;
    sp[15] = 0x01000000U;
    sp[13] = (uint32_t)sp + 64;
    sp[12] = (uint32_t)task->param;
    sp[8]  = 0xFFFFFFFDU;
    sp[9]  = (uint32_t)task_delete;
    task->sp = sp;
}

void scheduler_init(void)
{
    uint32_t i;
    for (i = 0; i < TASK_PRIORITY_MAX; i++) {
        task_list[i] = NULL;
    }
    tick_count = 0;
    scheduler_running = 0;
    context_switch_pending = 0;
    critical_nesting = 0;
    sentinel_tcb.priority = TASK_PRIORITY_MAX;
    sentinel_tcb.next = &sentinel_tcb;
    sentinel_tcb.prev = &sentinel_tcb;
    task_create(idle_entry, "idle", sizeof(idle_stack), NULL, TASK_PRIORITY_IDLE);
}

tcb_t *task_create(task_entry_t entry, const char *name, uint32_t stack_size,
                   void *param, uint32_t priority)
{
    tcb_t *task;
    if (priority >= TASK_PRIORITY_MAX) priority = TASK_PRIORITY_LOWEST;
    if (stack_size < TASK_STACK_MIN) stack_size = TASK_STACK_MIN;
    stack_size = (stack_size + 7) & ~7U;
    task = (tcb_t *)malloc_rtos(sizeof(tcb_t));
    if (!task) return NULL;
    task->stack_start = (uint32_t *)malloc_rtos(stack_size);
    if (!task->stack_start) {
        free_rtos(task);
        return NULL;
    }
    memset(task, 0, sizeof(tcb_t));
    task->stack_size = stack_size;
    task->priority = priority;
    task->base_priority = priority;
    task->state = TASK_READY;
    task->entry = entry;
    task->param = param;
    task->time_slice = 10;
    task->runtime_ticks = 0;
    if (name) {
        strncpy(task->name, name, TASK_NAME_MAX_LEN - 1);
        task->name[TASK_NAME_MAX_LEN - 1] = '\0';
    }
    task_stack_init(task);
    stack_overflow_init(task);
    task_enter_critical();
    {
        if (!task_list[priority]) {
            task_list[priority] = task;
            task->next = task;
            task->prev = task;
        } else {
            tcb_t *head = task_list[priority];
            task->next = head;
            task->prev = head->prev;
            head->prev->next = task;
            head->prev = task;
        }
    }
    task_exit_critical();
    return task;
}

void task_delete(tcb_t *task)
{
    if (!task || task == &idle_task_tcb) return;
    task_enter_critical();
    {
        if (task->next == task) {
            task_list[task->priority] = NULL;
        } else {
            if (task_list[task->priority] == task) {
                task_list[task->priority] = task->next;
            }
            task->prev->next = task->next;
            task->next->prev = task->prev;
        }
        if (current_task == task) {
            current_task = NULL;
            context_switch_pending = 1;
        }
    }
    task_exit_critical();
    free_rtos(task->stack_start);
    free_rtos(task);
    if (context_switch_pending) {
        PORT_PENDSV_TRIGGER();
    }
}

void task_suspend(tcb_t *task)
{
    if (!task) return;
    task_enter_critical();
    {
        if (task->state != TASK_SUSPENDED) {
            task->state = TASK_SUSPENDED;
            if (task_list[task->priority] == task) {
                if (task->next == task) {
                    task_list[task->priority] = NULL;
                } else {
                    task_list[task->priority] = task->next;
                    task->prev->next = task->next;
                    task->next->prev = task->prev;
                }
            } else {
                task->prev->next = task->next;
                task->next->prev = task->prev;
            }
            if (current_task == task) {
                context_switch_pending = 1;
            }
        }
    }
    task_exit_critical();
}

void task_resume(tcb_t *task)
{
    if (!task) return;
    task_enter_critical();
    {
        if (task->state == TASK_SUSPENDED) {
            task->state = TASK_READY;
            if (!task_list[task->priority]) {
                task_list[task->priority] = task;
                task->next = task;
                task->prev = task;
            } else {
                tcb_t *head = task_list[task->priority];
                task->next = head;
                task->prev = head->prev;
                head->prev->next = task;
                head->prev = task;
            }
            if (task->priority > current_task->priority) {
                context_switch_pending = 1;
            }
        }
    }
    task_exit_critical();
    if (context_switch_pending) {
        PORT_PENDSV_TRIGGER();
    }
}

void task_delay(uint32_t ticks)
{
    if (ticks == 0) return;
    task_enter_critical();
    {
        current_task->ticks_remaining = ticks;
        current_task->state = TASK_BLOCKED;
        if (task_list[current_task->priority] == current_task) {
            if (current_task->next == current_task) {
                task_list[current_task->priority] = NULL;
            } else {
                task_list[current_task->priority] = current_task->next;
                current_task->prev->next = current_task->next;
                current_task->next->prev = current_task->prev;
            }
        }
        context_switch_pending = 1;
    }
    task_exit_critical();
    PORT_PENDSV_TRIGGER();
}

void task_delay_until(uint32_t *last_wake, uint32_t ticks)
{
    uint32_t now = tick_count;
    uint32_t next = *last_wake + ticks;
    if (now < next) {
        task_delay(next - now);
    }
    *last_wake = tick_count;
}

void task_yield(void)
{
    task_enter_critical();
    context_switch_pending = 1;
    task_exit_critical();
    PORT_PENDSV_TRIGGER();
}

uint32_t task_get_tick_count(void)
{
    return tick_count;
}

tcb_t *task_get_current(void)
{
    return current_task;
}

void task_enter_critical(void)
{
    PORT_FAULT_MASK();
    critical_nesting++;
}

void task_exit_critical(void)
{
    critical_nesting--;
    if (critical_nesting == 0) {
        PORT_UNFAULT_MASK();
    }
}

static tcb_t *find_next_task(void)
{
    uint32_t prio;
    for (prio = TASK_PRIORITY_HIGHEST; prio > TASK_PRIORITY_IDLE; prio--) {
        if (task_list[prio]) {
            tcb_t *head = task_list[prio];
            tcb_t *candidate = head;
            do {
                if (candidate->state == TASK_READY) {
                    if (candidate->runtime_ticks < candidate->time_slice ||
                        head == candidate) {
                        task_list[prio] = candidate->next;
                        return candidate;
                    }
                }
                candidate = candidate->next;
            } while (candidate != head);
            task_list[prio] = head;
            return head;
        }
    }
    return task_list[TASK_PRIORITY_IDLE];
}

void scheduler_start(void)
{
    current_task = find_next_task();
    if (!current_task) {
        current_task = task_list[TASK_PRIORITY_IDLE];
    }
    current_task->state = TASK_RUNNING;
    scheduler_running = 1;
    port_start_first_task();
}

void scheduler_tick_handler(void)
{
    if (!scheduler_running) return;
    tick_count++;
    if (current_task) {
        current_task->runtime_ticks++;
    }
    {
        uint32_t prio;
        for (prio = 0; prio < TASK_PRIORITY_MAX; prio++) {
            tcb_t *head = task_list[prio];
            if (!head) continue;
            tcb_t *task = head;
            do {
                if (task->state == TASK_BLOCKED && task->ticks_remaining > 0) {
                    task->ticks_remaining--;
                    if (task->ticks_remaining == 0) {
                        task->state = TASK_READY;
                        task->block_obj = NULL;
                    }
                }
                task = task->next;
            } while (task != head);
        }
    }
    {
        uint32_t prio;
        for (prio = TASK_PRIORITY_HIGHEST; prio > TASK_PRIORITY_IDLE; prio--) {
            tcb_t *head = task_list[prio];
            if (!head) continue;
            tcb_t *task = head;
            do {
                if (task->state == TASK_READY && task != current_task &&
                    task->priority > current_task->priority) {
                    context_switch_pending = 1;
                    break;
                }
                task = task->next;
            } while (task != head);
            if (context_switch_pending) break;
        }
    }
    if (context_switch_pending) {
        PORT_PENDSV_TRIGGER();
    }
}

void pendsv_handler(void)
{
    port_save_context();
    next_task = find_next_task();
    if (next_task) {
        next_task->runtime_ticks = 0;
        next_task->state = TASK_RUNNING;
        current_task = next_task;
    }
    port_restore_context();
}

uint32_t task_stack_free(tcb_t *task)
{
    uint32_t *p = task->stack_start;
    uint32_t free_bytes = 0;
    while (free_bytes < task->stack_size && *p == TASK_STACK_WATERMARK) {
        free_bytes += 4;
        p++;
    }
    return free_bytes;
}

void task_stack_watermark_check(tcb_t *task)
{
    if (task_stack_free(task) < 64) {
        stack_overflow_hook(task);
    }
}

void task_list_debug(void)
{
    uint32_t prio;
    for (prio = 0; prio < TASK_PRIORITY_MAX; prio++) {
        tcb_t *head = task_list[prio];
        if (!head) continue;
        tcb_t *task = head;
        do {
            (void)task->name;
            task = task->next;
        } while (task != head);
    }
}
