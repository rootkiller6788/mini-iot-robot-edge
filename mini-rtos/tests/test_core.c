/*
 * test_core.c - Core Unit Tests for mini-rtos
 * Uses actual mini-rtos headers for type safety.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "task_scheduler.h"
#include "ipc_queue.h"
#include "semaphore_mutex.h"
#include "software_timer.h"
#include "memory_heap.h"
#include "event_groups.h"
#include "trace_diag.h"
#include "rwlock.h"

static int tests_run = 0, tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  TEST %s ... ", name); fflush(stdout); } while(0)
#define PASS()     do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg)  do { printf("FAIL: %s\n", msg); return 1; } while(0)
#define CHECK(cond, msg) if (!(cond)) FAIL(msg)

static uint8_t test_heap_buf[64 * 1024];
static void dummy_entry(void *param) { (void)param; }
static void timer_dummy_cb(void *param) { (void)param; }

static void reset_system(void) {
    heap_init(test_heap_buf, sizeof(test_heap_buf), HEAP_TYPE_4);
    scheduler_init();
}

/* task_scheduler tests */
static int test_scheduler_init(void) {
    TEST("scheduler_init");
    heap_init(test_heap_buf, sizeof(test_heap_buf), HEAP_TYPE_4);
    scheduler_init();
    PASS(); return 0;
}
static int test_task_create(void) {
    TEST("task_create");
    reset_system();
    tcb_t *t = task_create(dummy_entry, "test_task", 512, NULL, 3);
    CHECK(t != NULL, "task_create NULL");
    CHECK(strcmp(t->name, "test_task") == 0, "name mismatch");
    CHECK(t->priority == 3, "priority mismatch");
    CHECK(t->state == TASK_READY, "not READY");
    task_delete(t);
    PASS(); return 0;
}
static int test_task_suspend_resume(void) {
    TEST("task_suspend/resume");
    reset_system();
    tcb_t *t = task_create(dummy_entry, "susp", 512, NULL, 3);
    task_suspend(t);
    CHECK(t->state == TASK_SUSPENDED, "not suspended");
    task_resume(t);
    CHECK(t->state == TASK_READY, "not resumed");
    task_delete(t);
    PASS(); return 0;
}
static int test_task_yield(void) {
    TEST("task_yield");
    reset_system();
    task_yield();
    PASS(); return 0;
}
static int test_task_get_tick_count(void) {
    TEST("task_get_tick_count");
    reset_system();
    CHECK(task_get_tick_count() == 0, "tick != 0");
    PASS(); return 0;
}

/* ipc_queue tests */
static int test_queue_create_delete(void) {
    TEST("queue_create/delete");
    heap_init(test_heap_buf, sizeof(test_heap_buf), HEAP_TYPE_4);
    queue_t *q = queue_create(sizeof(int32_t), 64, "test_q");
    CHECK(q != NULL, "create NULL");
    CHECK(q->max_items == 64, "max mismatch");
    queue_delete(q);
    PASS(); return 0;
}
static int test_queue_send_receive(void) {
    TEST("queue_send/receive");
    heap_init(test_heap_buf, sizeof(test_heap_buf), HEAP_TYPE_4);
    queue_t *q = queue_create(sizeof(int32_t), 64, "test_q");
    int32_t sv = 42, rv = 0;
    CHECK(queue_send(q, &sv, 0) == 1, "send fail");
    CHECK(queue_messages_waiting(q) == 1, "count != 1");
    CHECK(queue_receive(q, &rv, 0) == 1, "recv fail");
    CHECK(rv == 42, "value mismatch");
    queue_delete(q);
    PASS(); return 0;
}
static int test_queue_reset(void) {
    TEST("queue_reset");
    heap_init(test_heap_buf, sizeof(test_heap_buf), HEAP_TYPE_4);
    queue_t *q = queue_create(sizeof(int32_t), 64, "test_q");
    int32_t v = 99;
    queue_send(q, &v, 0);
    queue_reset(q);
    CHECK(queue_messages_waiting(q) == 0, "not empty after reset");
    queue_delete(q);
    PASS(); return 0;
}
static int test_queue_set(void) {
    TEST("queue_set");
    heap_init(test_heap_buf, sizeof(test_heap_buf), HEAP_TYPE_4);
    queue_set_t *s = queue_set_create(4);
    CHECK(s != NULL && s->max_queues == 4, "set create fail");
    queue_t *q = queue_create(sizeof(int32_t), 64, "sq");
    queue_set_add(s, q);
    CHECK(s->count == 1, "set add fail");
    queue_set_remove(s, q);
    CHECK(s->count == 0, "set remove fail");
    queue_delete(q);
    free_rtos(s->queues); free_rtos(s);
    PASS(); return 0;
}

/* semaphore_mutex tests */
static int test_semaphore_counting(void) {
    TEST("semaphore counting");
    heap_init(test_heap_buf, sizeof(test_heap_buf), HEAP_TYPE_4);
    semaphore_t *s = semaphore_create_counting(5, 3, "ts");
    CHECK(s != NULL && semaphore_get_count(s) == 3, "init fail");
    CHECK(semaphore_take(s, 0) == 1, "take fail");
    CHECK(semaphore_get_count(s) == 2, "count after take");
    CHECK(semaphore_give(s) == 1, "give fail");
    CHECK(semaphore_get_count(s) == 3, "count after give");
    semaphore_delete(s);
    PASS(); return 0;
}
static int test_semaphore_binary(void) {
    TEST("semaphore binary");
    heap_init(test_heap_buf, sizeof(test_heap_buf), HEAP_TYPE_4);
    semaphore_t *s = semaphore_create_binary("bs");
    CHECK(s != NULL && semaphore_get_count(s) == 1, "binary init fail");
    semaphore_take(s, 0);
    CHECK(semaphore_get_count(s) == 0, "not taken");
    semaphore_give(s);
    CHECK(semaphore_get_count(s) == 1, "not given back");
    semaphore_delete(s);
    PASS(); return 0;
}
static int test_mutex_lock_unlock(void) {
    TEST("mutex lock/unlock");
    reset_system();
    task_create(dummy_entry, "mowner", 512, NULL, 3);
    mutex_t *m = mutex_create("mtx");
    CHECK(m != NULL, "create fail");
    CHECK(mutex_lock(m, 0) == 1, "lock fail");
    CHECK(m->held == 1, "not held");
    CHECK(mutex_unlock(m) == 1, "unlock fail");
    mutex_delete(m);
    PASS(); return 0;
}
static int test_mutex_recursive(void) {
    TEST("mutex recursive");
    reset_system();
    task_create(dummy_entry, "rmtx", 512, NULL, 3);
    mutex_t *m = mutex_create("rt");
    CHECK(mutex_lock_recursive(m, 0) == 1, "first lock fail");
    CHECK(mutex_lock_recursive(m, 0) == 1, "second lock fail");
    CHECK(mutex_unlock_recursive(m) == 1, "unlock1 fail");
    CHECK(mutex_unlock_recursive(m) == 1, "unlock2 fail");
    mutex_delete(m);
    PASS(); return 0;
}

/* software_timer tests */
static int test_timer_create_start_stop(void) {
    TEST("timer create/start/stop");
    reset_system();
    timer_daemon_init(512, 2);
    sw_timer_t *t = timer_create("tmr", timer_dummy_cb, NULL, 100, TIMER_TYPE_ONE_SHOT);
    CHECK(t != NULL && timer_get_id(t) > 0, "create fail");
    CHECK(timer_start(t, 50) == 1, "start fail");
    CHECK(timer_stop(t) == 1, "stop fail");
    timer_delete(t);
    PASS(); return 0;
}
static int test_timer_change_period(void) {
    TEST("timer change_period");
    reset_system();
    timer_daemon_init(512, 2);
    sw_timer_t *t = timer_create("ptmr", timer_dummy_cb, NULL, 100, TIMER_TYPE_AUTO_RELOAD);
    CHECK(timer_change_period(t, 200) == 1, "change fail");
    timer_delete(t);
    PASS(); return 0;
}

/* memory_heap tests */
static int test_heap_malloc_free(void) {
    TEST("malloc/free");
    heap_init(test_heap_buf, sizeof(test_heap_buf), HEAP_TYPE_4);
    heap_stats_t s = heap_get_stats();
    CHECK(s.total_size > 0, "total_size zero");
    void *p = malloc_rtos(128);
    CHECK(p != NULL, "malloc NULL");
    CHECK(heap_get_free_size() < s.free_size, "free not reduced");
    free_rtos(p);
    CHECK(heap_get_free_size() == s.free_size, "free not restored");
    PASS(); return 0;
}
static int test_heap_calloc(void) {
    TEST("calloc");
    heap_init(test_heap_buf, sizeof(test_heap_buf), HEAP_TYPE_4);
    uint32_t *a = (uint32_t *)calloc_rtos(10, sizeof(uint32_t));
    CHECK(a != NULL, "calloc NULL");
    int z = 1; for (int i = 0; i < 10; i++) if (a[i] != 0) z = 0;
    CHECK(z, "not zeroed");
    free_rtos(a);
    PASS(); return 0;
}
static int test_heap_realloc(void) {
    TEST("realloc");
    heap_init(test_heap_buf, sizeof(test_heap_buf), HEAP_TYPE_4);
    void *p = malloc_rtos(128);
    CHECK(p != NULL, "initial malloc");
    p = realloc_rtos(p, 512);
    CHECK(p != NULL, "realloc grow");
    p = realloc_rtos(p, 64);
    CHECK(p != NULL, "realloc shrink");
    free_rtos(p);
    PASS(); return 0;
}
static int test_heap_fragmentation(void) {
    TEST("heap_fragmentation");
    heap_init(test_heap_buf, sizeof(test_heap_buf), HEAP_TYPE_4);
    CHECK(heap_fragmentation_permil() == 0, "frag != 0 for fresh heap");
    PASS(); return 0;
}

/* rwlock tests */
static int test_rwlock_create(void) {
    TEST("rwlock create");
    heap_init(test_heap_buf, sizeof(test_heap_buf), HEAP_TYPE_4);
    rwlock_t *rw = rwlock_create("rw");
    CHECK(rw != NULL, "create NULL");
    CHECK(rwlock_get_reader_count(rw) == 0, "readers != 0");
    CHECK(rwlock_is_write_locked(rw) == 0, "write locked");
    rwlock_delete(rw);
    PASS(); return 0;
}
static int test_rwlock_read(void) {
    TEST("rwlock read lock");
    heap_init(test_heap_buf, sizeof(test_heap_buf), HEAP_TYPE_4);
    rwlock_t *rw = rwlock_create("rw");
    CHECK(rwlock_read_lock(rw, 0) == 1, "read lock fail");
    CHECK(rwlock_get_reader_count(rw) == 1, "count != 1");
    CHECK(rwlock_read_unlock(rw) == 1, "read unlock fail");
    rwlock_delete(rw);
    PASS(); return 0;
}
static int test_rwlock_write(void) {
    TEST("rwlock write lock");
    heap_init(test_heap_buf, sizeof(test_heap_buf), HEAP_TYPE_4);
    rwlock_t *rw = rwlock_create("rw");
    CHECK(rwlock_write_lock(rw, 0) == 1, "write lock fail");
    CHECK(rwlock_is_write_locked(rw) == 1, "not write locked");
    CHECK(rwlock_write_unlock(rw) == 1, "write unlock fail");
    rwlock_delete(rw);
    PASS(); return 0;
}

/* event_groups tests */
static int test_event_group_create(void) {
    TEST("event_group create");
    heap_init(test_heap_buf, sizeof(test_heap_buf), HEAP_TYPE_4);
    event_group_t *eg = event_group_create("eg");
    CHECK(eg != NULL && event_group_get_bits(eg) == 0, "create fail");
    event_group_delete(eg);
    PASS(); return 0;
}
static int test_event_group_set_get(void) {
    TEST("event_group set/get");
    heap_init(test_heap_buf, sizeof(test_heap_buf), HEAP_TYPE_4);
    event_group_t *eg = event_group_create("eg");
    event_group_set_bits(eg, 0x0F);
    CHECK(event_group_get_bits(eg) == 0x0F, "set fail");
    event_group_clear_bits(eg, 0x03);
    CHECK(event_group_get_bits(eg) == 0x0C, "clear fail");
    event_group_delete(eg);
    PASS(); return 0;
}

/* trace_diag tests */
static int test_trace_event(void) {
    TEST("trace_event");
    heap_init(test_heap_buf, sizeof(test_heap_buf), HEAP_TYPE_4);
    trace_init();
    trace_event(TRACE_TASK_CREATE, 1, 0);
    CHECK(trace_event_count() == 1, "count != 1");
    trace_clear();
    CHECK(trace_event_count() == 0, "not cleared");
    PASS(); return 0;
}
static int test_trace_dump(void) {
    TEST("trace_dump");
    heap_init(test_heap_buf, sizeof(test_heap_buf), HEAP_TYPE_4);
    trace_init();
    trace_event(TRACE_TASK_CREATE, 1, 100);
    trace_event_t buf[4];
    CHECK(trace_dump(buf, 4) == 1, "dump count != 1");
    CHECK(buf[0].type == TRACE_TASK_CREATE, "type mismatch");
    trace_clear();
    PASS(); return 0;
}
static int test_cpu_stats(void) {
    TEST("cpu_stats");
    reset_system();
    cpu_stats_init();
    cpu_stats_t s = cpu_stats_get();
    CHECK(s.total_ticks == 0 && s.cpu_usage_percent == 0, "stats init fail");
    PASS(); return 0;
}
static int test_rma_schedulability(void) {
    TEST("rma_schedulability");
    rma_task_t tasks[3];
    uint32_t util;
    tasks[0].period_ticks = 100; tasks[0].wcet_ticks = 10;
    tasks[1].period_ticks = 200; tasks[1].wcet_ticks = 20;
    tasks[2].period_ticks = 400; tasks[2].wcet_ticks = 40;
    CHECK(rma_schedulability_test(tasks, 3, &util) == 1, "RMA fail");
    CHECK(util < 400, "util too high");
    PASS(); return 0;
}

int main(void) {
    printf("\n=== mini-rtos Unit Tests ===\n\n");
    test_scheduler_init();
    test_task_create();
    test_task_suspend_resume();
    test_task_yield();
    test_task_get_tick_count();
    test_queue_create_delete();
    test_queue_send_receive();
    test_queue_reset();
    test_queue_set();
    test_semaphore_counting();
    test_semaphore_binary();
    test_mutex_lock_unlock();
    test_mutex_recursive();
    test_timer_create_start_stop();
    test_timer_change_period();
    test_heap_malloc_free();
    test_heap_calloc();
    test_heap_realloc();
    test_heap_fragmentation();
    test_rwlock_create();
    test_rwlock_read();
    test_rwlock_write();
    test_event_group_create();
    test_event_group_set_get();
    test_trace_event();
    test_trace_dump();
    test_cpu_stats();
    test_rma_schedulability();
    printf("\n%d / %d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
