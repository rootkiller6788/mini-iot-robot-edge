#include "task_scheduler.h"
#include "ipc_queue.h"
#include "memory_heap.h"
#include <string.h>

static queue_t *registry_head = NULL;
static uint32_t registry_count = 0;

static void block_task_on_queue(queue_t *q, struct tcb *task, uint8_t is_sender)
{
    task->state = TASK_BLOCKED;
    task->block_obj = q;
    if (is_sender) {
        task->block_next = q->blocked_senders;
        q->blocked_senders = task;
    } else {
        task->block_next = q->blocked_receivers;
        q->blocked_receivers = task;
    }
}

static void unblock_task_from_queue(queue_t *q, uint8_t is_sender)
{
    struct tcb **list = is_sender ? &q->blocked_senders : &q->blocked_receivers;
    struct tcb *task = *list;
    if (task) {
        *list = task->block_next;
        task->state = TASK_READY;
        task->block_obj = NULL;
        task->block_next = NULL;
    }
}

static inline uint32_t queue_is_full(const queue_t *q)
{
    return q->count >= q->max_items;
}

static inline uint32_t queue_is_empty(const queue_t *q)
{
    return q->count == 0;
}

static void queue_copy_in(queue_t *q, const void *data)
{
    uint8_t *dest = &q->buffer[q->head * q->item_size];
    memcpy(dest, data, q->item_size);
    q->head = (q->head + 1) % q->max_items;
    q->count++;
}

static void queue_copy_out(queue_t *q, void *buffer)
{
    uint8_t *src = &q->buffer[q->tail * q->item_size];
    memcpy(buffer, src, q->item_size);
    q->tail = (q->tail + 1) % q->max_items;
    q->count--;
}

queue_t *queue_create(uint32_t item_size, uint32_t max_items, const char *name)
{
    queue_t *q;
    if (item_size == 0 || max_items == 0) return NULL;
    q = (queue_t *)malloc_rtos(sizeof(queue_t));
    if (!q) return NULL;
    q->buffer = (uint8_t *)malloc_rtos(item_size * max_items);
    if (!q->buffer) {
        free_rtos(q);
        return NULL;
    }
    memset(q, 0, sizeof(queue_t));
    q->item_size = item_size;
    q->max_items = max_items;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->blocked_senders = NULL;
    q->blocked_receivers = NULL;
    q->isr_safe = 1;
    if (name) {
        strncpy(q->name, name, 15);
        q->name[15] = '\0';
    }
    return q;
}

void queue_delete(queue_t *q)
{
    if (!q) return;
    task_enter_critical();
    {
        struct tcb *task = q->blocked_senders;
        while (task) {
            struct tcb *next = task->block_next;
            task->state = TASK_READY;
            task->block_obj = NULL;
            task = next;
        }
        task = q->blocked_receivers;
        while (task) {
            struct tcb *next = task->block_next;
            task->state = TASK_READY;
            task->block_obj = NULL;
            task = next;
        }
        queue_registry_remove(q);
    }
    task_exit_critical();
    free_rtos(q->buffer);
    free_rtos(q);
}

int32_t queue_send(queue_t *q, const void *data, uint32_t timeout_ticks)
{
    uint32_t start = task_get_tick_count();
    if (!q || !data) return -1;
    task_enter_critical();
    while (queue_is_full(q)) {
        if (timeout_ticks == 0) {
            task_exit_critical();
            return 0;
        }
        block_task_on_queue(q, task_get_current(), 1);
        task_exit_critical();
        task_delay(1);
        if (timeout_ticks < (uint32_t)-1) {
            uint32_t elapsed = task_get_tick_count() - start;
            if (elapsed >= timeout_ticks) {
                task_enter_critical();
                {
                    struct tcb **list = &q->blocked_senders;
                    struct tcb *prev = NULL;
                    struct tcb *t = *list;
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
    queue_copy_in(q, data);
    if (q->blocked_receivers) {
        unblock_task_from_queue(q, 0);
    }
    task_exit_critical();
    return 1;
}

int32_t queue_receive(queue_t *q, void *buffer, uint32_t timeout_ticks)
{
    uint32_t start = task_get_tick_count();
    if (!q || !buffer) return -1;
    task_enter_critical();
    while (queue_is_empty(q)) {
        if (timeout_ticks == 0) {
            task_exit_critical();
            return 0;
        }
        block_task_on_queue(q, task_get_current(), 0);
        task_exit_critical();
        task_delay(1);
        if (timeout_ticks < (uint32_t)-1) {
            uint32_t elapsed = task_get_tick_count() - start;
            if (elapsed >= timeout_ticks) {
                task_enter_critical();
                {
                    struct tcb **list = &q->blocked_receivers;
                    struct tcb *prev = NULL;
                    struct tcb *t = *list;
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
    queue_copy_out(q, buffer);
    if (q->blocked_senders) {
        unblock_task_from_queue(q, 1);
    }
    task_exit_critical();
    return 1;
}

int32_t queue_send_from_isr(queue_t *q, const void *data, int32_t *woken)
{
    if (!q || !data) return -1;
    if (queue_is_full(q)) return 0;
    queue_copy_in(q, data);
    if (q->blocked_receivers) {
        unblock_task_from_queue(q, 0);
        if (woken) *woken = 1;
    }
    return 1;
}

int32_t queue_receive_from_isr(queue_t *q, void *buffer, int32_t *woken)
{
    if (!q || !buffer) return -1;
    if (queue_is_empty(q)) return 0;
    queue_copy_out(q, buffer);
    if (q->blocked_senders) {
        unblock_task_from_queue(q, 1);
        if (woken) *woken = 1;
    }
    return 1;
}

uint32_t queue_messages_waiting(const queue_t *q)
{
    if (!q) return 0;
    return q->count;
}

uint32_t queue_spaces_available(const queue_t *q)
{
    if (!q) return 0;
    return q->max_items - q->count;
}

void queue_reset(queue_t *q)
{
    if (!q) return;
    task_enter_critical();
    {
        q->head = 0;
        q->tail = 0;
        q->count = 0;
    }
    task_exit_critical();
}

void queue_registry_add(queue_t *q)
{
    if (!q) return;
    task_enter_critical();
    {
        q->next = registry_head;
        registry_head = q;
        registry_count++;
    }
    task_exit_critical();
}

void queue_registry_remove(queue_t *q)
{
    if (!q || !registry_head) return;
    task_enter_critical();
    {
        if (registry_head == q) {
            registry_head = q->next;
        } else {
            queue_t *prev = registry_head;
            while (prev->next && prev->next != q) {
                prev = prev->next;
            }
            if (prev->next == q) {
                prev->next = q->next;
            }
        }
        registry_count--;
    }
    task_exit_critical();
}

uint32_t queue_registry_count(void)
{
    return registry_count;
}

queue_set_t *queue_set_create(uint32_t max_queues)
{
    queue_set_t *s;
    if (max_queues == 0) return NULL;
    s = (queue_set_t *)malloc_rtos(sizeof(queue_set_t));
    if (!s) return NULL;
    s->queues = (queue_t **)malloc_rtos(sizeof(queue_t *) * max_queues);
    if (!s->queues) {
        free_rtos(s);
        return NULL;
    }
    s->max_queues = max_queues;
    s->count = 0;
    s->observer = NULL;
    return s;
}

void queue_set_add(queue_set_t *s, queue_t *q)
{
    if (!s || !q || s->count >= s->max_queues) return;
    task_enter_critical();
    s->queues[s->count++] = q;
    task_exit_critical();
}

void queue_set_remove(queue_set_t *s, queue_t *q)
{
    uint32_t i;
    if (!s || !q) return;
    task_enter_critical();
    for (i = 0; i < s->count; i++) {
        if (s->queues[i] == q) {
            s->queues[i] = s->queues[--s->count];
            break;
        }
    }
    task_exit_critical();
}

queue_t *queue_set_select(queue_set_t *s, uint32_t timeout_ticks)
{
    uint32_t i;
    if (!s) return NULL;
    (void)timeout_ticks;
    for (i = 0; i < s->count; i++) {
        if (s->queues[i] && !queue_is_empty(s->queues[i])) {
            return s->queues[i];
        }
    }
    return NULL;
}
