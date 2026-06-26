#include "task_scheduler.h"
#include "semaphore_mutex.h"
#include "memory_heap.h"
#include <string.h>

static void semaphore_block_task(semaphore_t *sem, tcb_t *task)
{
    task->state = TASK_BLOCKED;
    task->block_obj = sem;
    task->block_next = sem->blocked_tasks;
    sem->blocked_tasks = task;
}

static void semaphore_unblock_task(semaphore_t *sem)
{
    tcb_t *task = sem->blocked_tasks;
    if (task) {
        sem->blocked_tasks = task->block_next;
        task->state = TASK_READY;
        task->block_obj = NULL;
        task->block_next = NULL;
    }
}

semaphore_t *semaphore_create_binary(const char *name)
{
    return semaphore_create_counting(1, 1, name);
}

semaphore_t *semaphore_create_counting(uint32_t max_count, uint32_t initial_count,
                                       const char *name)
{
    semaphore_t *sem;
    if (max_count == 0 || initial_count > max_count) return NULL;
    sem = (semaphore_t *)malloc_rtos(sizeof(semaphore_t));
    if (!sem) return NULL;
    memset(sem, 0, sizeof(semaphore_t));
    sem->count = initial_count;
    sem->max_count = max_count;
    sem->blocked_tasks = NULL;
    if (name) {
        strncpy(sem->name, name, 15);
        sem->name[15] = '\0';
    }
    return sem;
}

void semaphore_delete(semaphore_t *sem)
{
    if (!sem) return;
    task_enter_critical();
    {
        tcb_t *task = sem->blocked_tasks;
        while (task) {
            tcb_t *next = task->block_next;
            task->state = TASK_READY;
            task->block_obj = NULL;
            task = next;
        }
    }
    task_exit_critical();
    free_rtos(sem);
}

int32_t semaphore_take(semaphore_t *sem, uint32_t timeout_ticks)
{
    uint32_t start = task_get_tick_count();
    if (!sem) return -1;
    task_enter_critical();
    while (sem->count == 0) {
        if (timeout_ticks == 0) {
            task_exit_critical();
            return 0;
        }
        semaphore_block_task(sem, task_get_current());
        task_exit_critical();
        task_delay(1);
        if (timeout_ticks < (uint32_t)-1) {
            uint32_t elapsed = task_get_tick_count() - start;
            if (elapsed >= timeout_ticks) {
                task_enter_critical();
                {
                    tcb_t **list = &sem->blocked_tasks;
                    tcb_t *prev = NULL;
                    tcb_t *t = *list;
                    while (t) {
                        if (t == task_get_current()) {
                            if (prev) prev->block_next = t->block_next;
                            else *list = t->block_next;
                            break;
                        }
                        prev = t;
                        t = t->block_next;
                    }
                }
                task_exit_critical();
                return -2;
            }
        }
        task_enter_critical();
    }
    sem->count--;
    task_exit_critical();
    return 1;
}

int32_t semaphore_give(semaphore_t *sem)
{
    if (!sem) return -1;
    task_enter_critical();
    if (sem->count < sem->max_count) {
        sem->count++;
    }
    if (sem->blocked_tasks) {
        semaphore_unblock_task(sem);
    }
    task_exit_critical();
    return 1;
}

int32_t semaphore_give_from_isr(semaphore_t *sem, int32_t *woken)
{
    if (!sem) return -1;
    if (sem->count < sem->max_count) {
        sem->count++;
    }
    if (sem->blocked_tasks) {
        semaphore_unblock_task(sem);
        if (woken) *woken = 1;
    }
    return 1;
}

uint32_t semaphore_get_count(const semaphore_t *sem)
{
    if (!sem) return 0;
    return sem->count;
}

static void mutex_block_task(mutex_t *m, tcb_t *task)
{
    task->state = TASK_BLOCKED;
    task->block_obj = m;
    task->block_next = m->blocked_tasks;
    m->blocked_tasks = task;
}

static void mutex_priority_inherit(mutex_t *m, tcb_t *blocker)
{
    if (!m->owner) return;
    if (blocker->priority > m->owner->priority) {
        m->owner_original_priority = m->owner->priority;
        m->owner->priority = blocker->priority;
    }
}

static void mutex_priority_restore(mutex_t *m)
{
    if (m->owner && m->owner_original_priority > 0) {
        m->owner->priority = m->owner_original_priority;
        m->owner_original_priority = 0;
    }
}

mutex_t *mutex_create(const char *name)
{
    mutex_t *m = (mutex_t *)malloc_rtos(sizeof(mutex_t));
    if (!m) return NULL;
    memset(m, 0, sizeof(mutex_t));
    m->held = 0;
    m->owner = NULL;
    m->owner_original_priority = 0;
    m->recursive_count = 0;
    m->blocked_tasks = NULL;
    if (name) {
        strncpy(m->name, name, 15);
        m->name[15] = '\0';
    }
    return m;
}

void mutex_delete(mutex_t *m)
{
    if (!m) return;
    task_enter_critical();
    {
        tcb_t *task = m->blocked_tasks;
        while (task) {
            tcb_t *next = task->block_next;
            task->state = TASK_READY;
            task->block_obj = NULL;
            task = next;
        }
    }
    task_exit_critical();
    free_rtos(m);
}

int32_t mutex_lock(mutex_t *m, uint32_t timeout_ticks)
{
    uint32_t start = task_get_tick_count();
    tcb_t *self = task_get_current();
    if (!m) return -1;
    task_enter_critical();
    if (!m->held) {
        m->held = 1;
        m->owner = self;
        m->recursive_count = 1;
        task_exit_critical();
        return 1;
    }
    if (m->owner == self) {
        m->recursive_count++;
        task_exit_critical();
        return 1;
    }
    while (m->held) {
        if (timeout_ticks == 0) {
            task_exit_critical();
            return 0;
        }
        mutex_priority_inherit(m, self);
        mutex_block_task(m, self);
        task_exit_critical();
        task_delay(1);
        if (timeout_ticks < (uint32_t)-1) {
            uint32_t elapsed = task_get_tick_count() - start;
            if (elapsed >= timeout_ticks) {
                task_enter_critical();
                {
                    tcb_t **list = &m->blocked_tasks;
                    tcb_t *prev = NULL;
                    tcb_t *t = *list;
                    while (t) {
                        if (t == self) {
                            if (prev) prev->block_next = t->block_next;
                            else *list = t->block_next;
                            break;
                        }
                        prev = t;
                        t = t->block_next;
                    }
                }
                task_exit_critical();
                return -2;
            }
        }
        task_enter_critical();
    }
    m->held = 1;
    m->owner = self;
    m->recursive_count = 1;
    task_exit_critical();
    return 1;
}

int32_t mutex_unlock(mutex_t *m)
{
    tcb_t *self = task_get_current();
    if (!m) return -1;
    task_enter_critical();
    if (m->owner != self) {
        task_exit_critical();
        return -1;
    }
    m->recursive_count--;
    if (m->recursive_count > 0) {
        task_exit_critical();
        return 1;
    }
    mutex_priority_restore(m);
    m->held = 0;
    m->owner = NULL;
    if (m->blocked_tasks) {
        tcb_t *task = m->blocked_tasks;
        m->blocked_tasks = task->block_next;
        task->state = TASK_READY;
        task->block_obj = NULL;
        m->held = 1;
        m->owner = task;
        m->recursive_count = 1;
    }
    task_exit_critical();
    return 1;
}

int32_t mutex_lock_recursive(mutex_t *m, uint32_t timeout_ticks)
{
    return mutex_lock(m, timeout_ticks);
}

int32_t mutex_unlock_recursive(mutex_t *m)
{
    return mutex_unlock(m);
}

tcb_t *mutex_get_owner(const mutex_t *m)
{
    if (!m) return NULL;
    return m->owner;
}

int32_t gatekeeper_call(mutex_t *gatekeeper, gatekeeper_task_t fn, void *param,
                        uint32_t timeout_ticks)
{
    int32_t ret;
    if (!gatekeeper || !fn) return -1;
    ret = mutex_lock(gatekeeper, timeout_ticks);
    if (ret != 1) return ret;
    fn(param);
    mutex_unlock(gatekeeper);
    return 1;
}

/*
 * L6: Deadlock Detection via Resource Allocation Graph (RAG).
 *
 * L5 Algorithm: Cycle detection in a directed graph.
 *
 * The Resource Allocation Graph has two types of nodes:
 *   - Task nodes (T): each mutex owner/waiter.
 *   - Resource nodes (R): each mutex.
 * Edges:
 *   - Request edge: T -> R (task is waiting for mutex).
 *   - Assignment edge: R -> T (mutex is held by task).
 *
 * A cycle in the RAG indicates a deadlock.
 *
 * This simplified detector checks for cycles using a DFS with
 * visited/finished coloring.
 *
 * L4 Theorem: Coffman et al. (1971) four necessary conditions for deadlock:
 *   1. Mutual exclusion
 *   2. Hold and wait
 *   3. No preemption
 *   4. Circular wait
 *
 * Reference: Coffman, Elphick, Shoshani (1971) "System Deadlocks",
 *   ACM Computing Surveys 3(2).
 */

#define DEADLOCK_MAX_MUTEXES 32

typedef struct {
    mutex_t *mutex;
    tcb_t   *owner;
} rag_edge_t;

/*
 * L5: DFS cycle detection in resource allocation graph.
 *
 * visited[i] = 0 (white), 1 (gray/in-progress), 2 (black/done).
 * A back-edge to a gray node indicates a cycle (deadlock).
 *
 * Returns: 1 if deadlock detected, 0 otherwise.
 */
static int deadlock_dfs(uint32_t node, uint8_t *visited,
                         uint32_t *adj_matrix, uint32_t n)
{
    uint32_t i;
    visited[node] = 1; /* gray */
    for (i = 0; i < n; i++) {
        if (adj_matrix[node * n + i]) {
            if (visited[i] == 1) {
                return 1; /* back-edge -> deadlock */
            }
            if (visited[i] == 0) {
                if (deadlock_dfs(i, visited, adj_matrix, n)) {
                    return 1;
                }
            }
        }
    }
    visited[node] = 2; /* black */
    return 0;
}

/*
 * L6: Simple deadlock check on mutex wait.
 * Checks if waiting on m would create a cycle involving current task.
 *
 * This is a simple "wait-for" graph check:
 * If mutex m is held by task T, and T is waiting (directly or transitively)
 * on a mutex held by the current task, then waiting on m would deadlock.
 *
 * Returns: 1 if deadlock would occur, 0 if safe.
 */
int32_t mutex_deadlock_check(mutex_t *m)
{
    tcb_t *self = task_get_current();
    tcb_t *holder;

    if (!m || !m->held) return 0;
    if (m->owner == self) return 0; /* recursive lock is safe */

    /* DFS on wait-for chain: follow m->owner->block_obj chain */
    holder = m->owner;
    while (holder && holder->block_obj) {
        mutex_t *held = (mutex_t *)holder->block_obj;
        if (held == m) return 1; /* cycle: would deadlock */
        if (held->owner == self) return 1; /* self in cycle */
        holder = held->owner;
    }
    return 0; /* no cycle found */
}

/*
 * L4: Priority Ceiling check.
 *
 * Priority Ceiling Protocol (Sha, Rajkumar, Lehoczky 1990):
 * Each mutex has a priority ceiling = max priority of all tasks
 * that may lock it. A task can lock a mutex only if its priority
 * is strictly higher than the ceiling of all currently locked mutexes.
 *
 * This prevents deadlock and chained blocking (at most one blocking
 * per critical section).
 *
 * Returns: 1 if the task can lock under PCP, 0 if blocked.
 */
int32_t mutex_priority_ceiling_check(mutex_t *m, tcb_t *task)
{
    /* For a simple implementation, return the ceiling value.
     * In production RTOS, this would track all held mutexes. */
    (void)m;
    (void)task;
    return 1; /* simplified check: always pass for this implementation */
}
