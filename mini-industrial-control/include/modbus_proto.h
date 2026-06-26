#ifndef MODBUS_PROTO_H
#define MODBUS_PROTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MODBUS_MAX_PDU_SIZE      253
#define MODBUS_MAX_ADU_RTU       256
#define MODBUS_MAX_ADU_TCP       260
#define MODBUS_SERVER_ID_DEFAULT 1
#define MODBUS_PORT_DEFAULT      502
#define MODBUS_RTU_BAUD_DEFAULT  19200
#define MODBUS_TIMEOUT_MS        1000
#define MODBUS_MAX_COILS         2000
#define MODBUS_MAX_REGISTERS     125

typedef enum {
    MODBUS_RTU = 0,
    MODBUS_ASCII = 1,
    MODBUS_TCP = 2
} modbus_mode_t;

typedef enum {
    MODBUS_FC_READ_COILS                = 0x01,
    MODBUS_FC_READ_DISCRETE_INPUTS      = 0x02,
    MODBUS_FC_READ_HOLDING_REGISTERS    = 0x03,
    MODBUS_FC_READ_INPUT_REGISTERS      = 0x04,
    MODBUS_FC_WRITE_SINGLE_COIL         = 0x05,
    MODBUS_FC_WRITE_SINGLE_REGISTER     = 0x06,
    MODBUS_FC_WRITE_MULTIPLE_COILS      = 0x0F,
    MODBUS_FC_WRITE_MULTIPLE_REGISTERS  = 0x10
} modbus_function_code_t;

typedef enum {
    MODBUS_EXC_NONE                     = 0x00,
    MODBUS_EXC_ILLEGAL_FUNCTION         = 0x01,
    MODBUS_EXC_ILLEGAL_DATA_ADDRESS     = 0x02,
    MODBUS_EXC_ILLEGAL_DATA_VALUE       = 0x03,
    MODBUS_EXC_SLAVE_DEVICE_FAILURE     = 0x04,
    MODBUS_EXC_ACKNOWLEDGE              = 0x05,
    MODBUS_EXC_SLAVE_DEVICE_BUSY        = 0x06,
    MODBUS_EXC_MEMORY_PARITY_ERROR      = 0x08,
    MODBUS_EXC_GATEWAY_PATH_UNAVAILABLE = 0x0A,
    MODBUS_EXC_GATEWAY_TARGET_FAILED    = 0x0B
} modbus_exception_code_t;

#pragma pack(push, 1)
typedef struct {
    uint16_t transaction_id;
    uint16_t protocol_id;
    uint16_t length;
    uint8_t  unit_id;
} modbus_mbap_header_t;
#pragma pack(pop)

typedef struct {
    uint8_t  slave_address;
    uint8_t  function_code;
    uint16_t start_address;
    uint16_t quantity;
} modbus_request_t;

typedef struct {
    uint8_t  slave_address;
    uint8_t  function_code;
    uint8_t  byte_count;
    uint8_t  data[MODBUS_MAX_PDU_SIZE];
} modbus_response_t;

typedef struct {
    uint8_t  slave_address;
    uint8_t  function_code;
    uint8_t  exception_code;
} modbus_exception_response_t;

typedef struct {
    bool     coils[MODBUS_MAX_COILS];
    uint16_t discrete_inputs[MODBUS_MAX_COILS];
    uint16_t holding_registers[MODBUS_MAX_REGISTERS];
    uint16_t input_registers[MODBUS_MAX_REGISTERS];
    uint8_t  slave_id;
} modbus_data_model_t;

typedef struct {
    modbus_mode_t       mode;
    uint8_t             slave_address;
    modbus_data_model_t data_model;
    uint16_t            coils_count;
    uint16_t            discrete_inputs_count;
    uint16_t            holding_registers_count;
    uint16_t            input_registers_count;
    uint32_t            baud_rate;
    uint8_t             parity;
    uint8_t             stop_bits;
    bool                connected;
} modbus_device_t;

void     modbus_init_device(modbus_device_t *dev, modbus_mode_t mode, uint8_t slave_id);
void     modbus_init_data_model(modbus_data_model_t *model);

bool modbus_read_coil(const modbus_data_model_t *model, uint16_t address, bool *value);
bool modbus_read_discrete_input(const modbus_data_model_t *model, uint16_t address, bool *value);
bool modbus_read_holding_register(const modbus_data_model_t *model, uint16_t address, uint16_t *value);
bool modbus_read_input_register(const modbus_data_model_t *model, uint16_t address, uint16_t *value);

bool modbus_write_single_coil(modbus_data_model_t *model, uint16_t address, bool value);
bool modbus_write_single_register(modbus_data_model_t *model, uint16_t address, uint16_t value);
bool modbus_write_multiple_coils(modbus_data_model_t *model, uint16_t start_address,
                                 uint16_t quantity, const bool *values);
bool modbus_write_multiple_registers(modbus_data_model_t *model, uint16_t start_address,
                                     uint16_t quantity, const uint16_t *values);

uint16_t modbus_crc16(const uint8_t *data, size_t length);
bool     modbus_verify_crc(const uint8_t *data, size_t length);

int  modbus_build_request_rtu(uint8_t *buffer, size_t buf_size,
                              const modbus_request_t *req);
int  modbus_build_request_tcp(uint8_t *buffer, size_t buf_size,
                              uint16_t transaction_id, const modbus_request_t *req);
int  modbus_parse_response_rtu(const uint8_t *buffer, size_t length,
                               modbus_response_t *resp);
int  modbus_parse_response_tcp(const uint8_t *buffer, size_t length,
                               modbus_response_t *resp);
bool modbus_is_exception(const modbus_response_t *resp);
int  modbus_parse_exception(const uint8_t *buffer, size_t length,
                            modbus_exception_response_t *exc);

int  modbus_build_mbap_header(uint8_t *buffer, uint16_t transaction_id,
                              uint16_t length, uint8_t unit_id);
int  modbus_parse_mbap_header(const uint8_t *buffer, size_t length,
                              modbus_mbap_header_t *header);

size_t modbus_coil_address_to_offset(uint16_t address);
size_t modbus_register_address_to_offset(uint16_t address);
bool   modbus_validate_address(uint16_t address, uint16_t max_count);
const char* modbus_exception_string(modbus_exception_code_t code);
const char* modbus_function_code_string(modbus_function_code_t code);

#endif /* MODBUS_PROTO_H */
