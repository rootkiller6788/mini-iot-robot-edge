#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H

#include <stdint.h>

#define TASK_PRIORITY_HIGHEST  32
#define TASK_PRIORITY_HIGH     24
#define TASK_PRIORITY_NORMAL   16
#define TASK_PRIORITY_LOW      8
#define TASK_PRIORITY_LOWEST   4
#define TASK_PRIORITY_IDLE     0
#define TASK_PRIORITY_MAX      (TASK_PRIORITY_HIGHEST + 1)
#define TASK_STACK_MIN         128
#define TASK_STACK_WATERMARK   0xA5A5A5A5U
#define TASK_NAME_MAX_LEN      16

typedef enum { TASK_READY, TASK_RUNNING, TASK_BLOCKED, TASK_SUSPENDED, TASK_DELETED } task_state_t;
typedef void (*task_entry_t)(void *param);

struct tcb;
typedef struct tcb tcb_t;

struct tcb {
    uint32_t    *sp;
    uint32_t    *stack_start;
    uint32_t     stack_size;
    uint32_t     priority;
    uint32_t     base_priority;
    task_state_t state;
    task_entry_t entry;
    void        *param;
    uint32_t     time_slice;
    uint32_t     runtime_ticks;
    uint32_t     ticks_remaining;
    void        *block_obj;
    tcb_t       *block_next;
    tcb_t       *next;
    tcb_t       *prev;
    char         name[TASK_NAME_MAX_LEN];
    uint32_t     owner_original_priority;
};

void     scheduler_init(void);
tcb_t   *task_create(task_entry_t entry, const char *name, uint32_t stack_size, void *param, uint32_t priority);
void     task_delete(tcb_t *task);
void     task_suspend(tcb_t *task);
void     task_resume(tcb_t *task);
void     task_delay(uint32_t ticks);
void     task_delay_until(uint32_t *last_wake, uint32_t ticks);
void     task_yield(void);
uint32_t task_get_tick_count(void);
tcb_t   *task_get_current(void);
void     task_enter_critical(void);
void     task_exit_critical(void);
void     scheduler_start(void);
void     scheduler_tick_handler(void);
void     pendsv_handler(void);
uint32_t task_stack_free(tcb_t *task);
void     task_stack_watermark_check(tcb_t *task);
void     task_list_debug(void);

void port_start_first_task(void);
void port_save_context(void);
void port_restore_context(void);

#endif
