/*
 * bench_core.c - Core Benchmarks for mini-rtos
 *
 * Mini-RTOS has NO header files. All types and prototypes are declared here.
 * Covers all 5 modules: task_scheduler, ipc_queue, semaphore_mutex,
 *   software_timer, memory_heap.
 *
 * Usage: bench_core [N]
 *   N = iteration scale factor (default 5000)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

/* ================================================================
 *  TYPE & CONSTANT DECLARATIONS  (no headers exist for mini-rtos)
 * ================================================================ */

/* ---- task_scheduler types ---- */

#define TASK_NAME_MAX_LEN   16
#define TASK_STACK_MIN      128
#define TASK_STACK_WATERMARK 0xA5A5A5A5U
#define TASK_PRIORITY_MAX   8

enum task_state {
    TASK_READY      = 0,
    TASK_BLOCKED    = 1,
    TASK_SUSPENDED  = 2,
    TASK_RUNNING    = 3
};

enum task_priority {
    TASK_PRIORITY_HIGHEST = 7,
    TASK_PRIORITY_LOWEST  = 1,
    TASK_PRIORITY_IDLE    = 0
};

typedef struct tcb {
    char        name[16];
    uint32_t   *stack_start;
    uint32_t    stack_size;
    uint32_t   *sp;
    void      (*entry)(void *param);
    void       *param;
    uint32_t    priority;
    uint32_t    base_priority;
    uint32_t    state;
    uint32_t    time_slice;
    uint32_t    runtime_ticks;
    uint32_t    ticks_remaining;
    void       *block_obj;
    struct tcb *block_next;
    struct tcb *next;
    struct tcb *prev;
} tcb_t;

typedef void (*task_entry_t)(void *param);

/* ---- ipc_queue types ---- */

typedef struct queue {
    uint8_t    *buffer;
    uint32_t    item_size;
    uint32_t    max_items;
    uint32_t    head;
    uint32_t    tail;
    uint32_t    count;
    tcb_t      *blocked_senders;
    tcb_t      *blocked_receivers;
    uint8_t     isr_safe;
    char        name[16];
    struct queue *next;
} queue_t;

typedef struct queue_set {
    queue_t  **queues;
    uint32_t   max_queues;
    uint32_t   count;
    void      *observer;
} queue_set_t;

/* ---- semaphore_mutex types ---- */

typedef struct semaphore {
    uint32_t count;
    uint32_t max_count;
    tcb_t   *blocked_tasks;
    char     name[16];
} semaphore_t;

typedef struct mutex {
    uint32_t held;
    tcb_t   *owner;
    uint32_t owner_original_priority;
    uint32_t recursive_count;
    tcb_t   *blocked_tasks;
    char     name[16];
} mutex_t;

typedef void (*gatekeeper_task_t)(void *param);

/* ---- software_timer types ---- */

typedef void (*timer_callback_t)(void *param);

enum timer_type {
    TIMER_TYPE_ONE_SHOT    = 0,
    TIMER_TYPE_AUTO_RELOAD = 1
};

enum timer_cmd_type {
    TIMER_CMD_START         = 0,
    TIMER_CMD_STOP          = 1,
    TIMER_CMD_RESET         = 2,
    TIMER_CMD_CHANGE_PERIOD = 3,
    TIMER_CMD_DELETE        = 4
};

typedef struct sw_timer {
    uint32_t      id;
    char          name[16];
    uint32_t      type;
    timer_callback_t callback;
    void         *param;
    uint32_t      period_ticks;
    uint32_t      expiry_tick;
    uint32_t      active;
    struct sw_timer *next;
} sw_timer_t;

typedef struct timer_cmd {
    uint32_t      type;
    sw_timer_t   *timer;
    uint32_t      period;
    struct timer_cmd *next;
} timer_cmd_t;

/* ---- memory_heap types ---- */

#define BLOCK_HEADER_SIZE   16
#define BLOCK_MAGIC_FREE    0xFEED0001U
#define BLOCK_MAGIC_ALLOC   0xCAFE0001U
#define HEAP_ALIGNMENT      8
#define HEAP_MIN_BLOCK_SIZE 32
#define HEAP_MAX_REGIONS    4

enum heap_alloc_type {
    HEAP_TYPE_1 = 1,
    HEAP_TYPE_2 = 2,
    HEAP_TYPE_3 = 3,
    HEAP_TYPE_4 = 4,
    HEAP_TYPE_5 = 5
};

typedef struct block_header {
    uint32_t             size;
    uint32_t             free;
    uint32_t             magic;
    struct block_header *next;
    struct block_header *prev;
} block_header_t;

typedef struct heap_region {
    uint8_t        *start;
    uint8_t        *end;
    block_header_t *free_list;
} heap_region_t;

typedef struct heap_stats {
    uint32_t total_size;
    uint32_t free_size;
    uint32_t allocated_size;
    uint32_t alloc_count;
    uint32_t free_count;
    uint32_t min_free_ever;
} heap_stats_t;

/* ---- function prototypes ---- */

/* task_scheduler */
void      scheduler_init(void);
void      scheduler_start(void);
void      scheduler_tick_handler(void);
tcb_t    *task_create(task_entry_t entry, const char *name, uint32_t stack_size,
                      void *param, uint32_t priority);
void      task_delete(tcb_t *task);
void      task_suspend(tcb_t *task);
void      task_resume(tcb_t *task);
void      task_delay(uint32_t ticks);
void      task_delay_until(uint32_t *last_wake, uint32_t ticks);
void      task_yield(void);
uint32_t  task_get_tick_count(void);
tcb_t    *task_get_current(void);
void      task_enter_critical(void);
void      task_exit_critical(void);
uint32_t  task_stack_free(tcb_t *task);
void      task_stack_watermark_check(tcb_t *task);
void      task_list_debug(void);

/* ipc_queue */
queue_t     *queue_create(uint32_t item_size, uint32_t max_items, const char *name);
void         queue_delete(queue_t *q);
void         queue_reset(queue_t *q);
int32_t      queue_send(queue_t *q, const void *data, uint32_t timeout_ticks);
int32_t      queue_receive(queue_t *q, void *buffer, uint32_t timeout_ticks);
int32_t      queue_send_from_isr(queue_t *q, const void *data, int32_t *woken);
int32_t      queue_receive_from_isr(queue_t *q, void *buffer, int32_t *woken);
uint32_t     queue_messages_waiting(const queue_t *q);
uint32_t     queue_spaces_available(const queue_t *q);
void         queue_registry_add(queue_t *q);
void         queue_registry_remove(queue_t *q);
uint32_t     queue_registry_count(void);
queue_set_t *queue_set_create(uint32_t max_queues);
void         queue_set_add(queue_set_t *s, queue_t *q);
void         queue_set_remove(queue_set_t *s, queue_t *q);
queue_t     *queue_set_select(queue_set_t *s, uint32_t timeout_ticks);

/* semaphore_mutex */
semaphore_t *semaphore_create_binary(const char *name);
semaphore_t *semaphore_create_counting(uint32_t max_count, uint32_t initial_count,
                                       const char *name);
void         semaphore_delete(semaphore_t *sem);
int32_t      semaphore_take(semaphore_t *sem, uint32_t timeout_ticks);
int32_t      semaphore_give(semaphore_t *sem);
int32_t      semaphore_give_from_isr(semaphore_t *sem, int32_t *woken);
uint32_t     semaphore_get_count(const semaphore_t *sem);
mutex_t     *mutex_create(const char *name);
void         mutex_delete(mutex_t *m);
int32_t      mutex_lock(mutex_t *m, uint32_t timeout_ticks);
int32_t      mutex_unlock(mutex_t *m);
int32_t      mutex_lock_recursive(mutex_t *m, uint32_t timeout_ticks);
int32_t      mutex_unlock_recursive(mutex_t *m);
tcb_t       *mutex_get_owner(const mutex_t *m);
int32_t      gatekeeper_call(mutex_t *gatekeeper, gatekeeper_task_t fn,
                             void *param, uint32_t timeout_ticks);

/* software_timer */
void         timer_daemon_init(uint32_t stack_size, uint32_t priority);
sw_timer_t  *timer_create(const char *name, timer_callback_t cb, void *param,
                          uint32_t period_ticks, uint32_t type);
void         timer_delete(sw_timer_t *t);
int32_t      timer_start(sw_timer_t *t, uint32_t delay_ticks);
int32_t      timer_stop(sw_timer_t *t);
int32_t      timer_reset(sw_timer_t *t, uint32_t delay_ticks);
int32_t      timer_change_period(sw_timer_t *t, uint32_t new_period_ticks);
int32_t      timer_start_from_isr(sw_timer_t *t, uint32_t delay_ticks, int32_t *woken);
int32_t      timer_stop_from_isr(sw_timer_t *t, int32_t *woken);
int32_t      timer_reset_from_isr(sw_timer_t *t, uint32_t delay_ticks, int32_t *woken);
uint32_t     timer_is_active(const sw_timer_t *t);
uint32_t     timer_get_id(const sw_timer_t *t);
uint32_t     timer_get_remaining(const sw_timer_t *t);

/* memory_heap */
void         heap_init(void *start, uint32_t size, uint8_t type);
void         heap_add_region(void *start, uint32_t size);
void        *malloc_rtos(size_t size);
void         free_rtos(void *ptr);
void        *calloc_rtos(size_t num, size_t size);
void        *realloc_rtos(void *ptr, size_t new_size);
heap_stats_t heap_get_stats(void);
void         heap_dump(void);
uint32_t     heap_get_free_size(void);
uint32_t     heap_get_min_free(void);
void         stack_overflow_init(tcb_t *task);
uint32_t     stack_overflow_check(tcb_t *task);
uint32_t     heap_calc_largest_free(void);

/* ================================================================
 *  HELPER
 * ================================================================ */

static double now_ms(void) {
    return (double)clock() * 1000.0 / (double)CLOCKS_PER_SEC;
}

static void bench_run(const char *name, void (*fn)(int), int n) {
    double t0 = now_ms();
    fn(n);
    double t1 = now_ms();
    double elapsed = t1 - t0;
    printf("  %-40s  %d ops in %9.1f ms  (%8.1f us/op)\n",
           name, n, elapsed, (elapsed * 1000.0) / (double)n);
}

/* dummy task entry */
static void dummy_entry(void *param) {
    (void)param;
}

static void timer_dummy_cb(void *param) {
    (void)param;
}

static void gatekeeper_dummy_fn(void *param) {
    (void)param;
}

/* ================================================================
 *  BENCHMARKS - task_scheduler
 * ================================================================ */

static uint8_t bench_heap_buf[1024 * 1024]; /* 1 MB for bench heap */

static void bm_task_create_delete(int n) {
    heap_init(bench_heap_buf, sizeof(bench_heap_buf), HEAP_TYPE_4);
    scheduler_init();
    for (int i = 0; i < n; i++) {
        tcb_t *t = task_create(dummy_entry, "bench", TASK_STACK_MIN, NULL, 3);
        if (t) task_delete(t);
    }
}

static void bm_task_suspend_resume(int n) {
    heap_init(bench_heap_buf, sizeof(bench_heap_buf), HEAP_TYPE_4);
    scheduler_init();
    tcb_t *t = task_create(dummy_entry, "bench", TASK_STACK_MIN, NULL, 3);
    for (int i = 0; i < n; i++) {
        task_suspend(t);
        task_resume(t);
    }
    task_delete(t);
}

static void bm_yield(int n) {
    heap_init(bench_heap_buf, sizeof(bench_heap_buf), HEAP_TYPE_4);
    scheduler_init();
    for (int i = 0; i < n; i++) {
        task_yield();
    }
}

static void bm_tick_get(int n) {
    volatile uint32_t tc;
    for (int i = 0; i < n; i++) {
        tc = task_get_tick_count();
        (void)tc;
    }
}

/* ================================================================
 *  BENCHMARKS - ipc_queue
 * ================================================================ */

static void bm_queue_create_delete(int n) {
    heap_init(bench_heap_buf, sizeof(bench_heap_buf), HEAP_TYPE_4);
    for (int i = 0; i < n; i++) {
        queue_t *q = queue_create(4, 64, "bench_q");
        if (q) queue_delete(q);
    }
}

static void bm_queue_send_receive(int n) {
    heap_init(bench_heap_buf, sizeof(bench_heap_buf), HEAP_TYPE_4);
    queue_t *q = queue_create(sizeof(int32_t), 256, "bench_q");
    int32_t val = 42, out;
    for (int i = 0; i < n; i++) {
        queue_send(q, &val, 0);
        queue_receive(q, &out, 0);
    }
    queue_delete(q);
}

static void bm_queue_set_select(int n) {
    heap_init(bench_heap_buf, sizeof(bench_heap_buf), HEAP_TYPE_4);
    queue_set_t *s = queue_set_create(4);
    queue_t *q = queue_create(sizeof(int32_t), 64, "bench_q");
    queue_set_add(s, q);
    int32_t val = 1;
    queue_send(q, &val, 0);
    for (int i = 0; i < n; i++) {
        queue_set_select(s, 0);
    }
    queue_delete(q);
    free_rtos(s->queues);
    free_rtos(s);
}

/* ================================================================
 *  BENCHMARKS - semaphore_mutex
 * ================================================================ */

static void bm_semaphore_give_take(int n) {
    heap_init(bench_heap_buf, sizeof(bench_heap_buf), HEAP_TYPE_4);
    semaphore_t *sem = semaphore_create_binary("bench_sem");
    for (int i = 0; i < n; i++) {
        semaphore_take(sem, 0);
        semaphore_give(sem);
    }
    semaphore_delete(sem);
}

static void bm_mutex_lock_unlock(int n) {
    heap_init(bench_heap_buf, sizeof(bench_heap_buf), HEAP_TYPE_4);
    mutex_t *m = mutex_create("bench_mtx");
    for (int i = 0; i < n; i++) {
        mutex_lock(m, 0);
        mutex_unlock(m);
    }
    mutex_delete(m);
}

/* ================================================================
 *  BENCHMARKS - software_timer
 * ================================================================ */

static void bm_timer_create_delete(int n) {
    heap_init(bench_heap_buf, sizeof(bench_heap_buf), HEAP_TYPE_4);
    timer_daemon_init(512, 2);
    for (int i = 0; i < n; i++) {
        sw_timer_t *t = timer_create("bench_tmr", timer_dummy_cb, NULL, 100, TIMER_TYPE_ONE_SHOT);
        if (t) timer_delete(t);
    }
}

static void bm_timer_start_stop(int n) {
    heap_init(bench_heap_buf, sizeof(bench_heap_buf), HEAP_TYPE_4);
    timer_daemon_init(512, 2);
    sw_timer_t *t = timer_create("bench_tmr", timer_dummy_cb, NULL, 100, TIMER_TYPE_ONE_SHOT);
    for (int i = 0; i < n; i++) {
        timer_start(t, 50);
        timer_stop(t);
    }
    timer_delete(t);
}

/* ================================================================
 *  BENCHMARKS - memory_heap
 * ================================================================ */

static void bm_malloc_free(int n) {
    heap_init(bench_heap_buf, sizeof(bench_heap_buf), HEAP_TYPE_4);
    for (int i = 0; i < n; i++) {
        void *p = malloc_rtos(64);
        if (p) free_rtos(p);
    }
}

static void bm_realloc(int n) {
    heap_init(bench_heap_buf, sizeof(bench_heap_buf), HEAP_TYPE_4);
    for (int i = 0; i < n; i++) {
        void *p = malloc_rtos(32);
        p = realloc_rtos(p, 128);
        if (p) free_rtos(p);
    }
}

static void bm_heap_stats(int n) {
    heap_init(bench_heap_buf, sizeof(bench_heap_buf), HEAP_TYPE_4);
    heap_stats_t s;
    for (int i = 0; i < n; i++) {
        s = heap_get_stats();
        (void)s.free_size;
    }
}

/* ================================================================
 *  MAIN
 * ================================================================ */

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 5000;
    if (N <= 0) N = 5000;

    printf("\n=== mini-rtos Benchmarks (N=%d) ===\n\n", N);

    int n_fast  = N / 10;  /* cheap ops */
    int n_cycle = N / 20;  /* create/delete cycles */
    int n_med   = N / 5;

    bench_run("tick_get (critical-free read)",       bm_tick_get,              n_fast * 10);
    bench_run("yield (context-switch hint)",          bm_yield,                 n_fast * 10);
    bench_run("task_create/delete (roundtrip)",       bm_task_create_delete,    n_cycle);
    bench_run("task_suspend/resume (roundtrip)",      bm_task_suspend_resume,   n_fast * 5);
    bench_run("queue_create/delete (roundtrip)",      bm_queue_create_delete,   n_cycle);
    bench_run("queue_send/receive (roundtrip)",       bm_queue_send_receive,    n_fast * 5);
    bench_run("queue_set_select (poll)",              bm_queue_set_select,      n_fast * 5);
    bench_run("semaphore_give/take (roundtrip)",      bm_semaphore_give_take,   n_fast * 5);
    bench_run("mutex_lock/unlock (roundtrip)",        bm_mutex_lock_unlock,     n_fast * 5);
    bench_run("timer_create/delete (roundtrip)",      bm_timer_create_delete,   n_cycle);
    bench_run("timer_start/stop (roundtrip)",         bm_timer_start_stop,      n_med);
    bench_run("malloc_rtos/free_rtos 64B (roundtrip)", bm_malloc_free,          n_med);
    bench_run("realloc_rtos 32B->128B (roundtrip)",   bm_realloc,               n_med);
    bench_run("heap_get_stats (read-only)",           bm_heap_stats,            n_fast * 10);

    printf("\nDone.\n");
    return 0;
}
