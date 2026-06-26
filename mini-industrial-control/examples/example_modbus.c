#include "modbus_proto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_data_model(const modbus_data_model_t *model)
{
    printf("  Coils [0-9]:       ");
    for (int i = 0; i < 10; i++) {
        printf("%d ", model->coils[i] ? 1 : 0);
    }
    printf("\n");

    printf("  Holding Regs [0-4]:");
    for (int i = 0; i < 5; i++) {
        printf(" %u", model->holding_registers[i]);
    }
    printf("\n");
}

int main(void)
{
    printf("=== Modbus Protocol Demo ===\n\n");

    modbus_device_t device;
    modbus_init_device(&device, MODBUS_TCP, 1);
    printf("Device: slave_id=%u, mode=%s\n",
           device.slave_address,
           device.mode == MODBUS_TCP ? "TCP" : "RTU");

    modbus_data_model_t *dm = &device.data_model;

    /* Write coils */
    printf("\n--- Write Coils ---\n");
    modbus_write_single_coil(dm, 1, true);
    modbus_write_single_coil(dm, 3, true);
    modbus_write_single_coil(dm, 5, true);
    printf("Wrote coils 1,3,5 = ON\n");

    bool val;
    modbus_read_coil(dm, 1, &val);
    printf("Read coil 1: %s\n", val ? "ON" : "OFF");
    modbus_read_coil(dm, 3, &val);
    printf("Read coil 3: %s\n", val ? "ON" : "OFF");

    /* Write multiple coils */
    printf("\n--- Write Multiple Coils ---\n");
    bool batch[] = {true, false, true, true, false};
    modbus_write_multiple_coils(dm, 10, 5, batch);
    printf("Wrote coils 10-14: 1,0,1,1,0\n");

    /* Read holding registers */
    printf("\n--- Holding Registers ---\n");
    modbus_write_single_register(dm, 1, 12345);
    modbus_write_single_register(dm, 2, 54321);
    modbus_write_single_register(dm, 3, 1000);

    uint16_t reg;
    for (int i = 1; i <= 3; i++) {
        modbus_read_holding_register(dm, (uint16_t)i, &reg);
        printf("HR[%d] = %u\n", i, reg);
    }

    /* Write multiple registers */
    uint16_t multi_regs[] = {100, 200, 300, 400, 500};
    modbus_write_multiple_registers(dm, 10, 5, multi_regs);
    printf("Wrote 5 registers at HR[10-14]\n");

    print_data_model(dm);

    /* CRC16 calculation */
    printf("\n--- CRC16 ---\n");
    uint8_t test_data[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
    uint16_t crc = modbus_crc16(test_data, sizeof(test_data));
    printf("CRC16 of {01 03 00 00 00 0A}: 0x%04X\n", crc);

    /* Build RTU request */
    printf("\n--- RTU Frame ---\n");
    modbus_request_t req;
    req.slave_address = 1;
    req.function_code = MODBUS_FC_READ_HOLDING_REGISTERS;
    req.start_address = 0;
    req.quantity = 10;

    uint8_t rtu_buffer[256];
    int rtu_len = modbus_build_request_rtu(rtu_buffer, sizeof(rtu_buffer), &req);
    printf("Function: %s\n", modbus_function_code_string(req.function_code));
    printf("RTU frame (%d bytes): ", rtu_len);
    for (int i = 0; i < rtu_len; i++) printf("%02X ", rtu_buffer[i]);
    printf("\n");
    printf("CRC verified: %s\n", modbus_verify_crc(rtu_buffer, rtu_len) ? "OK" : "FAIL");

    /* Build TCP request */
    printf("\n--- TCP Frame ---\n");
    uint8_t tcp_buffer[260];
    int tcp_len = modbus_build_request_tcp(tcp_buffer, sizeof(tcp_buffer), 0x0001, &req);
    printf("TCP frame (%d bytes): ", tcp_len);
    for (int i = 0; i < tcp_len; i++) printf("%02X ", tcp_buffer[i]);
    printf("\n");

    /* MBAP header parsing */
    modbus_mbap_header_t mbap;
    int mbap_len = modbus_parse_mbap_header(tcp_buffer, tcp_len, &mbap);
    printf("MBAP: tid=%04X pid=%04X len=%u uid=%u\n",
           mbap.transaction_id, mbap.protocol_id, mbap.length, mbap.unit_id);

    /* Exception simulation */
    printf("\n--- Exception Codes ---\n");
    for (int ec = 1; ec <= 6; ec++) {
        printf("Exc %d: %s\n", ec, modbus_exception_string((modbus_exception_code_t)ec));
    }

    printf("\n=== Demo Complete ===\n");
    return 0;
}
