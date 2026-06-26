#include "modbus_proto.h"
#include "opc_ua_sim.h"
#include "plc_ladder.h"
#include "safety_plc.h"
#include "scada_collect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

static int tests_run = 0, tests_passed = 0;
#define TEST(name) do { tests_run++; printf("  TEST %s ... ", name); } while(0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); return 1; } while(0)
#define CHECK(cond, msg) if (!(cond)) FAIL(msg)

/* ================================================================
 *  Modbus Protocol Tests
 * ================================================================ */

static int test_modbus_crc16(void) {
    TEST("modbus_crc16 non-zero and deterministic");
    uint8_t data[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
    uint16_t crc = modbus_crc16(data, sizeof(data));
    CHECK(crc != 0, "CRC should be non-zero");
    uint16_t crc2 = modbus_crc16(data, sizeof(data));
    CHECK(crc == crc2, "CRC not deterministic");
    uint8_t frame[8];
    memcpy(frame, data, sizeof(data));
    frame[6] = (uint8_t)(crc & 0xFF);
    frame[7] = (uint8_t)(crc >> 8);
    CHECK(modbus_verify_crc(frame, 8) == true, "CRC verify failed on valid frame");
    PASS();
    return 0;
}

static int test_modbus_init_device(void) {
    TEST("modbus_init_device sets mode and slave address");
    modbus_device_t dev;
    memset(&dev, 0xAA, sizeof(dev));
    modbus_init_device(&dev, MODBUS_RTU, 10);
    CHECK(dev.slave_address == 10, "slave_address not set");
    CHECK(dev.mode == MODBUS_RTU, "mode not RTU");
    modbus_init_data_model(&dev.data_model);
    CHECK(dev.data_model.slave_id == 0, "data model slave_id should be 0");
    PASS();
    return 0;
}

static int test_modbus_read_null_args(void) {
    TEST("modbus_read_coil with NULL output pointer");
    modbus_data_model_t model;
    modbus_init_data_model(&model);
    bool ret = modbus_read_coil(&model, 5, NULL);
    CHECK(ret == false, "NULL ptr should return false");
    PASS();
    return 0;
}

static int test_modbus_read_out_of_range(void) {
    TEST("modbus_read_holding_register out of range");
    modbus_data_model_t model;
    modbus_init_data_model(&model);
    uint16_t val = 0xFFFF;
    bool ret = modbus_read_holding_register(&model,
                                             MODBUS_MAX_REGISTERS + 10, &val);
    CHECK(ret == false, "out-of-range read should return false");
    PASS();
    return 0;
}

static int test_modbus_write_roundtrip(void) {
    TEST("modbus_write/read single register roundtrip");
    modbus_data_model_t model;
    modbus_init_data_model(&model);
    CHECK(modbus_write_single_register(&model, 5, 0xABCD) == true,
          "write_single_register failed");
    uint16_t val = 0;
    CHECK(modbus_read_holding_register(&model, 5, &val) == true,
          "read_holding_register failed");
    CHECK(val == 0xABCD, "roundtrip value mismatch");
    PASS();
    return 0;
}

static int test_modbus_write_null_model(void) {
    TEST("modbus_write_single_coil NULL model");
    bool ret = modbus_write_single_coil(NULL, 0, true);
    CHECK(ret == false, "NULL model should return false");
    PASS();
    return 0;
}

/* ================================================================
 *  OPC UA Tests
 * ================================================================ */

static int test_opcua_init_address_space(void) {
    TEST("opcua_init_address_space zeroes counts");
    opcua_address_space_t as;
    memset(&as, 0xFF, sizeof(as));
    opcua_init_address_space(&as);
    CHECK(as.num_nodes == 0, "num_nodes not zero");
    CHECK(as.num_subscriptions == 0, "subscriptions not zero");
    CHECK(as.server_running == false, "server not stopped");
    PASS();
    return 0;
}

static int test_opcua_add_and_find_node(void) {
    TEST("opcua_add_object_node and find_node");
    opcua_address_space_t as;
    opcua_init_address_space(&as);
    uint32_t root = opcua_add_object_node(&as, "RootFolder", 0);
    CHECK(root > 0, "add_object_node failed");
    opcua_node_t *node = opcua_find_node(&as, root);
    CHECK(node != NULL, "find_node returned NULL");
    CHECK(strcmp(node->display_name, "RootFolder") == 0, "wrong node name");
    CHECK(opcua_find_node(&as, 9999) == NULL, "find non-existent should be NULL");
    PASS();
    return 0;
}

static int test_opcua_read_write_value(void) {
    TEST("opcua_set/get double value roundtrip");
    opcua_address_space_t as;
    opcua_init_address_space(&as);
    uint32_t root = opcua_add_object_node(&as, "Objects", 0);
    uint32_t var = opcua_add_variable_node(&as, "Temperature", root,
                                            OPCUA_TYPE_DOUBLE, OPCUA_ACCESS_RW);
    CHECK(var > 0, "add_variable_node failed");
    CHECK(opcua_set_double_value(&as, var, 25.5) == true, "set_double failed");
    double val = 0.0;
    CHECK(opcua_get_double_value(&as, var, &val) == true, "get_double failed");
    CHECK(fabs(val - 25.5) < 0.001, "value mismatch");
    PASS();
    return 0;
}

static int test_opcua_subscription_lifecycle(void) {
    TEST("opcua_create/delete subscription lifecycle");
    opcua_address_space_t as;
    opcua_init_address_space(&as);
    uint32_t sub_id = opcua_create_subscription(&as, 100.0);
    CHECK(sub_id > 0, "create_subscription returned invalid id");
    CHECK(as.num_subscriptions == 1, "subscription count not incremented");
    bool del_ok = opcua_delete_subscription(&as, sub_id);
    CHECK(del_ok == true, "delete_subscription failed");
    CHECK(as.num_subscriptions == 0, "subscription count not decremented");
    del_ok = opcua_delete_subscription(&as, 999);
    CHECK(del_ok == false, "delete non-existent subscription should fail");
    PASS();
    return 0;
}

/* ================================================================
 *  PLC Engine Tests
 * ================================================================ */

static int test_plc_init(void) {
    TEST("plc_init initializes all fields");
    plc_program_t plc;
    memset(&plc, 0xFF, sizeof(plc));
    plc_init(&plc);
    CHECK(plc.num_rungs == 0, "rungs not zero");
    CHECK(plc.scan_counter == 0, "scan_counter not zero");
    CHECK(plc.running == false, "running not false after init");
    CHECK(plc.fault == false, "fault not false after init");
    PASS();
    return 0;
}

static int test_plc_add_rung(void) {
    TEST("plc_add_rung increments count");
    plc_program_t plc;
    plc_init(&plc);
    plc_rung_t *rung = plc_add_rung(&plc);
    CHECK(rung != NULL, "add_rung returned NULL");
    CHECK(plc.num_rungs == 1, "num_rungs should be 1");
    PASS();
    return 0;
}

static int test_plc_evaluate_rung(void) {
    TEST("plc_evaluate_rung with NO and NC contacts");
    plc_program_t plc;
    plc_init(&plc);
    plc_rung_t *rung = plc_add_rung(&plc);
    plc_add_contact(rung, "Start", PLC_CONTACT_NO, 0);
    plc_add_contact(rung, "Stop", PLC_CONTACT_NC, 1);
    plc_add_coil(rung, "Motor", PLC_COIL_OUTPUT, 0);
    plc_write_input(&plc, 0, true);
    plc_write_input(&plc, 1, false);
    bool result = plc_evaluate_rung(rung);
    CHECK(result == true, "rung should evaluate true with NO closed, NC open");
    CHECK(plc_read_output(&plc, 0) == false, "output should not be set before scan");
    PASS();
    return 0;
}

static int test_plc_scan_cycle(void) {
    TEST("plc_scan_cycle updates outputs from inputs");
    plc_program_t plc;
    plc_init(&plc);
    plc_rung_t *rung = plc_add_rung(&plc);
    plc_add_contact(rung, "I0", PLC_CONTACT_NO, 0);
    plc_add_coil(rung, "Q0", PLC_COIL_OUTPUT, 0);
    plc_write_input(&plc, 0, true);
    plc_scan_cycle(&plc);
    CHECK(plc_read_output(&plc, 0) == true, "output should be true after scan");
    CHECK(plc.scan_counter == 1, "scan_counter should be 1");
    plc_write_input(&plc, 0, false);
    plc_scan_cycle(&plc);
    CHECK(plc_read_output(&plc, 0) == false, "output should be false");
    PASS();
    return 0;
}

/* ================================================================
 *  Safety Controller Tests
 * ================================================================ */

static int test_safety_init(void) {
    TEST("safety_init sets all fields zero");
    safety_controller_t sc;
    memset(&sc, 0xFF, sizeof(sc));
    safety_init(&sc);
    CHECK(sc.num_channels == 0, "channels not zero");
    CHECK(sc.num_functions == 0, "functions not zero");
    CHECK(sc.estop_active == false, "estop not inactive");
    PASS();
    return 0;
}

static int test_safety_dual_channel(void) {
    TEST("safety_dual_channel: healthy and discrepancy detection");
    safety_controller_t sc;
    safety_init(&sc);
    uint16_t ch = safety_add_dual_channel(&sc);
    CHECK(ch == 0, "first channel should be 0");
    safety_update_channel_inputs(&sc, ch, true, true, 100);
    CHECK(safety_is_channel_healthy(&sc, ch) == true,
          "matching inputs should be healthy");
    safety_update_channel_inputs(&sc, ch, true, false, 300);
    CHECK(safety_check_discrepancy(&sc, ch, 500) == true,
          "discrepancy should be detected");
    PASS();
    return 0;
}

static int test_safety_sil_verify(void) {
    TEST("safety_sil_verify thresholds");
    CHECK(safety_sil_verify(SAFETY_SIL3, 100000) == true,
          "SIL3 + 100k hrs MTTFd should pass");
    CHECK(safety_sil_verify(SAFETY_SIL4, 1000) == false,
          "SIL4 + 1k hrs MTTFd should fail");
    PASS();
    return 0;
}

static int test_safety_estop(void) {
    TEST("safety_estop trigger/release cycle");
    safety_controller_t sc;
    safety_init(&sc);
    safety_add_function(&sc, "ESTOP", SAFETY_FUNC_ESTOP, SAFETY_SIL3);
    safety_estop_trigger(&sc, 100);
    CHECK(safety_estop_is_active(&sc) == true, "estop should be active");
    safety_estop_release(&sc);
    CHECK(safety_estop_is_active(&sc) == false, "estop should be released");
    PASS();
    return 0;
}

static int test_safety_two_hand(void) {
    TEST("safety_two_hand sequence validation");
    safety_two_hand_t th;
    safety_two_hand_init(&th, SAFETY_TWO_HAND_MAX_MS);
    safety_two_hand_update(&th, true, false, 0);
    safety_two_hand_update(&th, true, true, 100);
    CHECK(safety_two_hand_output(&th) == true,
          "both pressed within window should output true");
    safety_two_hand_update(&th, true, false, 200);
    CHECK(safety_two_hand_output(&th) == false,
          "release one should output false");
    PASS();
    return 0;
}

/* ================================================================
 *  SCADA Gateway Tests
 * ================================================================ */

static int test_scada_init_collector(void) {
    TEST("scada_init_collector zeros fields");
    scada_collector_t collector;
    memset(&collector, 0xFF, sizeof(collector));
    scada_init_collector(&collector);
    CHECK(collector.num_tags == 0, "tags not zero");
    CHECK(collector.num_history_points == 0, "history not zero");
    CHECK(collector.collecting == false, "collecting not false");
    PASS();
    return 0;
}

static int test_scada_tag_write_read_roundtrip(void) {
    TEST("scada_write/read tag value roundtrip");
    scada_collector_t collector;
    scada_init_collector(&collector);
    uint32_t idx = scada_add_tag(&collector, "Pressure", SCADA_TAG_ANALOG,
                                  1, 0x4002, 500);
    CHECK(scada_write_tag_value(&collector, idx, 1013.25) == true,
          "write_tag_value failed");
    double val = 0.0;
    CHECK(scada_read_tag(&collector, idx, &val) == true,
          "read_tag failed");
    CHECK(fabs(val - 1013.25) < 0.01, "roundtrip value mismatch");
    PASS();
    return 0;
}

static int test_scada_alarms(void) {
    TEST("scada_check_alarms activates on threshold crossing");
    scada_collector_t collector;
    scada_init_collector(&collector);
    uint32_t idx = scada_add_tag(&collector, "Temp", SCADA_TAG_ANALOG,
                                  1, 0x4001, 1000);
    scada_add_alarm(&collector, "HighTemp", idx, SCADA_ALARM_HI, 80.0, 2.0);
    scada_add_alarm(&collector, "LowTemp", idx, SCADA_ALARM_LO, 10.0, 1.0);
    scada_write_tag_value(&collector, idx, 85.0);
    scada_check_alarms(&collector, 1000);
    uint32_t active[4];
    int count = scada_get_active_alarms(&collector, active, 4);
    CHECK(count > 0, "high-value alarm should be active");
    PASS();
    return 0;
}

static int test_scada_history_and_query(void) {
    TEST("scada_store/query history points");
    scada_collector_t collector;
    scada_init_collector(&collector);
    uint32_t idx = scada_add_tag(&collector, "HistTag", SCADA_TAG_ANALOG,
                                  1, 0x4001, 1000);
    scada_write_tag_value(&collector, idx, 50.0);
    int i;
    for (i = 0; i < 10; i++) {
        scada_store_history_point(&collector, idx, (double)i * 1.5,
                                  SCADA_QUALITY_GOOD, (uint64_t)(i * 1000));
    }
    scada_history_point_t results[20];
    int count = scada_query_history(&collector, idx, 0ULL,
                                     10000ULL, results, 20);
    CHECK(count >= 10, "query_history should return stored points");
    PASS();
    return 0;
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void) {
    printf("=== mini-industrial-control Unit Tests ===\n\n");

    test_modbus_crc16();
    test_modbus_init_device();
    test_modbus_read_null_args();
    test_modbus_read_out_of_range();
    test_modbus_write_roundtrip();
    test_modbus_write_null_model();

    test_opcua_init_address_space();
    test_opcua_add_and_find_node();
    test_opcua_read_write_value();
    test_opcua_subscription_lifecycle();

    test_plc_init();
    test_plc_add_rung();
    test_plc_evaluate_rung();
    test_plc_scan_cycle();

    test_safety_init();
    test_safety_dual_channel();
    test_safety_sil_verify();
    test_safety_estop();
    test_safety_two_hand();

    test_scada_init_collector();
    test_scada_tag_write_read_roundtrip();
    test_scada_alarms();
    test_scada_history_and_query();

    printf("\n%d / %d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
