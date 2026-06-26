#include "task_scheduler.h"
#include "ipc_queue.h"

typedef struct {
    uint32_t sensor_id;
    uint32_t value;
    uint32_t timestamp;
} sensor_data_t;

static queue_t *sensor_queue = NULL;

static void task_sensor(void *param)
{
    uint32_t sensor_id = (uint32_t)(uintptr_t)param;
    uint32_t fake_value = 0;
    while (1) {
        sensor_data_t data;
        data.sensor_id = sensor_id;
        data.value = fake_value++;
        data.timestamp = task_get_tick_count();
        queue_send(sensor_queue, &data, 100);
        task_delay(100);
    }
}

static void task_aggregator(void *param)
{
    (void)param;
    while (1) {
        sensor_data_t data;
        if (queue_receive(sensor_queue, &data, 200) == 1) {
            (void)data.sensor_id;
            (void)data.value;
            (void)data.timestamp;
        }
    }
}

static void task_commander(void *param)
{
    (void)param;
    while (1) {
        uint32_t waiting = queue_messages_waiting(sensor_queue);
        if (waiting > 10) {
            queue_reset(sensor_queue);
        }
        task_delay(1000);
    }
}

int main(void)
{
    scheduler_init();

    sensor_queue = queue_create(sizeof(sensor_data_t), 16, "sensor_q");
    queue_registry_add(sensor_queue);

    task_create(task_sensor, "sensor0", 512, (void *)0, 3);
    task_create(task_sensor, "sensor1", 512, (void *)1, 3);
    task_create(task_aggregator, "aggreg", 512, NULL, 2);
    task_create(task_commander, "cmd", 256, NULL, 1);

    scheduler_start();
    return 0;
}
