#include "task_scheduler.h"
#include "semaphore_mutex.h"
#include "ipc_queue.h"
#include "software_timer.h"
#include "portable.h"

static volatile uint32_t shared_counter = 0;
static mutex_t *counter_mutex = NULL;
static volatile uint32_t high_prio_activated = 0;
static volatile uint32_t med_prio_running = 0;
static volatile uint32_t low_prio_blocked = 0;
static volatile uint32_t priority_inherited = 0;
static volatile uint32_t priority_restored = 0;
static volatile uint32_t total_context_switches = 0;
static volatile uint32_t deadlock_detected = 0;

typedef struct {
    uint32_t task_id;
    uint32_t burst_count;
} workload_t;

static void simulate_cpu_work(uint32_t cycles)
{
    volatile uint32_t i;
    for (i = 0; i < cycles; i++) {
        __asm volatile("nop");
    }
}

static void task_high_priority(void *param)
{
    uint32_t id = (uint32_t)(uintptr_t)param;
    while (1) {
        high_prio_activated = 1;
        {
            int32_t result = mutex_lock(counter_mutex, 0);
            if (result != 1) {
                high_prio_activated = 0;
                task_delay(10);
                continue;
            }
        }
        if (mutex_get_owner(counter_mutex) == task_get_current()) {
            shared_counter += 10;
            priority_inherited = 1;
            simulate_cpu_work(100);
            (void)id;
            mutex_unlock(counter_mutex);
            priority_restored = 1;
        }
        high_prio_activated = 0;
        task_delay(50);
    }
}

static void task_medium_priority(void *param)
{
    workload_t *w = (workload_t *)param;
    while (1) {
        med_prio_running = 1;
        simulate_cpu_work(w->burst_count);
        med_prio_running = 0;
        total_context_switches++;
        task_delay(w->burst_count / 1000);
    }
}

static void task_low_priority(void *param)
{
    uint32_t id = (uint32_t)(uintptr_t)param;
    while (1) {
        mutex_lock(counter_mutex, 0xFFFFFFFFU);
        low_prio_blocked = 1;
        {
            tcb_t *self = task_get_current();
            uint32_t orig_prio = self->priority;
            simulate_cpu_work(50000);
            shared_counter++;
            if (self->priority != orig_prio) {
                priority_inherited = 1;
            }
        }
        low_prio_blocked = 0;
        (void)id;
        mutex_unlock(counter_mutex);
        total_context_switches++;
        task_delay(200);
    }
}

static void task_monitor(void *param)
{
    uint32_t last_counter = 0;
    uint32_t samples = 0;
    (void)param;
    while (1) {
        if (shared_counter != last_counter) {
            last_counter = shared_counter;
            samples++;
        }
        if (high_prio_activated && low_prio_blocked && priority_inherited) {
            priority_inherited = 0;
        }
        if (med_prio_running && low_prio_blocked) {
            (void)low_prio_blocked;
        }
        if (samples > 100) {
            samples = 0;
            if (shared_counter < 1) {
                deadlock_detected = 1;
            }
        }
        task_delay(100);
    }
}

static void task_auxiliary_worker(void *param)
{
    uint32_t id = (uint32_t)(uintptr_t)param;
    while (1) {
        simulate_cpu_work(1000 * (id + 1));
        task_delay(30 + id * 20);
    }
}

static void timer_stats_callback(void *param)
{
    (void)param;
    (void)shared_counter;
    (void)total_context_switches;
    (void)deadlock_detected;
}

static void task_periodic_worker(void *param)
{
    uint32_t last_wake = task_get_tick_count();
    uint32_t id = (uint32_t)(uintptr_t)param;
    while (1) {
        if (mutex_lock(counter_mutex, 100) == 1) {
            shared_counter += (id + 1);
            mutex_unlock(counter_mutex);
        }
        task_delay_until(&last_wake, 100);
    }
}

static void task_watchdog(void *param)
{
    uint32_t last_total;
    (void)param;
    last_total = shared_counter;
    while (1) {
        task_delay(5000);
        if (shared_counter == last_total) {
            deadlock_detected = 1;
        }
        last_total = shared_counter;
    }
}

int main(void)
{
    static workload_t med_workload = { .task_id = 1, .burst_count = 50000 };
    static workload_t med2_workload = { .task_id = 2, .burst_count = 30000 };
    sw_timer_t *stats_timer;

    scheduler_init();
    timer_daemon_init(512, 4);
    counter_mutex = mutex_create("counter_mtx");

    task_create(task_low_priority, "low", 512, (void *)1, 1);
    task_create(task_medium_priority, "med0", 512, &med_workload, 2);
    task_create(task_medium_priority, "med1", 512, &med2_workload, 2);
    task_create(task_high_priority, "high", 512, (void *)3, 3);
    task_create(task_monitor, "mon", 512, NULL, 1);
    task_create(task_periodic_worker, "pwork0", 256, (void *)0, 2);
    task_create(task_periodic_worker, "pwork1", 256, (void *)1, 2);
    task_create(task_auxiliary_worker, "aux0", 256, (void *)0, 1);
    task_create(task_auxiliary_worker, "aux1", 256, (void *)1, 1);
    task_create(task_watchdog, "wdog", 256, NULL, 4);

    stats_timer = timer_create("stats_tmr", timer_stats_callback,
                               NULL, 1000, TIMER_TYPE_AUTO_RELOAD);
    timer_start(stats_timer, 1000);

    scheduler_start();
    return 0;
}
