/*
 * trace_diag.c -- Runtime Tracing & Diagnostics Implementation
 *
 * L6 Canonical Problem: Non-intrusive RTOS observability.
 * Lock-free ring buffer tracing and CPU utilization profiling.
 *
 * L8 Advanced Topic: Lock-free ring buffer with atomic write index.
 * Single-producer (tick ISR + tasks in critical section).
 *
 * L4 Theorem: RMA Schedulability Test (Liu & Layland 1973).
 * L5 Algorithm: Running average for CPU utilization.
 *
 * Course: CMU 15-410 Pintos instrumentation, Berkeley CS 162,
 *   MIT 6.828 xv6 tracing
 */
#include "task_scheduler.h"
#include "trace_diag.h"
#include <string.h>

/* ================================================================
 *  TRACE RING BUFFER (L6, L8)
 * ================================================================ */

static trace_event_t trace_buf[TRACE_BUF_SIZE];
static volatile uint32_t trace_write_idx = 0;
static volatile uint32_t trace_event_total = 0;
static volatile uint32_t trace_overflow_count = 0;

void trace_init(void)
{
    memset(trace_buf, 0, sizeof(trace_buf));
    trace_write_idx = 0;
    trace_event_total = 0;
    trace_overflow_count = 0;
}

/*
 * L8: Lock-free trace event writer.
 * Atomic write index updated in critical section.
 * If buffer wraps, increments overflow counter but never blocks.
 * Ensures tracing never affects real-time behavior. O(1).
 */
void trace_event(trace_event_type_t type, uint32_t task_id, uint32_t arg)
{
    uint32_t idx;
    task_enter_critical();
    idx = trace_write_idx;
    trace_buf[idx].tick = task_get_tick_count();
    trace_buf[idx].type = type;
    trace_buf[idx].task_id = task_id;
    trace_buf[idx].arg = arg;
    trace_buf[idx].text[0] = 0;
    trace_write_idx = (idx + 1) % TRACE_BUF_SIZE;
    trace_event_total++;
    if (trace_event_total > TRACE_BUF_SIZE * 2) {
        trace_overflow_count++;
    }
    task_exit_critical();
}

void trace_event_text(trace_event_type_t type, uint32_t task_id,
                       uint32_t arg, const char *text)
{
    uint32_t idx;
    task_enter_critical();
    idx = trace_write_idx;
    trace_buf[idx].tick = task_get_tick_count();
    trace_buf[idx].type = type;
    trace_buf[idx].task_id = task_id;
    trace_buf[idx].arg = arg;
    if (text) {
        strncpy(trace_buf[idx].text, text, TRACE_EVENT_MAX_TEXT - 1);
        trace_buf[idx].text[TRACE_EVENT_MAX_TEXT - 1] = 0;
    } else {
        trace_buf[idx].text[0] = 0;
    }
    trace_write_idx = (idx + 1) % TRACE_BUF_SIZE;
    trace_event_total++;
    if (trace_event_total > TRACE_BUF_SIZE * 2) {
        trace_overflow_count++;
    }
    task_exit_critical();
}

uint32_t trace_event_count(void)
{
    return trace_event_total;
}

/*
 * L5: Dump trace buffer to user array.
 * Copies most recent events in chronological order.
 * Counts from oldest event. O(min(n, max_events)).
 */
uint32_t trace_dump(trace_event_t *buf, uint32_t max_events)
{
    uint32_t start_idx, i, count, copied = 0;
    if (!buf) return 0;
    task_enter_critical();
    count = trace_event_total;
    if (count > TRACE_BUF_SIZE) count = TRACE_BUF_SIZE;
    if (max_events < count) count = max_events;
    /* Oldest event in buffer */
    if (trace_event_total <= TRACE_BUF_SIZE) {
        start_idx = 0;
    } else {
        start_idx = trace_write_idx;
    }
    for (i = 0; i < count && copied < max_events; i++) {
        uint32_t idx = (start_idx + i) % TRACE_BUF_SIZE;
        buf[copied] = trace_buf[idx];
        copied++;
    }
    task_exit_critical();
    return copied;
}

void trace_clear(void)
{
    task_enter_critical();
    memset(trace_buf, 0, sizeof(trace_buf));
    trace_write_idx = 0;
    trace_event_total = 0;
    trace_overflow_count = 0;
    task_exit_critical();
}

/* L1: Human-readable event type names */
const char *trace_event_name(trace_event_type_t type)
{
    static const char *names[] = {
        "TASK_CREATE","TASK_DELETE","TASK_SWITCH","TASK_BLOCK",
        "TASK_READY", "ISR_ENTER",  "ISR_EXIT",   "QUEUE_SEND",
        "QUEUE_RECV", "SEM_TAKE",   "SEM_GIVE",   "MUTEX_LOCK",
        "MUTEX_UNLOCK","TIMER_FIRE", "HEAP_ALLOC", "HEAP_FREE",
        "OVERFLOW",   "USER_EVENT"
    };
    if (type > TRACE_USER_EVENT) return "UNKNOWN";
    return names[type];
}

/* ================================================================
 *  CPU UTILIZATION STATISTICS (L3, L4)
 * ================================================================ */

static cpu_stats_t cpu_stats;
static volatile uint32_t cpu_idle_ticks_acc = 0;
static volatile uint32_t cpu_total_ticks_acc = 0;
static volatile uint32_t cpu_ctx_switch_count = 0;
static uint32_t cpu_last_dump_tick = 0;

void cpu_stats_init(void)
{
    memset(&cpu_stats, 0, sizeof(cpu_stats));
    cpu_idle_ticks_acc = 0;
    cpu_total_ticks_acc = 0;
    cpu_ctx_switch_count = 0;
    cpu_last_dump_tick = task_get_tick_count();
}

/*
 * L5: Tick-level CPU accounting. Called once per SysTick.
 * CPU utilization (%) = 100 * (total - idle) / total.
 *
 * L4 Theorem: For aperiodic servers, utilization bound:
 *   U <= 1 - U_s where U_s = server utilization.
 */
void cpu_stats_tick_hook(void)
{
    cpu_total_ticks_acc++;
    if (task_get_current()) {
        tcb_t *cur = task_get_current();
        if (cur->priority == TASK_PRIORITY_IDLE || cur->priority == 0) {
            cpu_idle_ticks_acc++;
        }
    }
}

void cpu_stats_task_switch_hook(uint32_t prev_id, uint32_t next_id)
{
    (void)prev_id; (void)next_id;
    cpu_ctx_switch_count++;
}

/*
 * L3: Aggregate CPU statistics snapshot.
 * Integer arithmetic: cpu_usage_percent = 100*(total-idle)/total.
 */
cpu_stats_t cpu_stats_get(void)
{
    cpu_stats_t snap;
    uint32_t delta_total, delta_idle;

    task_enter_critical();
    delta_total = cpu_total_ticks_acc;
    delta_idle = cpu_idle_ticks_acc;
    snap.context_switch_count = cpu_ctx_switch_count;
    task_exit_critical();

    snap.idle_ticks = delta_idle;
    snap.total_ticks = delta_total;

    if (delta_total > 0 && delta_total >= delta_idle) {
        snap.cpu_usage_percent = ((delta_total - delta_idle) * 100U) / delta_total;
    } else {
        snap.cpu_usage_percent = 0;
    }

    snap.task_count = 0;
    snap.max_runtime_task_id = 0;
    snap.max_runtime_ticks = 0;

    (void)cpu_last_dump_tick;
    return snap;
}

void cpu_stats_reset(void)
{
    task_enter_critical();
    cpu_idle_ticks_acc = 0;
    cpu_total_ticks_acc = 0;
    cpu_ctx_switch_count = 0;
    cpu_last_dump_tick = task_get_tick_count();
    task_exit_critical();
}

/* ================================================================
 *  PER-TASK PROFILING (L6)
 * ================================================================ */

void task_profile_init(task_profile_t *prof, const char *name)
{
    if (!prof) return;
    memset(prof, 0, sizeof(task_profile_t));
    if (name) {
        strncpy(prof->name, name, TASK_NAME_MAX_LEN - 1);
        prof->name[TASK_NAME_MAX_LEN - 1] = 0;
    }
}

void task_profile_tick_hook(task_profile_t *prof)
{
    if (!prof) return;
    prof->total_runtime_ticks++;
    prof->cur_continuous_ticks++;
    if (prof->cur_continuous_ticks > prof->max_continuous_ticks) {
        prof->max_continuous_ticks = prof->cur_continuous_ticks;
    }
}

void task_profile_switch_in(task_profile_t *prof)
{
    if (!prof) return;
    prof->switch_in_count++;
    prof->last_start_tick = task_get_tick_count();
    prof->cur_continuous_ticks = 0;
}

void task_profile_switch_out(task_profile_t *prof)
{
    (void)prof;
    /* cur_continuous_ticks tracked via tick_hook during execution */
}

uint32_t task_profile_avg_runtime(const task_profile_t *prof)
{
    if (!prof || prof->switch_in_count == 0) return 0;
    return prof->total_runtime_ticks / prof->switch_in_count;
}

/* ================================================================
 *  RMA SCHEDULABILITY TEST (L4: Liu & Layland 1973)
 * ================================================================ */

/*
 * L4 Theorem: Liu & Layland (1973) "Scheduling Algorithms for
 * Multiprogramming in a Hard-Real-Time Environment", JACM 20(1).
 *
 * For n independent, preemptable, periodic tasks with implicit
 * deadlines (D_i = T_i), Rate Monotonic priority assignment
 * (shorter period = higher priority) is optimal among all
 * fixed-priority schemes. The least upper bound on utilization:
 *
 *   U_lub(n) = n * (2^(1/n) - 1)
 *
 * As n -> infinity, U_lub -> ln(2) ~= 0.693147... (69.3%).
 *
 * SUFFICIENT condition: if U <= U_lub(n), the task set is
 * guaranteed schedulable under RM.
 *
 * Implementation: integer permil (1/1000) arithmetic.
 * Precomputed bound table for n = 1..32, asymptotic ln(2) for n > 32.
 */
int32_t rma_schedulability_test(const rma_task_t *tasks, uint32_t task_count,
                                 uint32_t *total_utilization_permil)
{
    uint32_t i;
    uint32_t util_sum = 0;
    uint32_t bound_permil;

    /* Precomputed: n * (2^(1/n) - 1) * 1000 for n=1..32 */
    static const uint32_t rma_bound_permil[] = {
        1000, 828, 779, 756, 743, 734, 728, 724, 720, 717,
        715,  713, 711, 710, 709, 708, 707, 706, 706, 705,
        704,  704, 703, 703, 702, 702, 702, 701, 701, 701,
        700,  700
    };

    if (!tasks || task_count == 0) {
        if (total_utilization_permil) *total_utilization_permil = 0;
        return 1; /* empty set is trivially schedulable */
    }

    /* Step 1: Total utilization U = sum(C_i / T_i) in permil */
    for (i = 0; i < task_count; i++) {
        if (tasks[i].period_ticks == 0) return 0;
        util_sum += (1000U * tasks[i].wcet_ticks) / tasks[i].period_ticks;
    }

    if (total_utilization_permil) *total_utilization_permil = util_sum;

    /* Step 2: Liu-Layland bound */
    if (task_count <= 32)
        bound_permil = rma_bound_permil[task_count - 1];
    else
        bound_permil = 693; /* ln(2) * 1000 */

    /* Step 3: Compare */
    return (util_sum <= bound_permil) ? 1 : 0;
}
