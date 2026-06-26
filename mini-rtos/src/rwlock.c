/*
 * rwlock.c -- Reader-Writer Lock Implementation
 *
 * L3 Engineering Structure: Write-preferring reader-writer lock.
 *
 * L5 Algorithm: Write-preferring policy prevents writer starvation.
 *
 * Locking rules:
 *   1. Read-lock granted IF no writer active AND no writer waiting.
 *   2. Write-lock granted IF no readers AND no writer active.
 *   3. Writer-waiting flag blocks new readers (write-preferring).
 *
 * Unlocking rules:
 *   1. Read-unlock decrements readers; if 0, wake a waiting writer.
 *   2. Write-unlock clears writer_active; wake writers first, then readers.
 *
 * Fairness analysis (L4):
 *   Write-preferring ensures bounded waiting for writers but can cause
 *   reader starvation under heavy write load. Courtois et al. (1971).
 *
 * Course: MIT 6.004, Berkeley CS 162, CMU 15-410
 */
#include "task_scheduler.h"
#include "rwlock.h"
#include "memory_heap.h"
#include <string.h>

static void rwlock_block_reader(rwlock_t *rw, struct tcb *task)
{
    task->state = TASK_BLOCKED;
    task->block_obj = rw;
    task->block_next = rw->blocked_readers;
    rw->blocked_readers = task;
}

static void rwlock_block_writer(rwlock_t *rw, struct tcb *task)
{
    task->state = TASK_BLOCKED;
    task->block_obj = rw;
    task->block_next = rw->blocked_writers;
    rw->blocked_writers = task;
}

/* L5: Unblock all pending readers (called when write lock released) */
static void rwlock_unblock_all_readers(rwlock_t *rw)
{
    struct tcb *task;
    while ((task = rw->blocked_readers) != NULL) {
        rw->blocked_readers = task->block_next;
        task->state = TASK_READY;
        task->block_obj = NULL;
        task->block_next = NULL;
        rw->readers++;
    }
}

/* L5: Unblock first waiting writer */
static void rwlock_unblock_one_writer(rwlock_t *rw)
{
    struct tcb *task = rw->blocked_writers;
    if (task) {
        rw->blocked_writers = task->block_next;
        task->state = TASK_READY;
        task->block_obj = NULL;
        task->block_next = NULL;
        rw->writer_active = 1;
        if (!rw->blocked_writers) rw->writer_waiting = 0;
    }
}

rwlock_t *rwlock_create(const char *name)
{
    rwlock_t *rw = (rwlock_t *)malloc_rtos(sizeof(rwlock_t));
    if (!rw) return NULL;
    memset(rw, 0, sizeof(rwlock_t));
    if (name) { strncpy(rw->name, name, 15); rw->name[15] = 0; }
    return rw;
}

void rwlock_delete(rwlock_t *rw)
{
    if (!rw) return;
    task_enter_critical();
    {
        struct tcb *task;
        while ((task = rw->blocked_readers) != NULL) {
            rw->blocked_readers = task->block_next;
            task->state = TASK_READY;
            task->block_obj = NULL;
            task->block_next = NULL;
        }
        while ((task = rw->blocked_writers) != NULL) {
            rw->blocked_writers = task->block_next;
            task->state = TASK_READY;
            task->block_obj = NULL;
            task->block_next = NULL;
        }
    }
    task_exit_critical();
    free_rtos(rw);
}

/*
 * L5: Acquire read lock with write-preferring policy.
 * Grants if no writer active AND no writer waiting.
 * Returns 1 on success, 0 on immediate fail, -2 on timeout.
 */
int32_t rwlock_read_lock(rwlock_t *rw, uint32_t timeout_ticks)
{
    uint32_t start;
    if (!rw) return -1;
    start = task_get_tick_count();
    task_enter_critical();
    while (rw->writer_active || rw->writer_waiting) {
        if (timeout_ticks == 0) { task_exit_critical(); return 0; }
        rwlock_block_reader(rw, task_get_current());
        task_exit_critical();
        task_delay(1);
        if (timeout_ticks < (uint32_t)-1) {
            uint32_t elapsed = task_get_tick_count() - start;
            if (elapsed >= timeout_ticks) {
                task_enter_critical();
                {
                    struct tcb **list = &rw->blocked_readers;
                    struct tcb *prev = NULL, *t = *list;
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
                return -2;
            }
        }
        task_enter_critical();
    }
    rw->readers++;
    task_exit_critical();
    return 1;
}

int32_t rwlock_read_unlock(rwlock_t *rw)
{
    if (!rw || rw->readers == 0) return -1;
    task_enter_critical();
    rw->readers--;
    if (rw->readers == 0 && rw->blocked_writers) {
        rwlock_unblock_one_writer(rw);
    }
    task_exit_critical();
    return 1;
}

/*
 * L5: Acquire write lock (exclusive access).
 * Sets writer_waiting before blocking to enable write-preferring.
 */
int32_t rwlock_write_lock(rwlock_t *rw, uint32_t timeout_ticks)
{
    uint32_t start;
    if (!rw) return -1;
    start = task_get_tick_count();
    task_enter_critical();
    rw->writer_waiting = 1;
    while (rw->writer_active || rw->readers > 0) {
        if (timeout_ticks == 0) {
            if (!rw->blocked_writers) rw->writer_waiting = 0;
            task_exit_critical();
            return 0;
        }
        rwlock_block_writer(rw, task_get_current());
        task_exit_critical();
        task_delay(1);
        if (timeout_ticks < (uint32_t)-1) {
            uint32_t elapsed = task_get_tick_count() - start;
            if (elapsed >= timeout_ticks) {
                task_enter_critical();
                {
                    struct tcb **list = &rw->blocked_writers;
                    struct tcb *prev = NULL, *t = *list;
                    while (t) {
                        if (t == task_get_current()) {
                            if (prev) prev->block_next = t->block_next;
                            else *list = t->block_next;
                            t->block_next = NULL; t->block_obj = NULL;
                            break;
                        }
                        prev = t; t = t->block_next;
                    }
                    if (!rw->blocked_writers) rw->writer_waiting = 0;
                }
                task_exit_critical();
                return -2;
            }
        }
        task_enter_critical();
    }
    rw->writer_active = 1;
    rw->writer_waiting = 0;
    task_exit_critical();
    return 1;
}

/*
 * L5: Release write lock. Wake writers first, then readers.
 */
int32_t rwlock_write_unlock(rwlock_t *rw)
{
    if (!rw || !rw->writer_active) return -1;
    task_enter_critical();
    rw->writer_active = 0;
    if (rw->blocked_writers)
        rwlock_unblock_one_writer(rw);
    else if (rw->blocked_readers)
        rwlock_unblock_all_readers(rw);
    task_exit_critical();
    return 1;
}

uint32_t rwlock_get_reader_count(const rwlock_t *rw)
{
    return rw ? rw->readers : 0;
}

uint32_t rwlock_is_write_locked(const rwlock_t *rw)
{
    return rw ? rw->writer_active : 0;
}

/* L7 Application: Non-blocking try-lock for optimistic concurrency */
int32_t rwlock_read_trylock(rwlock_t *rw)
{
    return rwlock_read_lock(rw, 0);
}

int32_t rwlock_write_trylock(rwlock_t *rw)
{
    return rwlock_write_lock(rw, 0);
}
