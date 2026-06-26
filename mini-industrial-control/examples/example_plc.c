#include "plc_ladder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    plc_program_t plc;
    plc_init(&plc);
    plc.running = true;

    printf("=== PLC Ladder Logic Demo ===\n\n");

    /* Rung 0: Start/Stop with latch */
    plc_rung_t *r0 = plc_add_rung(&plc);
    plc_add_contact(r0, "Start", PLC_CONTACT_NO, 0);
    plc_add_contact(r0, "Stop", PLC_CONTACT_NC, 1);
    plc_add_coil(r0, "Motor", PLC_COIL_LATCH_SET, 100);
    printf("[Rung 0] Start/Stop latch: contacts=NO(start), NC(stop) -> coil=set(motor)\n");

    /* Rung 1: Stop resets the latch */
    plc_rung_t *r1 = plc_add_rung(&plc);
    plc_add_contact(r1, "Stop", PLC_CONTACT_NO, 1);
    plc_add_coil(r1, "Motor_reset", PLC_COIL_LATCH_RESET, 100);
    printf("[Rung 1] Stop reset: contact=NO(stop) -> coil=reset(motor)\n");

    /* Rung 2: On-delay timer */
    plc_rung_t *r2 = plc_add_rung(&plc);
    plc_add_contact(r2, "Motor", PLC_CONTACT_NO, 100);
    plc_add_timer(r2, "T_DelayStart", PLC_TIMER_TON, 3000);
    plc_add_coil(r2, "AuxReady", PLC_COIL_OUTPUT, 200);
    printf("[Rung 2] TON timer: contact=NO(motor) -> timer(TON, 3s) -> coil(aux_ready)\n");

    /* Rung 3: Off-delay timer */
    plc_rung_t *r3 = plc_add_rung(&plc);
    plc_add_contact(r3, "Stop", PLC_CONTACT_NO, 1);
    plc_add_timer(r3, "T_DelayOff", PLC_TIMER_TOF, 2000);
    plc_add_coil(r3, "AuxDelay", PLC_COIL_OUTPUT, 201);
    printf("[Rung 3] TOF timer: contact=NO(stop) -> timer(TOF, 2s) -> coil(aux_delay)\n");

    /* Rung 4: Counter */
    plc_rung_t *r4 = plc_add_rung(&plc);
    plc_add_contact(r4, "Sensor", PLC_CONTACT_NO, 2);
    plc_add_counter(r4, "C_Parts", PLC_COUNTER_CTU, 10);
    plc_add_coil(r4, "FullBox", PLC_COIL_OUTPUT, 300);
    printf("[Rung 4] CTU counter: contact=NO(sensor) -> counter(CTU,10) -> coil(full_box)\n");

    printf("\n=== Initial State ===\n");
    printf("Motor:      %s\n", plc_read_output(&plc, 100) ? "ON" : "OFF");
    printf("AuxReady:   %s\n", plc_read_output(&plc, 200) ? "ON" : "OFF");
    printf("FullBox:    %s\n", plc_read_output(&plc, 300) ? "ON" : "OFF");

    /* Simulate: press Start */
    printf("\n--- Press Start ---\n");
    plc_write_input(&plc, 0, true);
    plc_scan_cycle(&plc);
    printf("Motor:      %s\n", plc_read_output(&plc, 100) ? "ON" : "OFF");
    printf("Rung0 power: %s\n", plc.rungs[0].power_flow ? "TRUE" : "FALSE");

    /* Release Start */
    plc_write_input(&plc, 0, false);

    /* Simulate motor running - 10 scan cycles */
    printf("\n--- Motor running (10 scans) ---\n");
    for (int s = 0; s < 10; s++) {
        plc_scan_cycle(&plc);
    }
    printf("Scan count:  %u\n", plc_get_scan_count(&plc));
    printf("Motor:       %s\n", plc_read_output(&plc, 100) ? "ON" : "OFF");
    printf("T_DelayStart: elapsed=%ums, done=%s\n",
           plc.rungs[2].timers[0].accumulated_ms,
           plc.rungs[2].timers[0].done ? "YES" : "NO");

    /* Simulate sensor toggling for counter */
    printf("\n--- Sensor toggling (15 pulses) ---\n");
    for (int p = 0; p < 15; p++) {
        plc_write_input(&plc, 2, true);
        plc_scan_cycle(&plc);
        plc_write_input(&plc, 2, false);
        plc_scan_cycle(&plc);
    }
    printf("Counter C_Parts: value=%u, preset=%u, done=%s\n",
           plc.rungs[4].counters[0].accumulated,
           plc.rungs[4].counters[0].preset,
           plc.rungs[4].counters[0].done ? "YES" : "NO");
    printf("FullBox:         %s\n", plc_read_output(&plc, 300) ? "ON" : "OFF");

    /* Press Stop */
    printf("\n--- Press Stop ---\n");
    plc_write_input(&plc, 1, true);
    plc_scan_cycle(&plc);
    printf("Motor:      %s\n", plc_read_output(&plc, 100) ? "ON" : "OFF");
    printf("Rung0 power: %s\n", plc.rungs[0].power_flow ? "TRUE" : "FALSE");

    plc_write_input(&plc, 1, false);

    printf("\n=== Demo Complete ===\n");
    printf("Scan cycles: %u\n", plc_get_scan_count(&plc));

    return 0;
}
