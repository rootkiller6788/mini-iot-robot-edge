/*
 * event_groups.c -- Event Groups Implementation
 *
 * L2 Core Concept: Event-driven task synchronization.
 * Each event bit = lightweight binary semaphore; combined with AND/OR logic.
 *
 * L3 Engineering Structure:
 *   Atomic bit-set/clear in critical section.
 *   Condition evaluation at every bit-set.
 *   Timeout-safe blocked-list removal.
 *
 * L5 Algorithm: Bit-mask condition evaluation O(n) where n=blocked tasks.
 *
 * L6 Canonical Problem: Multi-producer/multi-consumer event coordination.
 *
 * L7 Application: Sensor fusion (wait ALL sensors), peripheral IRQ (wait ANY).
 *
 * Course: CMU 15-410, Berkeley CS 162, MIT 6.004
 */
#include "task_scheduler.h"
#include "event_groups.h"
#include "memory_heap.h"
#include <string.h>

typedef struct {
    event_bits_t      wait_bits;
    event_wait_type_t wait_type;
    uint32_t          clear_on_exit;
} event_wait_info_t;

/*
 * L5: Evaluate if current event bits satisfy a task's wait condition.
 * AND_ALL: (bits & request) == request
 * OR_ANY:  (bits & request) != 0
 */
static int event_condition_met(event_bits_t current, event_bits_t request,
                                event_wait_type_t type)
{
    if (type == EVENT_WAIT_AND_ALL)
        return ((current & request) == request) ? 1 : 0;
    return ((current & request) != 0) ? 1 : 0;
}

/*
 * L5: Unblock first task with satisfied condition. Returns task or NULL.
 */
static struct tcb *unblock_first_ready(event_group_t *eg)
{
    struct tcb **prev_ptr = &eg->blocked_tasks;
    struct tcb *task = eg->blocked_tasks;

    while (task) {
        event_wait_info_t *info = (event_wait_info_t *)task->block_obj;
        if (info && event_condition_met(eg->bits, info->wait_bits,
                                         info->wait_type)) {
            *prev_ptr = task->block_next;
            task->block_next = NULL;
            task->state = TASK_READY;
            task->block_obj = NULL;
            return task;
        }
        prev_ptr = &task->block_next;
        task = task->block_next;
    }
    return NULL;
}

/* Unblock all ready tasks (called after bit-set) */
static void unblock_all_ready(event_group_t *eg)
{
    while (unblock_first_ready(eg)) {}
}

event_group_t *event_group_create(const char *name)
{
    event_group_t *eg = (event_group_t *)malloc_rtos(sizeof(event_group_t));
    if (!eg) return NULL;
    memset(eg, 0, sizeof(event_group_t));
    if (name) { strncpy(eg->name, name, 15); eg->name[15] = 0; }
    return eg;
}

void event_group_delete(event_group_t *eg)
{
    if (!eg) return;
    task_enter_critical();
    {
        struct tcb *task = eg->blocked_tasks;
        while (task) {
            struct tcb *next = task->block_next;
            task->state = TASK_READY;
            task->block_obj = NULL;
            task->block_next = NULL;
            task = next;
        }
        eg->blocked_tasks = NULL;
    }
    task_exit_critical();
    free_rtos(eg);
}

/*
 * L5: Atomic bit-set with unblock evaluation.
 * Complexity: O(n) blocked tasks. Critical-section protected.
 * Returns previous bits value.
 */
event_bits_t event_group_set_bits(event_group_t *eg, event_bits_t bits_to_set)
{
    event_bits_t prev;
    if (!eg) return 0;
    task_enter_critical();
    {
        prev = eg->bits;
        eg->bits |= bits_to_set;
        if (eg->blocked_tasks) unblock_all_ready(eg);
    }
    task_exit_critical();
    return prev;
}

/* L3: ISR-safe variant */
event_bits_t event_group_set_bits_from_isr(event_group_t *eg,
                                            event_bits_t bits_to_set,
                                            int32_t *woken)
{
    event_bits_t prev;
    if (!eg) return 0;
    prev = eg->bits;
    eg->bits |= bits_to_set;
    if (eg->blocked_tasks) { unblock_all_ready(eg); if (woken) *woken = 1; }
    return prev;
}

event_bits_t event_group_clear_bits(event_group_t *eg, event_bits_t bits_to_clear)
{
    event_bits_t prev;
    if (!eg) return 0;
    task_enter_critical();
    { prev = eg->bits; eg->bits &= ~bits_to_clear; }
    task_exit_critical();
    return prev;
}

/*
 * L5: Block-until-bits with timeout.
 * Stores wait metadata via task->block_obj (stack-allocated, no heap).
 * Returns event bits on success, 0 on timeout.
 */
event_bits_t event_group_wait_bits(event_group_t *eg,
                                    event_bits_t bits_to_wait,
                                    event_wait_type_t wait_type,
                                    uint32_t timeout_ticks,
                                    uint32_t clear_on_exit)
{
    event_wait_info_t info;
    event_bits_t result = 0;
    uint32_t start;

    if (!eg || bits_to_wait == 0) return 0;

    info.wait_bits = bits_to_wait;
    info.wait_type = wait_type;
    info.clear_on_exit = clear_on_exit;
    start = task_get_tick_count();

    task_enter_critical();
    if (event_condition_met(eg->bits, bits_to_wait, wait_type)) {
        result = eg->bits;
        if (clear_on_exit) {
            if (wait_type == EVENT_WAIT_OR_ANY_CLEAR)
                eg->bits &= ~(eg->bits & bits_to_wait);
            else
                eg->bits &= ~bits_to_wait;
        }
        task_exit_critical();
        return result;
    }
    /* Block */
    {
        struct tcb *self = task_get_current();
        self->state = TASK_BLOCKED;
        self->block_obj = &info;
        self->block_next = eg->blocked_tasks;
        eg->blocked_tasks = self;
    }
    task_exit_critical();

    while (1) {
        task_delay(1);
        if (timeout_ticks < (uint32_t)-1) {
            uint32_t elapsed = task_get_tick_count() - start;
            if (elapsed >= timeout_ticks) {
                task_enter_critical();
                {
                    struct tcb **list = &eg->blocked_tasks;
                    struct tcb *prev = NULL;
                    struct tcb *t = *list;
                    while (t) {
                        if (t == task_get_current()) {
                            if (prev) prev->block_next = t->block_next;
                            else *list = t->block_next;
                            t->block_next = NULL; t->block_obj = NULL;
                            break;
                        }
                        prev = t; t = t->block_next;
                    }
                }
                task_exit_critical();
                return 0;
            }
        }
        if (task_get_current()->state == TASK_READY) {
            task_enter_critical(); result = eg->bits; task_exit_critical();
            return result;
        }
    }
}

event_bits_t event_group_get_bits(const event_group_t *eg)
{
    return eg ? eg->bits : 0;
}

/*
 * L7 Application: Rendezvous synchronization (multi-task barrier).
 * Atomically sets bits, then waits for all required bits.
 * Equivalent to a countdown-latch on event groups.
 */
event_bits_t event_group_sync(event_group_t *eg,
                               event_bits_t bits_to_set,
                               event_bits_t bits_to_wait,
                               uint32_t timeout_ticks)
{
    if (!eg) return 0;
    event_group_set_bits(eg, bits_to_set);
    return event_group_wait_bits(eg, bits_to_wait, EVENT_WAIT_AND_ALL,
                                  timeout_ticks, 1);
}
