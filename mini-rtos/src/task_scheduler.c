#include "task_scheduler.h"
#include "memory_heap.h"
#include "portable.h"
#include <string.h>

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
        PORT_WFI();
    }
}

static void task_stack_init(tcb_t *task)
{
    uintptr_t sp_top = (uintptr_t)((uint8_t *)task->stack_start + task->stack_size);
    uint32_t *sp = (uint32_t *)sp_top;
    uint32_t i;
    for (i = 0; i < task->stack_size / 4; i++) {
        task->stack_start[i] = TASK_STACK_WATERMARK;
    }
    sp -= 16;
    sp[14] = (uint32_t)(uintptr_t)task->entry;
    sp[15] = 0x01000000U;
    sp[13] = (uint32_t)((uintptr_t)sp + 64);
    sp[12] = (uint32_t)(uintptr_t)task->param;
    sp[8]  = 0xFFFFFFFDU;
    sp[9]  = (uint32_t)(uintptr_t)task_delete;
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
    memset(task, 0, sizeof(tcb_t));
    task->stack_start = (uint32_t *)malloc_rtos(stack_size);
    if (!task->stack_start) {
        free_rtos(task);
        return NULL;
    }
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
            if (current_task && task->priority > current_task->priority) {
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

/*
 * L5: Rate-Monotonic priority assignment (Liu & Layland 1973).
 *
 * Given an array of task periods, assigns priorities such that
 * shorter period = higher priority. This is the optimal fixed-priority
 * assignment for independent periodic tasks with implicit deadlines.
 *
 * The schedulability test is performed separately via RMA test.
 *
 * L4 Theorem: Among all fixed-priority schemes, RM is optimal in the
 * sense that if any fixed-priority scheme can schedule a task set,
 * RM can also schedule it (Lehoczky et al., 1989).
 */
void scheduler_assign_rm_priorities(tcb_t **tasks, uint32_t *periods,
                                     uint32_t count)
{
    uint32_t i, j;
    if (!tasks || !periods || count < 2) return;
    /* Simple insertion sort by period (ascending -> higher priority) */
    for (i = 0; i < count; i++) {
        for (j = i + 1; j < count; j++) {
            if (periods[i] > periods[j]) {
                uint32_t tmp_p = periods[i];
                tcb_t *tmp_t = tasks[i];
                periods[i] = periods[j];
                tasks[i] = tasks[j];
                periods[j] = tmp_p;
                tasks[j] = tmp_t;
            }
        }
    }
    /* Assign priorities: TASK_PRIORITY_HIGHEST for shortest period */
    for (i = 0; i < count && i < TASK_PRIORITY_HIGHEST; i++) {
        tasks[i]->priority = TASK_PRIORITY_HIGHEST - i;
        tasks[i]->base_priority = tasks[i]->priority;
    }
}

/*
 * L3: System load factor computation.
 *
 * Load = total_runtime / total_ticks (ratio of non-idle time).
 * Used for capacity planning and overload detection.
 *
 * Returns load in permil (0-1000). 1000 = 100% CPU, 0 = fully idle.
 */
uint32_t scheduler_get_load_permil(void)
{
    uint32_t total = task_get_tick_count();
    uint32_t i, busy_ticks = 0;
    if (total == 0) return 0;
    for (i = 0; i < TASK_PRIORITY_MAX; i++) {
        tcb_t *head = task_list[i];
        if (!head) continue;
        tcb_t *task = head;
        do {
            busy_ticks += task->runtime_ticks;
            task = task->next;
        } while (task != head);
    }
    /* Clamp to 1000 permil */
    if (busy_ticks >= total) return 1000;
    return (busy_ticks * 1000U) / total;
}

/*
 * L6: EDF (Earliest Deadline First) ready-queue insertion.
 *
 * L5 Algorithm: EDF is a dynamic priority scheduling algorithm where
 * the task with the earliest absolute deadline has the highest priority.
 * Unlike RM, EDF can achieve 100% CPU utilization (U <= 1.0).
 *
 * L4 Theorem (Dertouzos, 1974): EDF is optimal among all preemptive
 * scheduling algorithms on uniprocessors — if any algorithm can
 * schedule a task set, EDF can too.
 *
 * This function inserts a task into a deadline-ordered list.
 */
void scheduler_edf_list_insert(tcb_t **edf_list, tcb_t *task,
                                uint32_t deadline)
{
    tcb_t *prev = NULL, *curr;
    if (!edf_list || !task) return;
    task->ticks_remaining = deadline; /* reuse field for EDF deadline */
    task->next = NULL;
    curr = *edf_list;
    /* Insert sorted by absolute deadline (earliest first) */
    while (curr && curr->ticks_remaining <= deadline) {
        prev = curr;
        curr = curr->next;
    }
    if (!prev) {
        task->next = *edf_list;
        *edf_list = task;
    } else {
        task->next = prev->next;
        prev->next = task;
    }
}

/*
 * L3: Current task runtime in ticks.
 * Returns accumulated runtime of the currently executing task.
 */
uint32_t task_get_runtime_ticks(void)
{
    tcb_t *cur = task_get_current();
    return cur ? cur->runtime_ticks : 0;
}

/*
 * L3: Port function stubs for host (non-ARM) compilation.
 * On ARM targets, these are provided by port.c with actual assembly.
 */
#ifndef __arm__

void port_start_first_task(void)
{
    /* Host stub: just set tick and loop the scheduler */
    tick_count = 1;
}

void port_save_context(void)
{
    /* Host stub: no actual context to save */
}

void port_restore_context(void)
{
    /* Host stub: no actual context to restore */
}

#endif /* !__arm__ */
