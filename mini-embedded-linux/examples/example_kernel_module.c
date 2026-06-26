#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kernel_module.h"

typedef struct {
    int value;
    char name[64];
} sensor_data_t;

static int sensor_read_proc(char *buf, int maxlen, void *data)
{
    sensor_data_t *sd = (sensor_data_t *)data;
    return snprintf(buf, (size_t)maxlen, "sensor: %s = %d\n", sd->name, sd->value);
}

static int sensor_write_proc(const char *buf, int len, void *data)
{
    sensor_data_t *sd = (sensor_data_t *)data;
    (void)len;
    if (buf) sd->value = atoi(buf);
    return 0;
}

static void my_tasklet_handler(unsigned long data)
{
    printf("  [TASKLET] executing with data=%lu\n", data);
}

static void my_work_handler(void *data)
{
    const char *msg = (const char *)data;
    printf("  [WORKQUEUE] executing: %s\n", msg);
}

static int my_device_open(void *data)
{
    (void)data;
    printf("  [DEV] device opened\n");
    return 0;
}

static int my_device_read(void *data, char *buf, int len)
{
    (void)data;
    const char *resp = "hello from kernel device";
    int n = (int)strlen(resp);
    if (n > len) n = len;
    if (buf) memcpy(buf, resp, (size_t)n);
    return n;
}

int main(void)
{
    printf("=== Linux Kernel Module Example ===\n\n");

    km_subsystem_t ks;
    km_subsystem_init(&ks);

    km_module_t mod_usb;
    km_module_init(&mod_usb, "usb_core", "2.0");
    km_module_set_author(&mod_usb, "Kernel Team");
    km_module_set_description(&mod_usb, "USB core driver subsystem");
    km_module_set_license(&mod_usb, KM_LICENSE_GPL);
    km_module_param_add(&mod_usb, "debug", KM_PARAM_INT, "Enable debug output");
    km_module_param_set_int(&mod_usb, "debug", 1);
    km_module_load(&ks, &mod_usb);

    km_module_t mod_storage;
    km_module_init(&mod_storage, "usb_storage", "1.0");
    km_module_set_license(&mod_storage, KM_LICENSE_GPL_V2);
    km_module_add_dependency(&mod_storage, "usb_core");
    km_module_param_add(&mod_storage, "delay_use", KM_PARAM_INT, "Initial delay");
    km_module_param_set_int(&mod_storage, "delay_use", 5);
    printf("Dependency check: %s\n",
           km_module_check_dependencies(&ks, &mod_storage) == 0 ? "OK" : "FAIL");
    km_module_load(&ks, &mod_storage);

    km_module_t mod_gpio;
    km_module_init(&mod_gpio, "gpio_keys", "1.1");
    km_module_set_author(&mod_gpio, "Embedded Dev");
    km_module_set_description(&mod_gpio, "GPIO key input driver");
    km_module_load(&ks, &mod_gpio);

    printf("\n--- /proc/modules view ---\n");
    km_module_list(&ks);

    printf("\n--- Proc FS example ---\n");
    sensor_data_t sd = {42, "temperature"};
    km_proc_create(&ks, "temperature", "driver/sensor", 0644, &sd,
                   sensor_read_proc, sensor_write_proc);
    char rbuf[256];
    km_proc_read(&ks, "temperature", rbuf, sizeof(rbuf));
    printf("  /proc/driver/sensor/temperature: %s", rbuf);

    printf("\n--- Tasklet / Workqueue example ---\n");
    km_tasklet_create(&ks, my_tasklet_handler, 100);
    km_tasklet_schedule(&ks, 0);
    km_tasklet_create(&ks, my_tasklet_handler, 200);
    km_tasklet_schedule(&ks, 1);

    km_workqueue_create(&ks, my_work_handler, "process_sensor_data");
    km_workqueue_queue(&ks, 0);
    km_workqueue_create(&ks, my_work_handler, "update_display");
    km_workqueue_queue(&ks, 1);

    printf("\n--- Device file creation ---\n");
    km_device_create(&ks, "my_sensor", KM_DEVTYPE_CHAR, 240, 0, 0666, NULL,
                     my_device_open, NULL, my_device_read, NULL);
    km_device_open(&ks, "my_sensor");
    char devbuf[64];
    int n = km_device_read(&ks, "my_sensor", devbuf, sizeof(devbuf));
    printf("  Read %d bytes: %.*s\n", n, n, devbuf);
    km_device_create(&ks, "my_led", KM_DEVTYPE_CHAR, 241, 0, 0666, NULL,
                     my_device_open, NULL, NULL, NULL);

    printf("\n--- Module parameters ---\n");
    int val;
    km_module_param_get_int(&mod_usb, "debug", &val);
    printf("  usb_core.debug = %d\n", val);
    km_module_param_get_int(&mod_storage, "delay_use", &val);
    printf("  usb_storage.delay_use = %d\n", val);

    printf("\n");
    km_subsystem_dump(&ks);

    printf("\n--- Deferred work demo ---\n");
    km_workqueue_create(&ks, my_work_handler, "cleanup_task");
    km_workqueue_cancel(&ks, 2);
    printf("  Workqueue[2] cancelled\n");
    km_tasklet_kill(&ks, 0);
    printf("  Tasklet[0] killed\n");

    printf("\n--- Unloading modules ---\n");
    km_module_unload(&ks, &mod_gpio);
    km_module_unload(&ks, &mod_storage);
    km_module_unload(&ks, &mod_usb);

    printf("\nExample complete.\n");
    return 0;
}
