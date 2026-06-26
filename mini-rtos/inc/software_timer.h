#ifndef SOFTWARE_TIMER_H
#define SOFTWARE_TIMER_H

#include <stdint.h>
#include "task_scheduler.h"

typedef enum { TIMER_TYPE_ONE_SHOT, TIMER_TYPE_AUTO_RELOAD } timer_type_t;

typedef enum {
    TIMER_CMD_START,
    TIMER_CMD_STOP,
    TIMER_CMD_RESET,
    TIMER_CMD_CHANGE_PERIOD,
    TIMER_CMD_DELETE
} timer_cmd_type_t;

typedef void (*timer_callback_t)(void *param);

typedef struct sw_timer {
    uint32_t          id;
    timer_type_t      type;
    timer_callback_t  callback;
    void             *param;
    uint32_t          period_ticks;
    uint32_t          expiry_tick;
    uint32_t          active;
    char              name[16];
    struct sw_timer  *next;
} sw_timer_t;

typedef struct timer_cmd {
    timer_cmd_type_t  type;
    sw_timer_t       *timer;
    uint32_t          period;
    struct timer_cmd *next;
} timer_cmd_t;

void     timer_daemon_init(uint32_t stack_size, uint32_t priority);
sw_timer_t *timer_create(const char *name, timer_callback_t cb, void *param, uint32_t period_ticks, timer_type_t type);
int32_t  timer_start(sw_timer_t *t, uint32_t delay_ticks);
int32_t  timer_stop(sw_timer_t *t);
int32_t  timer_reset(sw_timer_t *t, uint32_t delay_ticks);
int32_t  timer_change_period(sw_timer_t *t, uint32_t new_period_ticks);
void     timer_delete(sw_timer_t *t);
int32_t  timer_start_from_isr(sw_timer_t *t, uint32_t delay_ticks, int32_t *woken);
int32_t  timer_stop_from_isr(sw_timer_t *t, int32_t *woken);
int32_t  timer_reset_from_isr(sw_timer_t *t, uint32_t delay_ticks, int32_t *woken);
uint32_t timer_is_active(const sw_timer_t *t);
uint32_t timer_get_id(const sw_timer_t *t);
uint32_t timer_get_remaining(const sw_timer_t *t);

#endif
