#ifndef IPC_QUEUE_H
#define IPC_QUEUE_H

#include <stdint.h>
#include "task_scheduler.h"

typedef struct queue {
    uint8_t     *buffer;
    uint32_t     item_size;
    uint32_t     max_items;
    uint32_t     head;
    uint32_t     tail;
    uint32_t     count;
    tcb_t       *blocked_senders;
    tcb_t       *blocked_receivers;
    uint8_t      isr_safe;
    char         name[16];
    struct queue *next;
} queue_t;

typedef struct {
    queue_t   **queues;
    uint32_t    max_queues;
    uint32_t    count;
    void       *observer;
} queue_set_t;

queue_t  *queue_create(uint32_t item_size, uint32_t max_items, const char *name);
void      queue_delete(queue_t *q);
int32_t   queue_send(queue_t *q, const void *data, uint32_t timeout_ticks);
int32_t   queue_receive(queue_t *q, void *buffer, uint32_t timeout_ticks);
int32_t   queue_send_from_isr(queue_t *q, const void *data, int32_t *woken);
int32_t   queue_receive_from_isr(queue_t *q, void *buffer, int32_t *woken);
uint32_t  queue_messages_waiting(const queue_t *q);
uint32_t  queue_spaces_available(const queue_t *q);
void      queue_reset(queue_t *q);
void      queue_registry_add(queue_t *q);
void      queue_registry_remove(queue_t *q);
uint32_t  queue_registry_count(void);

queue_set_t *queue_set_create(uint32_t max_queues);
void         queue_set_add(queue_set_t *s, queue_t *q);
void         queue_set_remove(queue_set_t *s, queue_t *q);
queue_t     *queue_set_select(queue_set_t *s, uint32_t timeout_ticks);

#endif
