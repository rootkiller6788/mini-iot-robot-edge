/*
 * event_groups.h — Lightweight Event Groups for mini-rtos
 *
 * L2 Core Concept: Event-driven task synchronization using bit-waiting.
 * Tasks can pend on a combination of event bits with AND/OR semantics.
 *
 * Engineering Model: FreeRTOS EventBits_t (32-bit word used as bitmask).
 * Each bit represents an independent binary event.
 *
 * L3 Engineering Structure:
 *   - 32-bit event word with atomic bit-set/clear.
 *   - Blocked-task list ordered by priority.
 *   - Conditional unblock evaluation at every bit-set.
 *
 * L4 Formal Basis:
 *   Event groups implement a subset of CSP (Communicating Sequential Processes)
 *   event coordination, where each bit corresponds to a channel-event.
 *
 * Course Mapping: CMU 15-410 Synchronization Primitives, MIT 6.004 Signals
 */
#ifndef EVENT_GROUPS_H
#define EVENT_GROUPS_H

#include <stdint.h>
#include "task_scheduler.h"

typedef uint32_t event_bits_t;

/*
 * L3: Wait condition enum — determines how event bits are evaluated.
 * AND_ALL: task unblocks when ALL requested bits are set.
 * OR_ANY:  task unblocks when ANY requested bit is set.
 * OR_ANY_CLEAR: like OR_ANY but clears bits on exit (auto-reset).
 */
typedef enum {
    EVENT_WAIT_AND_ALL = 0,
    EVENT_WAIT_OR_ANY  = 1,
    EVENT_WAIT_OR_ANY_CLEAR = 2
} event_wait_type_t;

/*
 * L1: Core Definition — Event Group structure.
 *
 * An event group holds a 32-bit word where each bit is an independent
 * event flag. Tasks can set bits, clear bits, and wait for bit combinations.
 *
 * bits: current state of all 32 event flags.
 * blocked_tasks: singly-linked list of tasks blocked on this group.
 * wait_bits / wait_type: stored per-blocked-task via block_obj cast.
 */
typedef struct event_group {
    event_bits_t bits;
    struct tcb  *blocked_tasks;
    char         name[16];
} event_group_t;

/*
 * L1: API Declarations
 *
 * event_group_create        — allocate and initialize an event group.
 * event_group_delete        — free event group, unblock all waiters.
 * event_group_set_bits      — atomically set bits (task context).
 * event_group_set_bits_from_isr — ISR-safe bit set with yield flag.
 * event_group_clear_bits    — atomically clear bits.
 * event_group_wait_bits     — block until bits match condition with timeout.
 *                             Returns the event-bits value at unblock time.
 * event_group_get_bits      — read current bits without blocking.
 * event_group_sync          — atomically set some bits and wait for others.
 *                             (rendezvous: used for multi-task barrier sync)
 */
event_group_t *event_group_create(const char *name);
void           event_group_delete(event_group_t *eg);

event_bits_t   event_group_set_bits(event_group_t *eg, event_bits_t bits_to_set);
event_bits_t   event_group_set_bits_from_isr(event_group_t *eg,
                                              event_bits_t bits_to_set,
                                              int32_t *woken);
event_bits_t   event_group_clear_bits(event_group_t *eg,
                                       event_bits_t bits_to_clear);

event_bits_t   event_group_wait_bits(event_group_t *eg,
                                      event_bits_t bits_to_wait,
                                      event_wait_type_t wait_type,
                                      uint32_t timeout_ticks,
                                      uint32_t clear_on_exit);
event_bits_t   event_group_get_bits(const event_group_t *eg);

event_bits_t   event_group_sync(event_group_t *eg,
                                 event_bits_t bits_to_set,
                                 event_bits_t bits_to_wait,
                                 uint32_t timeout_ticks);

#endif /* EVENT_GROUPS_H */
