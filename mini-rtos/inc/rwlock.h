/*
 * rwlock.h -- Reader-Writer Lock for mini-rtos
 *
 * L3 Engineering Structure: Reader-writer synchronization.
 * Allows concurrent readers OR exclusive writer access.
 * Write-preferring fairness policy prevents writer starvation.
 *
 * L4 Standard: Courtois et al. (1971) "Concurrent Control with
 * Readers and Writers", CACM 14(10).
 *
 * L5 Algorithm: Write-preferring RW lock.
 *   Writer-waiting flag blocks new readers, preventing starvation.
 *   Nested read-locks via per-task depth counter.
 *   Write lock is exclusive and non-recursive.
 *
 * L7 Application: Shared configuration access (many readers,
 *   infrequent updates), database page cache, file system metadata.
 *
 * Course: MIT 6.004/6.828, CMU 15-410, Berkeley CS 162
 */
#ifndef RWLOCK_H
#define RWLOCK_H

#include <stdint.h>
#include "task_scheduler.h"

typedef struct rwlock {
    uint32_t     readers;
    uint32_t     writer_active;
    uint32_t     writer_waiting;
    struct tcb  *blocked_readers;
    struct tcb  *blocked_writers;
    char         name[16];
} rwlock_t;

rwlock_t *rwlock_create(const char *name);
void      rwlock_delete(rwlock_t *rw);

int32_t   rwlock_read_lock(rwlock_t *rw, uint32_t timeout_ticks);
int32_t   rwlock_read_unlock(rwlock_t *rw);

int32_t   rwlock_write_lock(rwlock_t *rw, uint32_t timeout_ticks);
int32_t   rwlock_write_unlock(rwlock_t *rw);

uint32_t  rwlock_get_reader_count(const rwlock_t *rw);
uint32_t  rwlock_is_write_locked(const rwlock_t *rw);

/* L7: Try-lock variants for non-blocking protocols */
int32_t   rwlock_read_trylock(rwlock_t *rw);
int32_t   rwlock_write_trylock(rwlock_t *rw);

#endif /* RWLOCK_H */
