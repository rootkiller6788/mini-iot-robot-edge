#include "task_scheduler.h"
#include "semaphore_mutex.h"
#include "ipc_queue.h"

static semaphore_t *uart_sem = NULL;
static mutex_t     *print_mutex = NULL;
static semaphore_t *resource_pool = NULL;

static void task_producer(void *param)
{
    uint32_t id = (uint32_t)(uintptr_t)param;
    while (1) {
        if (semaphore_take(resource_pool, 500) == 1) {
            mutex_lock(print_mutex, 100);
            (void)id;
            mutex_unlock(print_mutex);
            task_delay(200);
            semaphore_give(resource_pool);
        }
        task_delay(50);
    }
}

static void task_consumer(void *param)
{
    uint32_t id = (uint32_t)(uintptr_t)param;
    while (1) {
        if (semaphore_take(resource_pool, 1000) == 1) {
            mutex_lock(print_mutex, 100);
            (void)id;
            mutex_unlock(print_mutex);
            task_delay(100);
            semaphore_give(resource_pool);
        }
    }
}

static void task_uart_writer(void *param)
{
    (void)param;
    while (1) {
        semaphore_take(uart_sem, 0xFFFFFFFFU);
        mutex_lock(print_mutex, 0xFFFFFFFFU);
        mutex_unlock(print_mutex);
    }
}

static void gatekeeper_handler(void *param)
{
    uint32_t *val = (uint32_t *)param;
    *val += 1;
}

static void task_gatekeeper_client(void *param)
{
    mutex_t *gk = (mutex_t *)param;
    while (1) {
        uint32_t shared = 0;
        gatekeeper_call(gk, gatekeeper_handler, &shared, 100);
        task_delay(500);
    }
}

int main(void)
{
    mutex_t *gatekeeper_mutex;
    scheduler_init();

    uart_sem = semaphore_create_binary("uart_sem");
    print_mutex = mutex_create("print_mtx");
    resource_pool = semaphore_create_counting(3, 3, "res_pool");
    gatekeeper_mutex = mutex_create("gatekeeper");

    task_create(task_producer, "prod0", 256, (void *)0, 2);
    task_create(task_producer, "prod1", 256, (void *)1, 2);
    task_create(task_consumer, "cons0", 256, (void *)0, 1);
    task_create(task_consumer, "cons1", 256, (void *)1, 1);
    task_create(task_uart_writer, "uart", 256, NULL, 3);
    task_create(task_gatekeeper_client, "gk_cli", 256, gatekeeper_mutex, 1);

    scheduler_start();
    return 0;
}
