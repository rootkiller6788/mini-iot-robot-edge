#include "task_scheduler.h"
#include "ipc_queue.h"
#include "semaphore_mutex.h"
#include "software_timer.h"
#include "memory_heap.h"

#define PRODUCER_COUNT  4
#define CONSUMER_COUNT  3
#define BUFFER_SIZE     32
#define ITEM_COUNT      200

typedef struct {
    uint32_t id;
    uint32_t production_id;
    uint32_t timestamp;
    uint32_t checksum;
    uint8_t  payload[16];
} data_item_t;

typedef struct {
    uint32_t producer_id;
    uint32_t items_produced;
    uint32_t total_value;
} producer_stats_t;

typedef struct {
    uint32_t consumer_id;
    uint32_t items_consumed;
    uint32_t total_value;
    uint32_t last_item_id;
} consumer_stats_t;

static queue_t      *data_queue = NULL;
static semaphore_t  *empty_slots = NULL;
static semaphore_t  *filled_slots = NULL;
static mutex_t      *stats_mutex = NULL;
static mutex_t      *pipe_mutex = NULL;
static producer_stats_t prod_stats[PRODUCER_COUNT];
static consumer_stats_t cons_stats[CONSUMER_COUNT];
static volatile uint32_t system_running = 1;
static volatile uint32_t overflow_count = 0;
static volatile uint32_t underflow_count = 0;
static volatile uint32_t total_produced = 0;
static volatile uint32_t total_consumed = 0;
static volatile uint32_t checksum_errors = 0;
static volatile uint32_t peak_queue_depth = 0;
static volatile uint32_t alloc_failures = 0;

static uint32_t compute_checksum(const data_item_t *item)
{
    uint32_t sum;
    uint32_t i;
    const uint8_t *p = (const uint8_t *)item;
    sum = item->id ^ item->production_id ^ item->timestamp;
    for (i = 0; i < sizeof(item->payload); i++) {
        sum += p[offsetof(data_item_t, payload) + i] * (i + 1);
    }
    return sum;
}

static void produce_item(data_item_t *item, uint32_t producer_id, uint32_t prod_num)
{
    uint32_t i;
    item->id = (producer_id << 24) | (prod_num & 0x00FFFFFFU);
    item->production_id = prod_num;
    item->timestamp = task_get_tick_count();
    for (i = 0; i < sizeof(item->payload); i++) {
        item->payload[i] = (uint8_t)((producer_id * 37 + prod_num + i) & 0xFFU);
    }
    item->checksum = compute_checksum(item);
}

static int32_t validate_item(const data_item_t *item)
{
    uint32_t expected = compute_checksum(item);
    return expected == item->checksum ? 1 : 0;
}

static void update_peak_queue_depth(void)
{
    uint32_t depth = queue_messages_waiting(data_queue);
    if (depth > peak_queue_depth) {
        peak_queue_depth = depth;
    }
}

static void task_producer(void *param)
{
    uint32_t prod_id = (uint32_t)(uintptr_t)param;
    uint32_t items_to_produce = ITEM_COUNT / PRODUCER_COUNT;
    uint32_t i, prod_num = 0;
    producer_stats_t *stats = &prod_stats[prod_id];
    stats->producer_id = prod_id;
    stats->items_produced = 0;
    stats->total_value = 0;
    for (i = 0; i < items_to_produce; i++) {
        data_item_t item;
        produce_item(&item, prod_id, prod_num);
        if (semaphore_take(empty_slots, 500) != 1) {
            overflow_count++;
            task_delay(10);
            continue;
        }
        mutex_lock(pipe_mutex, 100);
        if (queue_send(data_queue, &item, 100) == 1) {
            semaphore_give(filled_slots);
            prod_num++;
            stats->items_produced++;
            stats->total_value += item.checksum;
            total_produced++;
            update_peak_queue_depth();
        } else {
            semaphore_give(empty_slots);
            overflow_count++;
        }
        mutex_unlock(pipe_mutex);
        task_delay(5 + (prod_id * 3));
    }
    mutex_lock(stats_mutex, 100);
    (void)prod_num;
    mutex_unlock(stats_mutex);
    while (system_running) {
        task_delay(1000);
    }
}

static void task_consumer(void *param)
{
    uint32_t cons_id = (uint32_t)(uintptr_t)param;
    consumer_stats_t *stats = &cons_stats[cons_id];
    stats->consumer_id = cons_id;
    stats->items_consumed = 0;
    stats->total_value = 0;
    stats->last_item_id = 0;
    while (system_running) {
        data_item_t item;
        if (semaphore_take(filled_slots, 300) != 1) {
            underflow_count++;
            task_delay(20);
            continue;
        }
        mutex_lock(pipe_mutex, 100);
        if (queue_receive(data_queue, &item, 100) == 1) {
            semaphore_give(empty_slots);
            if (validate_item(&item)) {
                stats->items_consumed++;
                stats->total_value += item.checksum;
                stats->last_item_id = item.production_id;
                total_consumed++;
            } else {
                checksum_errors++;
            }
        } else {
            semaphore_give(filled_slots);
            underflow_count++;
        }
        mutex_unlock(pipe_mutex);
        task_delay(8 + (cons_id * 2));
    }
}

static void task_statistics_reporter(void *param)
{
    (void)param;
    while (system_running) {
        mutex_lock(stats_mutex, 100);
        {
            uint32_t i;
            uint32_t total_p = 0, total_c = 0;
            heap_stats_t heap;
            for (i = 0; i < PRODUCER_COUNT; i++) {
                total_p += prod_stats[i].items_produced;
            }
            for (i = 0; i < CONSUMER_COUNT; i++) {
                total_c += cons_stats[i].items_consumed;
            }
            heap = heap_get_stats();
            (void)total_p;
            (void)total_c;
            (void)overflow_count;
            (void)underflow_count;
            (void)checksum_errors;
            (void)queue_messages_waiting(data_queue);
            (void)peak_queue_depth;
            (void)heap.free_size;
            (void)heap.min_free_ever;
        }
        mutex_unlock(stats_mutex);
        task_delay(2000);
    }
}

static void task_memory_monitor(void *param)
{
    (void)param;
    while (system_running) {
        uint32_t free_size = heap_get_free_size();
        uint32_t min_free = heap_get_min_free();
        if (free_size < 1024) {
            alloc_failures++;
        }
        (void)min_free;
        task_delay(5000);
    }
}

static void timer_health_check(void *param)
{
    (void)param;
    if (total_produced > 0 && total_consumed > 0) {
        if (total_consumed < total_produced) {
            (void)(total_produced - total_consumed);
        }
    }
    if (checksum_errors > 0) {
        (void)checksum_errors;
    }
}

static void timer_resource_report(void *param)
{
    (void)param;
    {
        uint32_t empty = semaphore_get_count(empty_slots);
        uint32_t filled = semaphore_get_count(filled_slots);
        (void)empty;
        (void)filled;
    }
    {
        heap_stats_t heap = heap_get_stats();
        (void)heap.allocated_size;
        (void)heap.alloc_count;
        (void)heap.free_count;
    }
}

static void task_watchdog(void *param)
{
    (void)param;
    while (1) {
        task_delay(10000);
        if (total_produced >= ITEM_COUNT) {
            system_running = 0;
        }
    }
}

static void task_fast_burst_producer(void *param)
{
    uint32_t id = (uint32_t)(uintptr_t)param;
    (void)id;
    while (system_running) {
        uint32_t i;
        for (i = 0; i < 5; i++) {
            data_item_t item;
            produce_item(&item, 99, i);
            if (semaphore_take(empty_slots, 50) == 1) {
                mutex_lock(pipe_mutex, 50);
                if (queue_send(data_queue, &item, 0) == 1) {
                    semaphore_give(filled_slots);
                    total_produced++;
                    update_peak_queue_depth();
                } else {
                    semaphore_give(empty_slots);
                }
                mutex_unlock(pipe_mutex);
            }
        }
        task_delay(1000);
    }
}

int main(void)
{
    uint32_t i;
    sw_timer_t *health_timer;
    sw_timer_t *resource_timer;

    scheduler_init();
    timer_daemon_init(512, 5);

    data_queue = queue_create(sizeof(data_item_t), BUFFER_SIZE, "data_q");
    empty_slots = semaphore_create_counting(BUFFER_SIZE, BUFFER_SIZE, "empty");
    filled_slots = semaphore_create_counting(BUFFER_SIZE, 0, "filled");
    stats_mutex = mutex_create("stats_mtx");
    pipe_mutex = mutex_create("pipe_mtx");

    health_timer = timer_create("health", timer_health_check, NULL,
                                5000, TIMER_TYPE_AUTO_RELOAD);
    timer_start(health_timer, 5000);

    resource_timer = timer_create("res_rep", timer_resource_report, NULL,
                                  10000, TIMER_TYPE_AUTO_RELOAD);
    timer_start(resource_timer, 10000);

    for (i = 0; i < PRODUCER_COUNT; i++) {
        char name[16] = {'p','r','o','d','_','0' + (char)i,'\0'};
        task_create(task_producer, name, 768, (void *)(uintptr_t)i, 3);
    }

    for (i = 0; i < CONSUMER_COUNT; i++) {
        char name[16] = {'c','o','n','s','_','0' + (char)i,'\0'};
        task_create(task_consumer, name, 768, (void *)(uintptr_t)i, 2);
    }

    task_create(task_statistics_reporter, "stats", 512, NULL, 1);
    task_create(task_memory_monitor, "mem_mon", 384, NULL, 1);
    task_create(task_watchdog, "wdog", 256, NULL, 4);
    task_create(task_fast_burst_producer, "burst", 384, (void *)99, 3);

    scheduler_start();
    return 0;
}
