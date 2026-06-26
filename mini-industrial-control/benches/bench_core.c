#include "modbus_proto.h"
#include "opc_ua_sim.h"
#include "plc_ladder.h"
#include "safety_plc.h"
#include "scada_collect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_ms(void) {
    return (double)clock() * 1000.0 / (double)CLOCKS_PER_SEC;
}

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 5000;
    if (N < 1) N = 5000;
    int i, k;
    double t0, t1;
    volatile int dummy = 0;

    printf("=== mini-industrial-control Benchmarks (N=%d) ===\n\n", N);

    /* ---- 1. Modbus: init_device and read/write coils ---- */
    {
        modbus_device_t dev;
        modbus_data_model_t model;
        modbus_init_device(&dev, MODBUS_RTU, 1);
        modbus_init_data_model(&model);
        bool bval = false;
        uint16_t uval = 0;
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            modbus_read_coil(&model, (uint16_t)(i % 100), &bval);
            modbus_read_discrete_input(&model, (uint16_t)(i % 100), &bval);
            modbus_read_holding_register(&model, (uint16_t)(i % 100), &uval);
            modbus_read_input_register(&model, (uint16_t)(i % 100), &uval);
        }
        t1 = now_ms();
        printf("  modbus read ops:               %d ops in %.1f ms  (%.1f us/op)\n",
               N * 4, t1 - t0, (t1 - t0) / (N * 4) * 1000.0);

        t0 = now_ms();
        for (i = 0; i < N; i++) {
            modbus_write_single_coil(&model, (uint16_t)(i % 100), (bool)(i & 1));
            modbus_write_single_register(&model, (uint16_t)(i % 100), (uint16_t)i);
        }
        t1 = now_ms();
        printf("  modbus write ops:              %d ops in %.1f ms  (%.1f us/op)\n",
               N * 2, t1 - t0, (t1 - t0) / (N * 2) * 1000.0);
        dummy += bval ? 1 : 0;
        dummy += (int)uval;
    }

    /* ---- 2. Modbus: CRC16 ---- */
    {
        uint8_t data[MODBUS_MAX_ADU_RTU];
        memset(data, 0xA5, sizeof(data));
        t0 = now_ms();
        for (i = 0; i < N * 10; i++) {
            dummy += (int)modbus_crc16(data, sizeof(data));
        }
        t1 = now_ms();
        printf("  modbus_crc16:                  %d ops in %.1f ms  (%.1f us/op)\n",
               N * 10, t1 - t0, (t1 - t0) / (N * 10) * 1000.0);
    }

    /* ---- 3. Modbus: RTU frame build/parse ---- */
    {
        uint8_t buf[MODBUS_MAX_ADU_RTU];
        modbus_request_t req;
        modbus_response_t resp;
        memset(&req, 0, sizeof(req));
        req.slave_address = 1;
        req.function_code = MODBUS_FC_READ_HOLDING_REGISTERS;
        req.start_address = 0;
        req.quantity = 10;
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            dummy += modbus_build_request_rtu(buf, sizeof(buf), &req);
        }
        t1 = now_ms();
        printf("  modbus_build_request_rtu:      %d ops in %.1f ms  (%.1f us/op)\n",
               N, t1 - t0, (t1 - t0) / N * 1000.0);

        uint8_t tcp_buf[MODBUS_MAX_ADU_TCP];
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            dummy += modbus_build_request_tcp(tcp_buf, sizeof(tcp_buf),
                                              (uint16_t)i, &req);
        }
        t1 = now_ms();
        printf("  modbus_build_request_tcp:      %d ops in %.1f ms  (%.1f us/op)\n",
               N, t1 - t0, (t1 - t0) / N * 1000.0);

        /* parse */
        modbus_build_request_rtu(buf, sizeof(buf), &req);
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            dummy += modbus_parse_response_rtu(buf, sizeof(buf), &resp);
        }
        t1 = now_ms();
        printf("  modbus_parse_response_rtu:     %d ops in %.1f ms  (%.1f us/op)\n",
               N, t1 - t0, (t1 - t0) / N * 1000.0);
    }

    /* ---- 4. Modbus: multiple writes ---- */
    {
        modbus_data_model_t model;
        modbus_init_data_model(&model);
        uint16_t vals[10];
        bool cvals[20];
        for (i = 0; i < 10; i++) vals[i] = (uint16_t)i;
        for (i = 0; i < 20; i++) cvals[i] = (bool)(i & 1);
        int mN = N / 4;
        if (mN < 1) mN = 1;
        t0 = now_ms();
        for (i = 0; i < mN; i++) {
            modbus_write_multiple_registers(&model, 0, 10, vals);
            modbus_write_multiple_coils(&model, 0, 20, cvals);
        }
        t1 = now_ms();
        printf("  modbus_write_multiple:         %d ops in %.1f ms  (%.1f us/op)\n",
               mN * 2, t1 - t0, (t1 - t0) / (mN * 2) * 1000.0);
    }

    /* ---- 5. OPC UA: init address space and add nodes ---- */
    {
        opcua_address_space_t as;
        opcua_init_address_space(&as);
        int objN = N / 5;
        if (objN < 1) objN = 1;
        t0 = now_ms();
        uint32_t root = opcua_add_object_node(&as, "Root", 0);
        uint32_t folder = opcua_add_object_node(&as, "Devices", root);
        for (i = 0; i < objN; i++) {
            char name[32];
            snprintf(name, sizeof(name), "Sensor_%d", i);
            opcua_add_variable_node(&as, name, folder,
                                    OPCUA_TYPE_DOUBLE, OPCUA_ACCESS_RW);
            snprintf(name, sizeof(name), "Method_%d", i);
            opcua_add_method_node(&as, name, folder);
        }
        t1 = now_ms();
        printf("  opcua_add nodes:               %d ops in %.1f ms  (%.1f us/op)\n",
               objN * 2 + 2, t1 - t0, (t1 - t0) / (objN * 2 + 2) * 1000.0);
        dummy += (int)root + (int)folder;
    }

    /* ---- 6. OPC UA: read/write values ---- */
    {
        opcua_address_space_t as;
        opcua_init_address_space(&as);
        uint32_t root = opcua_add_object_node(&as, "Objects", 0);
        uint32_t vid = opcua_add_variable_node(&as, "Temperature", root,
                                                OPCUA_TYPE_DOUBLE, OPCUA_ACCESS_RW);
        opcua_set_double_value(&as, vid, 25.0);
        double dtmp = 0.0;
        bool btmp = false;
        int32_t itmp = 0;
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            opcua_set_double_value(&as, vid, (double)(i % 100));
            opcua_get_double_value(&as, vid, &dtmp);
            opcua_set_bool_value(&as, vid, (bool)(i & 1));
            opcua_get_bool_value(&as, vid, &btmp);
            opcua_set_int32_value(&as, vid, i);
            opcua_get_int32_value(&as, vid, &itmp);
        }
        t1 = now_ms();
        printf("  opcua read/write value:        %d ops in %.1f ms  (%.1f us/op)\n",
               N * 6, t1 - t0, (t1 - t0) / (N * 6) * 1000.0);
        dummy += (int)dtmp + (btmp ? 1 : 0) + (int)itmp;
    }

    /* ---- 7. OPC UA: browse nodes ---- */
    {
        opcua_address_space_t as;
        opcua_init_address_space(&as);
        uint32_t root = opcua_add_object_node(&as, "Objects", 0);
        for (i = 0; i < 50; i++) {
            char name[32];
            snprintf(name, sizeof(name), "Item_%d", i);
            opcua_add_variable_node(&as, name, root,
                                    OPCUA_TYPE_INT32, OPCUA_ACCESS_RW);
        }
        opcua_node_id_t results[OPCUA_MAX_BROWSE_RESULTS];
        int browseN = N / 2;
        if (browseN < 1) browseN = 1;
        t0 = now_ms();
        for (i = 0; i < browseN; i++) {
            dummy += opcua_browse_node(&as, root, results, OPCUA_MAX_BROWSE_RESULTS);
        }
        t1 = now_ms();
        printf("  opcua_browse_node:             %d ops in %.1f ms  (%.1f us/op)\n",
               browseN, t1 - t0, (t1 - t0) / browseN * 1000.0);
    }

    /* ---- 8. OPC UA: subscriptions and monitored items ---- */
    {
        opcua_address_space_t as;
        opcua_init_address_space(&as);
        uint32_t root = opcua_add_object_node(&as, "Objects", 0);
        uint32_t vid = opcua_add_variable_node(&as, "Sensor", root,
                                                OPCUA_TYPE_DOUBLE, OPCUA_ACCESS_RW);
        uint32_t sub_id = opcua_create_subscription(&as, 100.0);
        uint32_t mi_id = opcua_add_monitored_item(&as, sub_id, vid, 50.0);
        opcua_set_double_value(&as, vid, 25.0);
        opcua_variant_t new_val;
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            opcua_set_double_value(&as, vid, (double)(25 + (i & 3)));
            dummy += opcua_check_data_change(&as, mi_id, &new_val) ? 1 : 0;
        }
        t1 = now_ms();
        printf("  opcua monitored items:         %d ops in %.1f ms  (%.1f us/op)\n",
               N, t1 - t0, (t1 - t0) / N * 1000.0);
        (void)sub_id;
    }

    /* ---- 9. PLC: init and rung evaluation ---- */
    {
        plc_program_t plc;
        plc_init(&plc);
        plc_rung_t *rung = plc_add_rung(&plc);
        plc_add_contact(rung, "Start", PLC_CONTACT_NO, 0);
        plc_add_contact(rung, "Stop", PLC_CONTACT_NC, 1);
        plc_add_coil(rung, "Motor", PLC_COIL_OUTPUT, 0);
        plc_write_input(&plc, 0, true);
        plc_write_input(&plc, 1, false);
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            dummy += plc_evaluate_rung(rung) ? 1 : 0;
        }
        t1 = now_ms();
        printf("  plc_evaluate_rung:             %d ops in %.1f ms  (%.1f us/op)\n",
               N, t1 - t0, (t1 - t0) / N * 1000.0);
    }

    /* ---- 10. PLC: scan cycle ---- */
    {
        plc_program_t plc;
        plc_init(&plc);
        plc_rung_t *r1 = plc_add_rung(&plc);
        plc_add_contact(r1, "I0", PLC_CONTACT_NO, 0);
        plc_add_coil(r1, "Q0", PLC_COIL_OUTPUT, 0);
        plc_rung_t *r2 = plc_add_rung(&plc);
        plc_add_contact(r2, "I1", PLC_CONTACT_NO, 1);
        plc_add_timer(r2, "T0", PLC_TIMER_TON, 1000);
        plc_rung_t *r3 = plc_add_rung(&plc);
        plc_add_contact(r3, "C0", PLC_CONTACT_NO, 2);
        plc_add_counter(r3, "C1", PLC_COUNTER_CTU, 10);
        plc_write_input(&plc, 0, true);
        plc_write_input(&plc, 1, true);
        int scanN = N / 2;
        if (scanN < 1) scanN = 1;
        t0 = now_ms();
        for (i = 0; i < scanN; i++) {
            plc_scan_cycle(&plc);
        }
        t1 = now_ms();
        printf("  plc_scan_cycle:                %d ops in %.1f ms  (%.1f us/op)\n",
               scanN, t1 - t0, (t1 - t0) / scanN * 1000.0);
    }

    /* ---- 11. Safety: init and evaluate ---- */
    {
        safety_controller_t sc;
        safety_init(&sc);
        uint16_t ch = safety_add_dual_channel(&sc);
        safety_add_function(&sc, "ESTOP", SAFETY_FUNC_ESTOP, SAFETY_SIL3);
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            safety_update_channel_inputs(&sc, ch, true, true,
                                         (uint32_t)(i * 10));
            safety_evaluate(&sc, (uint64_t)(i * 10));
        }
        t1 = now_ms();
        int total = N * 2;
        printf("  safety update+evaluate:        %d ops in %.1f ms  (%.1f us/op)\n",
               total, t1 - t0, (t1 - t0) / total * 1000.0);
    }

    /* ---- 12. Safety: e-stop trigger/release ---- */
    {
        safety_controller_t sc;
        safety_init(&sc);
        safety_add_function(&sc, "ESTOP1", SAFETY_FUNC_ESTOP, SAFETY_SIL3);
        t0 = now_ms();
        for (i = 0; i < N * 2; i++) {
            safety_estop_trigger(&sc, (uint64_t)i);
            dummy += safety_estop_is_active(&sc) ? 1 : 0;
            safety_estop_release(&sc);
        }
        t1 = now_ms();
        printf("  safety_estop cycle:            %d ops in %.1f ms  (%.1f us/op)\n",
               N * 2, t1 - t0, (t1 - t0) / (N * 2) * 1000.0);
    }

    /* ---- 13. Safety: two-hand control and SIL verify ---- */
    {
        safety_two_hand_t th;
        safety_two_hand_init(&th, SAFETY_TWO_HAND_MAX_MS);
        t0 = now_ms();
        for (i = 0; i < N * 3; i++) {
            safety_two_hand_update(&th, (bool)((i & 1) == 0),
                                   (bool)((i & 2) == 0), (uint32_t)(i * 10));
            dummy += safety_two_hand_output(&th) ? 1 : 0;
        }
        t1 = now_ms();
        printf("  safety_two_hand:               %d ops in %.1f ms  (%.1f us/op)\n",
               N * 3, t1 - t0, (t1 - t0) / (N * 3) * 1000.0);

        t0 = now_ms();
        for (i = 0; i < N * 5; i++) {
            dummy += safety_sil_verify(SAFETY_SIL3, 100000) ? 1 : 0;
        }
        t1 = now_ms();
        printf("  safety_sil_verify:             %d ops in %.1f ms  (%.1f us/op)\n",
               N * 5, t1 - t0, (t1 - t0) / (N * 5) * 1000.0);
    }

    /* ---- 14. SCADA: tag management and read ---- */
    {
        scada_collector_t collector;
        scada_init_collector(&collector);
        scada_add_tag(&collector, "Temperature", SCADA_TAG_ANALOG, 1, 0x4001, 1000);
        scada_add_tag(&collector, "Pressure", SCADA_TAG_ANALOG, 1, 0x4002, 500);
        scada_add_tag(&collector, "Pump_Run", SCADA_TAG_DISCRETE, 1, 0x0001, 100);
        scada_write_tag_value(&collector, 0, 25.0);
        scada_write_tag_value(&collector, 1, 1013.0);
        scada_write_tag_value(&collector, 2, 1.0);
        double val;
        t0 = now_ms();
        for (i = 0; i < N; i++) {
            scada_read_tag(&collector, (uint32_t)(i % 3), &val);
        }
        t1 = now_ms();
        printf("  scada_read_tag:                %d ops in %.1f ms  (%.1f us/op)\n",
               N, t1 - t0, (t1 - t0) / N * 1000.0);
        dummy += (int)val;
    }

    /* ---- 15. SCADA: poll all, history, alarms, and report ---- */
    {
        scada_collector_t collector;
        scada_init_collector(&collector);
        scada_add_tag(&collector, "Temp", SCADA_TAG_ANALOG, 1, 0x4001, 1000);
        scada_add_tag(&collector, "Flow", SCADA_TAG_COUNTER, 1, 0x0002, 500);
        scada_write_tag_value(&collector, 0, 75.0);
        scada_write_tag_value(&collector, 1, 150.0);

        int pollN = N / 2;
        if (pollN < 1) pollN = 1;
        t0 = now_ms();
        for (i = 0; i < pollN; i++) {
            scada_poll_all_tags(&collector, (uint64_t)(i * 1000));
        }
        t1 = now_ms();
        printf("  scada_poll_all_tags:           %d ops in %.1f ms  (%.1f us/op)\n",
               pollN, t1 - t0, (t1 - t0) / pollN * 1000.0);

        t0 = now_ms();
        for (i = 0; i < N; i++) {
            scada_store_history_point(&collector, 0, (double)(i % 1000) * 0.1,
                                      SCADA_QUALITY_GOOD, (uint64_t)(i * 1000));
        }
        t1 = now_ms();
        printf("  scada_store_history_point:     %d ops in %.1f ms  (%.1f us/op)\n",
               N, t1 - t0, (t1 - t0) / N * 1000.0);

        t0 = now_ms();
        for (i = 0; i < N; i++) {
            scada_write_tag_value(&collector, 0, (double)(i % 120));
            scada_check_alarms(&collector, (uint64_t)(i * 1000));
        }
        t1 = now_ms();
        printf("  scada_check_alarms:            %d ops in %.1f ms  (%.1f us/op)\n",
               N, t1 - t0, (t1 - t0) / N * 1000.0);

        scada_history_point_t results[100];
        int qN = N / 4;
        if (qN < 1) qN = 1;
        t0 = now_ms();
        for (i = 0; i < qN; i++) {
            scada_query_history(&collector, 0, 0ULL, 99000ULL, results, 100);
        }
        t1 = now_ms();
        printf("  scada_query_history:           %d ops in %.1f ms  (%.1f us/op)\n",
               qN, t1 - t0, (t1 - t0) / qN * 1000.0);

        scada_daily_report_t report;
        int repN = N / 2;
        if (repN < 1) repN = 1;
        t0 = now_ms();
        for (i = 0; i < repN; i++) {
            scada_generate_daily_report(&collector, &report);
        }
        t1 = now_ms();
        printf("  scada_daily_report:            %d ops in %.1f ms  (%.1f us/op)\n",
               repN, t1 - t0, (t1 - t0) / repN * 1000.0);
    }

    printf("\nDone.\n");
    return dummy ? 0 : 0;
}
