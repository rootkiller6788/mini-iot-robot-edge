#include "modbus_proto.h"
#include <string.h>
#include <stdlib.h>

static const uint16_t crc_table[] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040
};

void modbus_init_device(modbus_device_t *dev, modbus_mode_t mode, uint8_t slave_id)
{
    memset(dev, 0, sizeof(modbus_device_t));
    dev->mode = mode;
    dev->slave_address = slave_id;
    dev->connected = false;
    modbus_init_data_model(&dev->data_model);
    dev->data_model.slave_id = slave_id;
    dev->coils_count = MODBUS_MAX_COILS;
    dev->discrete_inputs_count = MODBUS_MAX_COILS;
    dev->holding_registers_count = MODBUS_MAX_REGISTERS;
    dev->input_registers_count = MODBUS_MAX_REGISTERS;
    dev->baud_rate = MODBUS_RTU_BAUD_DEFAULT;
    dev->parity = 2;
    dev->stop_bits = 1;
}

void modbus_init_data_model(modbus_data_model_t *model)
{
    memset(model, 0, sizeof(modbus_data_model_t));
}

bool modbus_read_coil(const modbus_data_model_t *model, uint16_t address, bool *value)
{
    if (!model || !value) return false;
    size_t offset = modbus_coil_address_to_offset(address);
    if (!modbus_validate_address(address, MODBUS_MAX_COILS)) return false;
    *value = model->coils[offset];
    return true;
}

bool modbus_read_discrete_input(const modbus_data_model_t *model, uint16_t address, bool *value)
{
    if (!model || !value) return false;
    size_t offset = modbus_coil_address_to_offset(address);
    if (!modbus_validate_address(address, MODBUS_MAX_COILS)) return false;
    *value = (model->discrete_inputs[offset] != 0);
    return true;
}

bool modbus_read_holding_register(const modbus_data_model_t *model, uint16_t address, uint16_t *value)
{
    if (!model || !value) return false;
    size_t offset = modbus_register_address_to_offset(address);
    if (!modbus_validate_address(address, MODBUS_MAX_REGISTERS)) return false;
    *value = model->holding_registers[offset];
    return true;
}

bool modbus_read_input_register(const modbus_data_model_t *model, uint16_t address, uint16_t *value)
{
    if (!model || !value) return false;
    size_t offset = modbus_register_address_to_offset(address);
    if (!modbus_validate_address(address, MODBUS_MAX_REGISTERS)) return false;
    *value = model->input_registers[offset];
    return true;
}

bool modbus_write_single_coil(modbus_data_model_t *model, uint16_t address, bool value)
{
    if (!model) return false;
    size_t offset = modbus_coil_address_to_offset(address);
    if (!modbus_validate_address(address, MODBUS_MAX_COILS)) return false;
    model->coils[offset] = value;
    return true;
}

bool modbus_write_single_register(modbus_data_model_t *model, uint16_t address, uint16_t value)
{
    if (!model) return false;
    size_t offset = modbus_register_address_to_offset(address);
    if (!modbus_validate_address(address, MODBUS_MAX_REGISTERS)) return false;
    model->holding_registers[offset] = value;
    return true;
}

bool modbus_write_multiple_coils(modbus_data_model_t *model, uint16_t start_address,
                                 uint16_t quantity, const bool *values)
{
    if (!model || !values || quantity == 0) return false;
    if (start_address + quantity > MODBUS_MAX_COILS) return false;
    for (uint16_t i = 0; i < quantity; i++) {
        size_t offset = modbus_coil_address_to_offset(start_address + i);
        model->coils[offset] = values[i];
    }
    return true;
}

bool modbus_write_multiple_registers(modbus_data_model_t *model, uint16_t start_address,
                                     uint16_t quantity, const uint16_t *values)
{
    if (!model || !values || quantity == 0) return false;
    if (start_address + quantity > MODBUS_MAX_REGISTERS) return false;
    for (uint16_t i = 0; i < quantity; i++) {
        size_t offset = modbus_register_address_to_offset(start_address + i);
        model->holding_registers[offset] = values[i];
    }
    return true;
}

uint16_t modbus_crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        uint8_t index = (crc ^ data[i]) & 0x0F;
        crc = (crc >> 4) ^ crc_table[index];
        index = (crc ^ (data[i] >> 4)) & 0x0F;
        crc = (crc >> 4) ^ crc_table[index];
    }
    return crc;
}

bool modbus_verify_crc(const uint8_t *data, size_t length)
{
    if (length < 2) return false;
    uint16_t computed = modbus_crc16(data, length - 2);
    uint16_t received = (uint16_t)(data[length - 1] << 8) | data[length - 2];
    return computed == received;
}

int modbus_build_request_rtu(uint8_t *buffer, size_t buf_size,
                             const modbus_request_t *req)
{
    if (!buffer || !req || buf_size < 8) return -1;

    buffer[0] = req->slave_address;
    buffer[1] = req->function_code;
    buffer[2] = (uint8_t)(req->start_address >> 8);
    buffer[3] = (uint8_t)(req->start_address & 0xFF);
    buffer[4] = (uint8_t)(req->quantity >> 8);
    buffer[5] = (uint8_t)(req->quantity & 0xFF);

    uint16_t crc = modbus_crc16(buffer, 6);
    buffer[6] = (uint8_t)(crc & 0xFF);
    buffer[7] = (uint8_t)(crc >> 8);

    return 8;
}

int modbus_build_request_tcp(uint8_t *buffer, size_t buf_size,
                             uint16_t transaction_id, const modbus_request_t *req)
{
    if (!buffer || !req || buf_size < 12) return -1;

    int header_len = modbus_build_mbap_header(buffer, transaction_id, 6, req->slave_address);
    if (header_len < 0) return -1;

    buffer[header_len + 0] = req->function_code;
    buffer[header_len + 1] = (uint8_t)(req->start_address >> 8);
    buffer[header_len + 2] = (uint8_t)(req->start_address & 0xFF);
    buffer[header_len + 3] = (uint8_t)(req->quantity >> 8);
    buffer[header_len + 4] = (uint8_t)(req->quantity & 0xFF);

    return header_len + 5;
}

int modbus_parse_response_rtu(const uint8_t *buffer, size_t length,
                              modbus_response_t *resp)
{
    if (!buffer || !resp || length < 4) return -1;

    resp->slave_address = buffer[0];
    resp->function_code = buffer[1];

    if (modbus_is_exception(resp)) {
        return 3;
    }

    resp->byte_count = buffer[2];
    if (length < (size_t)(3 + resp->byte_count + 2)) return -1;

    memcpy(resp->data, buffer + 3, resp->byte_count);

    if (!modbus_verify_crc(buffer, length)) return -2;

    return 3 + resp->byte_count + 2;
}

int modbus_parse_response_tcp(const uint8_t *buffer, size_t length,
                              modbus_response_t *resp)
{
    if (!buffer || !resp || length < 9) return -1;

    modbus_mbap_header_t header;
    int header_len = modbus_parse_mbap_header(buffer, length, &header);
    if (header_len < 0) return -1;

    resp->slave_address = header.unit_id;
    resp->function_code = buffer[header_len];

    if (modbus_is_exception(resp)) {
        return header_len + 2;
    }

    resp->byte_count = buffer[header_len + 1];
    memcpy(resp->data, buffer + header_len + 2, resp->byte_count);

    return header_len + 2 + resp->byte_count;
}

bool modbus_is_exception(const modbus_response_t *resp)
{
    return (resp->function_code & 0x80) != 0;
}

int modbus_parse_exception(const uint8_t *buffer, size_t length,
                           modbus_exception_response_t *exc)
{
    if (!buffer || !exc || length < 3) return -1;
    exc->slave_address = buffer[0];
    exc->function_code = buffer[1];
    exc->exception_code = buffer[2];
    return 3;
}

int modbus_build_mbap_header(uint8_t *buffer, uint16_t transaction_id,
                             uint16_t length, uint8_t unit_id)
{
    if (!buffer) return -1;
    buffer[0] = (uint8_t)(transaction_id >> 8);
    buffer[1] = (uint8_t)(transaction_id & 0xFF);
    buffer[2] = 0x00;
    buffer[3] = 0x00;
    buffer[4] = (uint8_t)(length >> 8);
    buffer[5] = (uint8_t)(length & 0xFF);
    buffer[6] = unit_id;
    return 7;
}

int modbus_parse_mbap_header(const uint8_t *buffer, size_t length,
                             modbus_mbap_header_t *header)
{
    if (!buffer || !header || length < 7) return -1;
    header->transaction_id = ((uint16_t)buffer[0] << 8) | buffer[1];
    header->protocol_id    = ((uint16_t)buffer[2] << 8) | buffer[3];
    header->length         = ((uint16_t)buffer[4] << 8) | buffer[5];
    header->unit_id        = buffer[6];
    return 7;
}

size_t modbus_coil_address_to_offset(uint16_t address)
{
    return (size_t)(address - 1);
}

size_t modbus_register_address_to_offset(uint16_t address)
{
    return (size_t)(address - 1);
}

bool modbus_validate_address(uint16_t address, uint16_t max_count)
{
    return address >= 1 && address <= max_count;
}

const char* modbus_exception_string(modbus_exception_code_t code)
{
    switch (code) {
    case MODBUS_EXC_NONE:                     return "No exception";
    case MODBUS_EXC_ILLEGAL_FUNCTION:         return "Illegal function";
    case MODBUS_EXC_ILLEGAL_DATA_ADDRESS:     return "Illegal data address";
    case MODBUS_EXC_ILLEGAL_DATA_VALUE:       return "Illegal data value";
    case MODBUS_EXC_SLAVE_DEVICE_FAILURE:     return "Slave device failure";
    case MODBUS_EXC_ACKNOWLEDGE:              return "Acknowledge";
    case MODBUS_EXC_SLAVE_DEVICE_BUSY:        return "Slave device busy";
    case MODBUS_EXC_MEMORY_PARITY_ERROR:      return "Memory parity error";
    case MODBUS_EXC_GATEWAY_PATH_UNAVAILABLE: return "Gateway path unavailable";
    case MODBUS_EXC_GATEWAY_TARGET_FAILED:    return "Gateway target failed";
    default:                                   return "Unknown exception";
    }
}

const char* modbus_function_code_string(modbus_function_code_t code)
{
    switch (code) {
    case MODBUS_FC_READ_COILS:               return "Read Coils";
    case MODBUS_FC_READ_DISCRETE_INPUTS:     return "Read Discrete Inputs";
    case MODBUS_FC_READ_HOLDING_REGISTERS:   return "Read Holding Registers";
    case MODBUS_FC_READ_INPUT_REGISTERS:     return "Read Input Registers";
    case MODBUS_FC_WRITE_SINGLE_COIL:        return "Write Single Coil";
    case MODBUS_FC_WRITE_SINGLE_REGISTER:    return "Write Single Register";
    case MODBUS_FC_WRITE_MULTIPLE_COILS:     return "Write Multiple Coils";
    case MODBUS_FC_WRITE_MULTIPLE_REGISTERS: return "Write Multiple Registers";
    default:                                 return "Unknown function code";
    }
}
