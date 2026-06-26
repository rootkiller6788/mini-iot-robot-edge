#include "task_scheduler.h"
#include "software_timer.h"
#include "ipc_queue.h"
#include "memory_heap.h"
#include <string.h>

static sw_timer_t  *timer_list = NULL;
static uint32_t     next_timer_id = 1;
static timer_cmd_t *cmd_queue_head = NULL;
static timer_cmd_t *cmd_queue_tail = NULL;
static queue_t     *cmd_queue = NULL;

static uint8_t timer_daemon_initialized = 0;
static tcb_t  *timer_daemon_task = NULL;

static void timer_daemon(void *param);
static void timer_process_commands(void);
static void timer_process_expiry(void);

void timer_daemon_init(uint32_t stack_size, uint32_t priority)
{
    if (timer_daemon_initialized) return;
    cmd_queue = queue_create(sizeof(timer_cmd_t *), 32, "timer_cmd_q");
    timer_daemon_task = task_create(timer_daemon, "timer_daemon",
                                    stack_size, NULL, priority);
    timer_daemon_initialized = 1;
}

sw_timer_t *timer_create(const char *name, timer_callback_t cb, void *param,
                         uint32_t period_ticks, timer_type_t type)
{
    sw_timer_t *t;
    if (!cb || period_ticks == 0) return NULL;
    if (!timer_daemon_initialized) return NULL;
    t = (sw_timer_t *)malloc_rtos(sizeof(sw_timer_t));
    if (!t) return NULL;
    memset(t, 0, sizeof(sw_timer_t));
    t->id = next_timer_id++;
    t->type = type;
    t->callback = cb;
    t->param = param;
    t->period_ticks = period_ticks;
    t->expiry_tick = 0;
    t->active = 0;
    t->next = NULL;
    if (name) {
        strncpy(t->name, name, 15);
        t->name[15] = '\0';
    }
    task_enter_critical();
    {
        t->next = timer_list;
        timer_list = t;
    }
    task_exit_critical();
    return t;
}

static timer_cmd_t *timer_cmd_alloc(void)
{
    timer_cmd_t *cmd = (timer_cmd_t *)malloc_rtos(sizeof(timer_cmd_t));
    if (cmd) {
        memset(cmd, 0, sizeof(timer_cmd_t));
    }
    return cmd;
}

static void timer_cmd_free(timer_cmd_t *cmd)
{
    free_rtos(cmd);
}

static void timer_enqueue_cmd(timer_cmd_t *cmd)
{
    cmd->next = NULL;
    if (cmd_queue_tail) {
        cmd_queue_tail->next = cmd;
    } else {
        cmd_queue_head = cmd;
    }
    cmd_queue_tail = cmd;
}

static timer_cmd_t *timer_dequeue_cmd(void)
{
    timer_cmd_t *cmd = cmd_queue_head;
    if (cmd) {
        cmd_queue_head = cmd->next;
        if (!cmd_queue_head) {
            cmd_queue_tail = NULL;
        }
        cmd->next = NULL;
    }
    return cmd;
}

int32_t timer_start(sw_timer_t *t, uint32_t delay_ticks)
{
    timer_cmd_t *cmd;
    if (!t) return -1;
    cmd = timer_cmd_alloc();
    if (!cmd) return -1;
    cmd->type = TIMER_CMD_START;
    cmd->timer = t;
    cmd->period = delay_ticks;
    task_enter_critical();
    timer_enqueue_cmd(cmd);
    task_exit_critical();
    return 1;
}

int32_t timer_stop(sw_timer_t *t)
{
    timer_cmd_t *cmd;
    if (!t) return -1;
    cmd = timer_cmd_alloc();
    if (!cmd) return -1;
    cmd->type = TIMER_CMD_STOP;
    cmd->timer = t;
    cmd->period = 0;
    task_enter_critical();
    timer_enqueue_cmd(cmd);
    task_exit_critical();
    return 1;
}

int32_t timer_reset(sw_timer_t *t, uint32_t delay_ticks)
{
    timer_cmd_t *cmd;
    if (!t) return -1;
    cmd = timer_cmd_alloc();
    if (!cmd) return -1;
    cmd->type = TIMER_CMD_RESET;
    cmd->timer = t;
    cmd->period = delay_ticks;
    task_enter_critical();
    timer_enqueue_cmd(cmd);
    task_exit_critical();
    return 1;
}

int32_t timer_change_period(sw_timer_t *t, uint32_t new_period_ticks)
{
    timer_cmd_t *cmd;
    if (!t || new_period_ticks == 0) return -1;
    cmd = timer_cmd_alloc();
    if (!cmd) return -1;
    cmd->type = TIMER_CMD_CHANGE_PERIOD;
    cmd->timer = t;
    cmd->period = new_period_ticks;
    task_enter_critical();
    timer_enqueue_cmd(cmd);
    task_exit_critical();
    return 1;
}

void timer_delete(sw_timer_t *t)
{
    timer_cmd_t *cmd;
    if (!t) return;
    cmd = timer_cmd_alloc();
    if (!cmd) return;
    cmd->type = TIMER_CMD_DELETE;
    cmd->timer = t;
    cmd->period = 0;
    task_enter_critical();
    timer_enqueue_cmd(cmd);
    task_exit_critical();
}

int32_t timer_start_from_isr(sw_timer_t *t, uint32_t delay_ticks, int32_t *woken)
{
    timer_cmd_t *cmd;
    if (!t) return -1;
    cmd = timer_cmd_alloc();
    if (!cmd) return -1;
    cmd->type = TIMER_CMD_START;
    cmd->timer = t;
    cmd->period = delay_ticks;
    timer_enqueue_cmd(cmd);
    if (woken) *woken = 1;
    return 1;
}

int32_t timer_stop_from_isr(sw_timer_t *t, int32_t *woken)
{
    timer_cmd_t *cmd;
    if (!t) return -1;
    cmd = timer_cmd_alloc();
    if (!cmd) return -1;
    cmd->type = TIMER_CMD_STOP;
    cmd->timer = t;
    cmd->period = 0;
    timer_enqueue_cmd(cmd);
    if (woken) *woken = 1;
    return 1;
}

int32_t timer_reset_from_isr(sw_timer_t *t, uint32_t delay_ticks, int32_t *woken)
{
    timer_cmd_t *cmd;
    if (!t) return -1;
    cmd = timer_cmd_alloc();
    if (!cmd) return -1;
    cmd->type = TIMER_CMD_RESET;
    cmd->timer = t;
    cmd->period = delay_ticks;
    timer_enqueue_cmd(cmd);
    if (woken) *woken = 1;
    return 1;
}

uint32_t timer_is_active(const sw_timer_t *t)
{
    if (!t) return 0;
    return t->active;
}

uint32_t timer_get_id(const sw_timer_t *t)
{
    if (!t) return 0;
    return t->id;
}

uint32_t timer_get_remaining(const sw_timer_t *t)
{
    uint32_t now;
    if (!t || !t->active) return 0;
    now = task_get_tick_count();
    if (t->expiry_tick > now) {
        return t->expiry_tick - now;
    }
    return 0;
}

static void timer_process_commands(void)
{
    timer_cmd_t *cmd;
    while ((cmd = timer_dequeue_cmd()) != NULL) {
        sw_timer_t *t = cmd->timer;
        if (!t) {
            timer_cmd_free(cmd);
            continue;
        }
        switch (cmd->type) {
        case TIMER_CMD_START:
            t->active = 1;
            t->expiry_tick = task_get_tick_count() + cmd->period;
            break;
        case TIMER_CMD_STOP:
            t->active = 0;
            t->expiry_tick = 0;
            break;
        case TIMER_CMD_RESET:
            t->active = 1;
            t->expiry_tick = task_get_tick_count() + cmd->period;
            break;
        case TIMER_CMD_CHANGE_PERIOD:
            t->period_ticks = cmd->period;
            if (t->active) {
                t->expiry_tick = task_get_tick_count() + t->period_ticks;
            }
            break;
        case TIMER_CMD_DELETE: {
            sw_timer_t **list = &timer_list;
            t->active = 0;
            while (*list) {
                if (*list == t) {
                    *list = t->next;
                    break;
                }
                list = &(*list)->next;
            }
            timer_cmd_free(cmd);
            free_rtos(t);
            continue;
        }
        default:
            break;
        }
        timer_cmd_free(cmd);
    }
}

static void timer_process_expiry(void)
{
    uint32_t now = task_get_tick_count();
    sw_timer_t *t = timer_list;
    while (t) {
        sw_timer_t *next_timer = t->next;
        if (t->active && t->expiry_tick <= now) {
            if (t->callback) {
                t->callback(t->param);
            }
            if (t->type == TIMER_TYPE_AUTO_RELOAD) {
                t->expiry_tick = now + t->period_ticks;
            } else {
                t->active = 0;
                t->expiry_tick = 0;
            }
        }
        t = next_timer;
    }
}

static void timer_daemon(void *param)
{
    (void)param;
    while (1) {
        timer_process_commands();
        timer_process_expiry();
        task_delay(1);
    }
}

/*
 * L6: Timer chain for sequential delayed operations.
 *
 * Chains multiple timer callbacks to execute in sequence with a
 * fixed delay between each. Useful for multi-stage initialization
 * or power-up sequencing.
 *
 * L5 Algorithm: Linked-list of timers where each callback starts
 * the next timer in the chain.
 */
typedef struct timer_chain_node {
    sw_timer_t         *timer;
    struct timer_chain_node *next;
} timer_chain_node_t;

static timer_chain_node_t *timer_chain_head = NULL;

static void timer_chain_callback(void *param)
{
    timer_chain_node_t *node = (timer_chain_node_t *)param;
    if (!node) return;
    /* Remove from chain */
    timer_chain_head = node->next;
    /* Start next timer if exists */
    if (node->next && node->next->timer) {
        timer_start(node->next->timer, 0);
    }
    free_rtos(node);
}

/*
 * L6: Create and schedule a chain of timers.
 *
 * times: array of delay values between each step.
 * callbacks: array of callback functions (one per step).
 * params: array of callback parameters.
 * count: number of steps in the chain.
 *
 * Returns 1 on success, 0 on allocation failure.
 */
int32_t timer_chain_create(uint32_t *times, timer_callback_t *callbacks,
                            void **params, uint32_t count)
{
    uint32_t i;
    timer_chain_node_t *prev = NULL, *node;

    if (!times || !callbacks || count == 0) return 0;

    /* Create nodes in reverse order so first node is head */
    for (i = 0; i < count; i++) {
        node = (timer_chain_node_t *)malloc_rtos(sizeof(timer_chain_node_t));
        if (!node) return 0;
        node->timer = NULL;
        node->next = prev;
        prev = node;
    }

    timer_chain_head = prev;

    /* Create timers for each node */
    {
        timer_chain_node_t *curr = timer_chain_head;
        i = 0;
        while (curr && i < count) {
            curr->timer = timer_create("chain_tmr",
                           (curr->next || i == count - 1) ? callbacks[i] : timer_chain_callback,
                           curr->next ? curr : params[i],
                           times[i], TIMER_TYPE_ONE_SHOT);
            if (!curr->timer) {
                /* Cleanup on failure */
                while (timer_chain_head) {
                    timer_chain_node_t *tmp = timer_chain_head;
                    timer_chain_head = timer_chain_head->next;
                    if (tmp->timer) timer_delete(tmp->timer);
                    free_rtos(tmp);
                }
                return 0;
            }
            curr = curr->next;
            i++;
        }
    }

    /* Start the first timer */
    if (timer_chain_head && timer_chain_head->timer) {
        timer_start(timer_chain_head->timer, 0);
    }

    return 1;
}

/*
 * L3: Timer pool — pre-allocated timer array for fast allocation.
 *
 * Avoids heap fragmentation by pre-allocating a fixed pool of timers.
 * Used in hard-real-time systems where dynamic allocation is forbidden
 * after initialization.
 *
 * L4 Design Principle: Static allocation for WCET determinism (ISO 26262).
 */

#define TIMER_POOL_SIZE 16

static sw_timer_t  timer_pool_buf[TIMER_POOL_SIZE];
static uint32_t    timer_pool_free_mask = 0xFFFFU; /* bit=1 means free */

void timer_pool_init(void)
{
    memset(timer_pool_buf, 0, sizeof(timer_pool_buf));
    timer_pool_free_mask = (1U << TIMER_POOL_SIZE) - 1;
}

/*
 * L5: Allocate a timer from the static pool (O(1) with ffs).
 * Returns NULL if pool exhausted.
 */
sw_timer_t *timer_pool_alloc(void)
{
    uint32_t i;
    for (i = 0; i < TIMER_POOL_SIZE; i++) {
        if (timer_pool_free_mask & (1U << i)) {
            timer_pool_free_mask &= ~(1U << i);
            memset(&timer_pool_buf[i], 0, sizeof(sw_timer_t));
            timer_pool_buf[i].id = i + 1000; /* offset pool IDs */
            return &timer_pool_buf[i];
        }
    }
    return NULL;
}

/*
 * L5: Return a timer to the static pool.
 */
void timer_pool_free(sw_timer_t *t)
{
    if (!t) return;
    if (t >= timer_pool_buf &&
        t < timer_pool_buf + TIMER_POOL_SIZE) {
        uint32_t idx = (uint32_t)(t - timer_pool_buf);
        timer_pool_free_mask |= (1U << idx);
        t->active = 0;
        t->callback = NULL;
    }
}
