/*
 * trace_diag.h -- Runtime Tracing & Diagnostics for mini-rtos
 *
 * L6 Canonical Problem: Non-intrusive RTOS observability.
 * Lock-free ring-buffer trace log, CPU utilization profiling,
 * and task runtime statistics for system health monitoring.
 *
 * L8 Advanced Topic: Lock-free ring buffer for trace events.
 * Single-producer (ISR/task), single-consumer (dump task).
 *
 * L4 Standard: Amdahl's Law applied to RTOS profiling.
 * Speedup = 1/((1-P)+P/N); CPU utilization < 69% for RMA (n tasks).
 *
 * L4 Theorem: Liu & Layland (1973) RMA schedulability test:
 *   U = sum(C_i/T_i) <= n * (2^(1/n) - 1)
 *
 * Course Mapping: CMU 15-410 Kernel instrumentation, Berkeley CS 162
 *   Performance monitoring, MIT 6.828 xv6 tracing
 */
#ifndef TRACE_DIAG_H
#define TRACE_DIAG_H

#include <stdint.h>
#include "task_scheduler.h"

#define TRACE_BUF_SIZE        256
#define TRACE_EVENT_MAX_TEXT  32

typedef enum {
    TRACE_TASK_CREATE   = 0,
    TRACE_TASK_DELETE   = 1,
    TRACE_TASK_SWITCH   = 2,
    TRACE_TASK_BLOCK    = 3,
    TRACE_TASK_READY    = 4,
    TRACE_ISR_ENTER     = 5,
    TRACE_ISR_EXIT      = 6,
    TRACE_QUEUE_SEND    = 7,
    TRACE_QUEUE_RECV    = 8,
    TRACE_SEM_TAKE      = 9,
    TRACE_SEM_GIVE      = 10,
    TRACE_MUTEX_LOCK    = 11,
    TRACE_MUTEX_UNLOCK  = 12,
    TRACE_TIMER_FIRE    = 13,
    TRACE_HEAP_ALLOC    = 14,
    TRACE_HEAP_FREE     = 15,
    TRACE_OVERFLOW      = 16,
    TRACE_USER_EVENT    = 17
} trace_event_type_t;

typedef struct {
    uint32_t           tick;
    trace_event_type_t type;
    uint32_t           task_id;
    uint32_t           arg;
    char               text[TRACE_EVENT_MAX_TEXT];
} trace_event_t;

typedef struct {
    uint32_t idle_ticks;
    uint32_t total_ticks;
    uint32_t cpu_usage_percent;
    uint32_t task_count;
    uint32_t max_runtime_task_id;
    uint32_t max_runtime_ticks;
    uint32_t context_switch_count;
} cpu_stats_t;

typedef struct {
    uint32_t task_id;
    uint32_t total_runtime_ticks;
    uint32_t last_start_tick;
    uint32_t switch_in_count;
    uint32_t max_continuous_ticks;
    uint32_t cur_continuous_ticks;
    char     name[TASK_NAME_MAX_LEN];
} task_profile_t;

void        trace_init(void);
void        trace_event(trace_event_type_t type, uint32_t task_id, uint32_t arg);
void        trace_event_text(trace_event_type_t type, uint32_t task_id,
                              uint32_t arg, const char *text);
uint32_t    trace_event_count(void);
uint32_t    trace_dump(trace_event_t *buf, uint32_t max_events);
void        trace_clear(void);
const char *trace_event_name(trace_event_type_t type);

void        cpu_stats_init(void);
void        cpu_stats_tick_hook(void);
void        cpu_stats_task_switch_hook(uint32_t prev_id, uint32_t next_id);
cpu_stats_t cpu_stats_get(void);
void        cpu_stats_reset(void);

void        task_profile_init(task_profile_t *prof, const char *name);
void        task_profile_tick_hook(task_profile_t *prof);
void        task_profile_switch_in(task_profile_t *prof);
void        task_profile_switch_out(task_profile_t *prof);
uint32_t    task_profile_avg_runtime(const task_profile_t *prof);

/*
 * L4: RMA Schedulability Test (Liu & Layland 1973).
 * For n periodic tasks with implicit deadlines:
 *   U = sum(C_i / T_i) <= n * (2^(1/n) - 1)
 * Returns 1 if schedulable, 0 otherwise.
 */
typedef struct {
    uint32_t period_ticks;
    uint32_t wcet_ticks;
} rma_task_t;

int32_t rma_schedulability_test(const rma_task_t *tasks, uint32_t task_count,
                                 uint32_t *total_utilization_permil);

#endif /* TRACE_DIAG_H */
