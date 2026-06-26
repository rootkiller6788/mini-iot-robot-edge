#ifndef SEMAPHORE_MUTEX_H
#define SEMAPHORE_MUTEX_H

#include <stdint.h>
#include "task_scheduler.h"

typedef struct {
    uint32_t     count;
    uint32_t     max_count;
    tcb_t       *blocked_tasks;
    char         name[16];
} semaphore_t;

typedef struct {
    uint32_t     held;
    tcb_t       *owner;
    uint32_t     owner_original_priority;
    uint32_t     recursive_count;
    tcb_t       *blocked_tasks;
    char         name[16];
} mutex_t;

typedef void (*gatekeeper_task_t)(void *param);

semaphore_t *semaphore_create_binary(const char *name);
semaphore_t *semaphore_create_counting(uint32_t max_count, uint32_t initial_count, const char *name);
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

int32_t      gatekeeper_call(mutex_t *gatekeeper, gatekeeper_task_t fn, void *param, uint32_t timeout_ticks);

#endif
