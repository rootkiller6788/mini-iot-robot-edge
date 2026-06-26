#include "task_scheduler.h"

#define LED_PORT    ((volatile uint32_t *)0x40020000)
#define LED_PIN     13

static void led_on(void)
{
    *LED_PORT |= (1U << LED_PIN);
}

static void led_off(void)
{
    *LED_PORT &= ~(1U << LED_PIN);
}

static void led_toggle(void)
{
    *LED_PORT ^= (1U << LED_PIN);
}

static void task_led_blink(void *param)
{
    uint32_t delay_ms = (uint32_t)(uintptr_t)param;
    while (1) {
        led_toggle();
        task_delay(delay_ms);
    }
}

static void task_heartbeat(void *param)
{
    (void)param;
    while (1) {
        led_on();
        task_delay(50);
        led_off();
        task_delay(950);
    }
}

int main(void)
{
    scheduler_init();

    task_create(task_led_blink, "blink", 256, (void *)500, 2);
    task_create(task_heartbeat, "heart", 256, NULL, 1);

    scheduler_start();
    return 0;
}
