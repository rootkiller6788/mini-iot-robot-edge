#include "modbus_proto.h"
#include "opc_ua_sim.h"
#include "plc_ladder.h"
#include "safety_plc.h"
#include "scada_collect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(void) {
    printf("\n");
    printf("  ==================================================================\n");
    printf("  ==          mini-industrial-control -- Full Demo                 ==\n");
    printf("  ==  Modbus / OPC UA / PLC Engine / Safety / SCADA Gateway        ==\n");
    printf("  ==================================================================\n");
    printf("\n");

    /* ---- 1. Modbus Protocol ---- */
    printf("--- 1. Modbus Protocol -----------------------------------------\n\n");

    {
        modbus_device_t dev;
        modbus_data_model_t model;

        modbus_init_device(&dev, MODBUS_RTU, 1);
        printf("  [OK] Modbus device initialized (RTU mode, slave ID=1)\n");
        modbus_init_data_model(&model);
        printf("  [OK] Data model initialized\n");

        uint8_t sample[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
        uint16_t crc = modbus_crc16(sample, sizeof(sample));
        printf("  [OK] CRC16 of read-holding-registers request: 0x%04X\n", crc);

        modbus_request_t req;
        memset(&req, 0, sizeof(req));
        req.slave_address = 1;
        req.function_code = MODBUS_FC_READ_HOLDING_REGISTERS;
        req.start_address = 0;
        req.quantity = 10;
        uint8_t buf[MODBUS_MAX_ADU_RTU];
        int len = modbus_build_request_rtu(buf, sizeof(buf), &req);
        printf("  [OK] Built RTU request frame: %d bytes\n", len);

        modbus_write_single_coil(&model, 0, true);
        modbus_write_single_coil(&model, 1, false);
        modbus_write_single_coil(&model, 2, true);
        modbus_write_single_register(&model, 0, 1000);
        modbus_write_single_register(&model, 1, 2000);
        uint16_t vals[4] = {10, 20, 30, 40};
        modbus_write_multiple_registers(&model, 2, 4, vals);

        bool c0, c1, c2;
        uint16_t r0, r1, r2;
        modbus_read_coil(&model, 0, &c0);
        modbus_read_coil(&model, 1, &c1);
        modbus_read_coil(&model, 2, &c2);
        modbus_read_holding_register(&model, 0, &r0);
        modbus_read_holding_register(&model, 1, &r1);
        modbus_read_holding_register(&model, 2, &r2);

        printf("  Coil reads:      C0=%d C1=%d C2=%d\n", c0, c1, c2);
        printf("  Register reads:  HR0=%d HR1=%d HR2=%d\n", r0, r1, r2);
        printf("  Address validation (50): %s\n",
               modbus_validate_address(50, MODBUS_MAX_REGISTERS) ? "valid" : "invalid");
    }

    printf("\n");

    /* ---- 2. OPC UA Server Simulation ---- */
    printf("--- 2. OPC UA Server Simulation ---------------------------------\n\n");

    {
        opcua_address_space_t as;
        opcua_init_address_space(&as);
        printf("  [OK] OPC UA address space initialized\n");

        uint32_t root = opcua_add_object_node(&as, "Objects", 0);
        printf("  [OK] Root Objects folder created (node ID=%u)\n", root);

        uint32_t area = opcua_add_object_node(&as, "ProductionArea", root);
        printf("  [OK] ProductionArea folder created (node ID=%u)\n", area);

        uint32_t temp = opcua_add_variable_node(&as, "Temperature", area,
                                                 OPCUA_TYPE_DOUBLE, OPCUA_ACCESS_RW);
        uint32_t press = opcua_add_variable_node(&as, "Pressure", area,
                                                  OPCUA_TYPE_DOUBLE, OPCUA_ACCESS_RW);
        uint32_t speed = opcua_add_variable_node(&as, "MotorSpeed", area,
                                                  OPCUA_TYPE_INT32, OPCUA_ACCESS_RW);
        uint32_t running = opcua_add_variable_node(&as, "MotorRunning", area,
                                                    OPCUA_TYPE_BOOLEAN, OPCUA_ACCESS_RW);
        printf("  Variables: Temp=%u Press=%u Speed=%u Running=%u\n",
               temp, press, speed, running);

        opcua_set_double_value(&as, temp, 23.5);
        opcua_set_double_value(&as, press, 1013.25);
        opcua_set_int32_value(&as, speed, 1500);
        opcua_set_bool_value(&as, running, true);

        double t_val, p_val;
        int32_t s_val;
        bool r_val;
        opcua_get_double_value(&as, temp, &t_val);
        opcua_get_double_value(&as, press, &p_val);
        opcua_get_int32_value(&as, speed, &s_val);
        opcua_get_bool_value(&as, running, &r_val);
        printf("  Read: Temp=%.1f C  Press=%.2f hPa  Speed=%d RPM  Run=%d\n",
               t_val, p_val, s_val, r_val);

        opcua_node_id_t results[OPCUA_MAX_BROWSE_RESULTS];
        int browse_count = opcua_browse_node(&as, area, results,
                                              OPCUA_MAX_BROWSE_RESULTS);
        printf("  Browse ProductionArea: %d children found\n", browse_count);

        uint32_t sub_id = opcua_create_subscription(&as, 100.0);
        uint32_t mi1 = opcua_add_monitored_item(&as, sub_id, temp, 50.0);
        printf("  Subscription=%u MonitoredItem=%u\n", sub_id, mi1);

        opcua_set_double_value(&as, temp, 28.0);
        opcua_variant_t new_val;
        if (opcua_check_data_change(&as, mi1, &new_val)) {
            printf("  [OK] Data change detected: %.1f\n", 28.0);
        }
    }

    printf("\n");

    /* ---- 3. PLC Ladder Logic Engine ---- */
    printf("--- 3. PLC Ladder Logic Engine --------------------------------\n\n");

    {
        plc_program_t plc;
        plc_init(&plc);
        printf("  [OK] PLC program initialized\n");

        plc_rung_t *rung1 = plc_add_rung(&plc);
        plc_add_contact(rung1, "StartPB", PLC_CONTACT_NO, 0);
        plc_add_contact(rung1, "StopPB", PLC_CONTACT_NC, 1);
        plc_add_coil(rung1, "MotorRun", PLC_COIL_OUTPUT, 0);
        printf("  [OK] Rung 0: Motor start/stop (I:0 NO AND I:1 NC -> Q:0)\n");

        plc_rung_t *rung2 = plc_add_rung(&plc);
        plc_add_contact(rung2, "MotorRun_C", PLC_CONTACT_NO, 0);
        plc_add_timer(rung2, "DelayTimer", PLC_TIMER_TON, 5000);
        plc_add_coil(rung2, "AuxLight", PLC_COIL_INTERNAL, 1);
        printf("  [OK] Rung 1: (Q:0 NO) -> TON 5s -> M:1\n");

        plc_rung_t *rung3 = plc_add_rung(&plc);
        plc_add_contact(rung3, "SensorIn", PLC_CONTACT_NO, 2);
        plc_add_counter(rung3, "PartCounter", PLC_COUNTER_CTU, 100);
        plc_add_coil(rung3, "BatchDone", PLC_COIL_OUTPUT, 2);
        printf("  [OK] Rung 2: (I:2 NO) -> CTU preset=100 -> Q:2\n");

        printf("\n  Simulating scan cycles:\n");
        printf("    Cycle 1: START pressed\n");
        plc_write_input(&plc, 0, true);
        plc_write_input(&plc, 1, false);
        plc_scan_cycle(&plc);
        printf("    -> MotorRun: %s\n", plc_read_output(&plc, 0) ? "ON" : "OFF");

        printf("    Cycle 2: STOP pressed\n");
        plc_write_input(&plc, 0, true);
        plc_write_input(&plc, 1, true);
        plc_scan_cycle(&plc);
        printf("    -> MotorRun: %s\n", plc_read_output(&plc, 0) ? "ON" : "OFF");

        printf("    Cycle 3: RESTART\n");
        plc_write_input(&plc, 0, true);
        plc_write_input(&plc, 1, false);
        plc_scan_cycle(&plc);
        printf("    -> MotorRun: %s\n", plc_read_output(&plc, 0) ? "ON" : "OFF");

        printf("    Cycles 4-8: Sensor pulses for counter\n");
        int j;
        for (j = 0; j < 5; j++) {
            plc_write_input(&plc, 2, true);
            plc_scan_cycle(&plc);
            plc_write_input(&plc, 2, false);
            plc_scan_cycle(&plc);
        }
        printf("    -> Counter: %d  |  Scan count: %d  |  Fault: %s\n",
               plc_counter_value(&rung3->counters),
               plc_get_scan_count(&plc),
               plc_is_fault(&plc) ? "FAULT" : "OK");
    }

    printf("\n");

    /* ---- 4. Safety PLC (SIL-Rated) ---- */
    printf("--- 4. Safety PLC (SIL-Rated) --------------------------------\n\n");

    {
        safety_controller_t sc;
        safety_init(&sc);
        printf("  [OK] Safety controller initialized\n");

        uint16_t ch1 = safety_add_dual_channel(&sc);
        printf("  [OK] Dual channel added (ID=%u)\n", ch1);

        uint16_t f1 = safety_add_function(&sc, "E-Stop", SAFETY_FUNC_ESTOP,
                                           SAFETY_SIL3);
        uint16_t f2 = safety_add_function(&sc, "DoorInterlock",
                                           SAFETY_FUNC_DOOR_INTERLOCK,
                                           SAFETY_SIL2);
        printf("  [OK] Functions: E-Stop(SIL3,ID=%u) Door(SIL2,ID=%u)\n", f1, f2);

        (void)f1; (void)f2;

        printf("  SIL Verification:\n");
        printf("    SIL3 + 1000h MTTFd: %s\n",
               safety_sil_verify(SAFETY_SIL3, 1000) ? "PASS" : "FAIL");
        printf("    SIL3 PFD: %.2e\n", safety_pfd_calculate(SAFETY_SIL3));

        printf("\n  Channel monitoring:\n");
        safety_update_channel_inputs(&sc, ch1, true, true, 0);
        printf("    Both closed: healthy=%d\n", safety_is_channel_healthy(&sc, ch1));
        safety_update_channel_inputs(&sc, ch1, true, false, 100);
        printf("    Discrepancy: healthy=%d\n", safety_is_channel_healthy(&sc, ch1));

        printf("\n  E-Stop scenario:\n");
        safety_estop_trigger(&sc, 1000);
        printf("    E-Stop pressed: active=%d healthy=%d\n",
               safety_estop_is_active(&sc), safety_system_healthy(&sc));
        safety_evaluate(&sc, 2000);
        safety_estop_release(&sc);
        printf("    Released: active=%d\n", safety_estop_is_active(&sc));

        printf("\n  Two-hand control test:\n");
        safety_two_hand_t th;
        safety_two_hand_init(&th, SAFETY_TWO_HAND_MAX_MS);
        safety_two_hand_update(&th, true, true, 0);
        printf("    Both pressed: output=%d\n", safety_two_hand_output(&th));
        safety_two_hand_update(&th, false, true, 100);
        printf("    Left released: output=%d\n", safety_two_hand_output(&th));

        printf("\n  Stats: faults=%u trips=%u\n",
               safety_get_total_faults(&sc), safety_get_total_trips(&sc));
    }

    printf("\n");

    /* ---- 5. SCADA Data Collection & Alarming ---- */
    printf("--- 5. SCADA Data Collection & Alarming -----------------------\n\n");

    {
        scada_collector_t collector;
        scada_init_collector(&collector);
        printf("  [OK] SCADA collector initialized\n");

        uint32_t t1 = scada_add_tag(&collector, "BoilerTemp",
                                     SCADA_TAG_ANALOG, 1, 0x4001, 1000);
        uint32_t t2 = scada_add_tag(&collector, "SteamPressure",
                                     SCADA_TAG_ANALOG, 1, 0x4002, 500);
        uint32_t t3 = scada_add_tag(&collector, "PumpRunning",
                                     SCADA_TAG_DISCRETE, 1, 0x0001, 100);
        uint32_t t4 = scada_add_tag(&collector, "PartCount",
                                     SCADA_TAG_COUNTER, 2, 0x0005, 500);
        printf("  Tags: BoilerTemp=%u SteamPressure=%u PumpRun=%u PartCount=%u\n",
               t1, t2, t3, t4);

        scada_tag_t *bt = scada_find_tag(&collector, "BoilerTemp");
        if (bt) scada_set_tag_scale(bt, 0.1, 0.0);
        scada_tag_t *sp = scada_find_tag(&collector, "SteamPressure");
        if (sp) { scada_set_tag_scale(sp, 0.01, 0.0); scada_set_tag_deadband(sp, 0.5); }
        printf("  [OK] Tag scaling configured\n");

        scada_write_tag_value(&collector, t1, 85.0);
        scada_write_tag_value(&collector, t2, 10.5);
        scada_write_tag_value(&collector, t3, 1.0);
        scada_write_tag_value(&collector, t4, 500.0);

        double v1, v2, v3;
        scada_read_tag(&collector, t1, &v1);
        scada_read_tag(&collector, t2, &v2);
        scada_read_tag(&collector, t3, &v3);
        printf("  Values: BT=%.1f C  SP=%.2f bar  Pump=%d\n", v1, v2, (int)v3);

        printf("\n  Polling (3 cycles):\n");
        int c;
        for (c = 1; c <= 3; c++) {
            scada_write_tag_value(&collector, t1, 85.0 + (double)c * 2.0);
            scada_poll_all_tags(&collector, (uint64_t)(c * 1000));
            printf("    Cycle %d: BoilerTemp=%.1f C\n", c, 85.0 + c * 2.0);
        }

        printf("\n  History (5 points):\n");
        int h;
        for (h = 0; h < 5; h++) {
            scada_store_history_point(&collector, t1, 80.0 + (double)h * 5.0,
                                      SCADA_QUALITY_GOOD, (uint64_t)(h * 60000));
            printf("    [%llu ms] value=%.1f\n",
                   (unsigned long long)(h * 60000), 80.0 + h * 5.0);
        }

        scada_history_point_t results[10];
        int qcount = scada_query_history(&collector, t1, 0ULL,
                                          240000ULL, results, 10);
        printf("  Query (0-240s): %d points returned\n", qcount);

        printf("\n  Alarm configuration:\n");
        scada_add_alarm(&collector, "BT_High", t1, SCADA_ALARM_HI, 90.0, 2.0);
        scada_add_alarm(&collector, "BT_HiHi", t1, SCADA_ALARM_HIHI, 100.0, 3.0);
        scada_add_alarm(&collector, "BT_Low", t1, SCADA_ALARM_LO, 60.0, 2.0);
        printf("  [OK] Alarms: HI(90C) HIHI(100C) LO(60C)\n");

        printf("\n  Alarm check (value=95C -> triggers HI):\n");
        scada_write_tag_value(&collector, t1, 95.0);
        scada_check_alarms(&collector, 5000);
        uint32_t active_alarms[10];
        int aa_count = scada_get_active_alarms(&collector, active_alarms, 10);
        printf("    Active alarms: %d\n", aa_count);

        if (aa_count > 0) {
            scada_acknowledge_alarm(&collector, active_alarms[0], 6000);
            printf("    Alarm acknowledged\n");
        }

        printf("\n  Daily report:\n");
        scada_daily_report_t report;
        scada_generate_daily_report(&collector, &report);
        scada_print_report(&report);
    }

    printf("\n");
    printf("  ==================================================================\n");
    printf("  ==     mini-industrial-control Demo Completed Successfully        ==\n");
    printf("  ==================================================================\n");
    printf("\n");

    return 0;
}
