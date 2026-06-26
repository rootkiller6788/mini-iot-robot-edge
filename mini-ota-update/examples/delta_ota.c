#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "delta_update.h"

#define DELTA_MAX_BUF (1024 * 1024)

static void *delta_malloc(size_t size, void *ctx)
{
    (void)ctx;
    return malloc(size);
}

static void delta_free_void(void *ptr, void *ctx)
{
    (void)ctx;
    free(ptr);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("=== Delta OTA Update Demo ===\n\n");

    const char *old_fw =
        "Firmware v1.0.0\n"
        "void init(void) { UART_init(115200); GPIO_init(); }\n"
        "void main_loop(void) { read_sensors(); process_data(); transmit(); }\n"
        "void read_sensors(void) { temp = adc_read(TEMP_PIN); }\n"
        "void process_data(void) { avg = temp / 100; }\n"
        "void transmit(void) { UART_send(avg); }\n"
        "int config_value = 42;\n"
        "char device_name[] = \"Sensor-001\";\n";

    const char *new_fw =
        "Firmware v2.0.0\n"
        "void init(void) { UART_init(115200); GPIO_init(); WIFI_init(); }\n"
        "void main_loop(void) { read_sensors(); process_data(); transmit(); "
        "read_all_sensors(); send_telemetry(); }\n"
        "void read_sensors(void) { temp = adc_read(TEMP_PIN); "
        "humidity = adc_read(HUM_PIN); }\n"
        "void process_data(void) { avg_temp = temp / 100; "
        "avg_hum = humidity / 100; }\n"
        "void transmit(void) { UART_send(avg_temp); UART_send(avg_hum); }\n"
        "void read_all_sensors(void) { read_sensors(); }\n"
        "void send_telemetry(void) { wifi_send_json(avg_temp, avg_hum); }\n"
        "int config_value = 99;\n"
        "char device_name[] = \"Sensor-002\";\n";

    size_t old_len = strlen(old_fw);
    size_t new_len = strlen(new_fw);

    printf("1. Old firmware (%zu bytes):\n   ", old_len);
    for (int i = 0; i < 4; i++) {
        const char *p = old_fw;
        for (int j = 0; j < i; j++) p = strchr(p, '\n') + 1;
        printf("%.30s\n   ", p);
    }
    printf("...\n");

    printf("\n2. New firmware (%zu bytes):\n   ", new_len);
    for (int i = 0; i < 4; i++) {
        const char *p = new_fw;
        for (int j = 0; j < i; j++) p = strchr(p, '\n') + 1;
        printf("%.30s\n   ", p);
    }
    printf("...\n");

    uint8_t *patch = NULL;
    size_t patch_size = 0;

    printf("\n3. Creating delta patch...\n");
    delta_result_t res = delta_create_patch(
        (const uint8_t *)old_fw, old_len,
        (const uint8_t *)new_fw, new_len,
        &patch, &patch_size, NULL, NULL, NULL);

    printf("   Result: %s\n", delta_result_str(res));
    printf("   Original new: %zu bytes\n", new_len);
    printf("   Delta patch:  %zu bytes\n", patch_size);
    if (new_len > 0)
        printf("   Savings:      %.1f%%\n",
               100.0 * (1.0 - (double)patch_size / (double)new_len));

    uint8_t *reconstructed = (uint8_t *)malloc(new_len + 1);
    memset(reconstructed, 0, new_len + 1);

    delta_io_t io;
    memset(&io, 0, sizeof(io));
    io.ctx = NULL;

    uint8_t patch_buf[DELTA_MAX_BUF];
    uint8_t new_buf[DELTA_MAX_BUF];
    size_t read_patch_offset = 0;
    size_t read_old_offset = 0;
    size_t new_write_offset = 0;

    (void)patch_buf;
    (void)new_buf;
    (void)read_patch_offset;
    (void)read_old_offset;
    (void)new_write_offset;

    printf("\n4. Patch format:\n");
    delta_patch_header_t *hdr = (delta_patch_header_t *)patch;
    printf("   Magic: 0x%08X\n", hdr->magic);
    printf("   Version: %u\n", hdr->version);
    printf("   Old size: %u bytes\n", hdr->old_file_size);
    printf("   New size: %u bytes\n", hdr->new_file_size);
    printf("   Chunks: %u\n", hdr->chunk_count);

    printf("\n5. Simulating patch application...\n");
    size_t pi = sizeof(delta_patch_header_t);
    size_t ni = 0;
    int op_count = 0;

    while (pi < sizeof(delta_patch_header_t) + hdr->patch_data_size
           && ni < new_len) {
        uint8_t op = patch[pi++];
        if (op == DELTA_OP_ADD) {
            uint32_t len = patch[pi] | ((uint32_t)patch[pi + 1] << 8);
            pi += 2;
            memcpy(reconstructed + ni, patch + pi, len);
            pi += len;
            ni += len;
            op_count++;
            printf("   ADD %u bytes at offset %zu\n", len, ni - len);
        } else if (op == DELTA_OP_COPY) {
            uint32_t len = patch[pi] | ((uint32_t)patch[pi + 1] << 8);
            pi += 2;
            uint32_t pos = patch[pi] | ((uint32_t)patch[pi + 1] << 8) |
                          ((uint32_t)patch[pi + 2] << 16) |
                          ((uint32_t)patch[pi + 3] << 24);
            pi += 4;
            memcpy(reconstructed + ni, old_fw + pos, len);
            ni += len;
            op_count++;
            printf("   COPY %u bytes from offset %u to %zu\n", len, pos, ni - len);
        } else if (op == DELTA_OP_SEEK) {
            printf("   SEEK marker\n");
            break;
        }
    }
    reconstructed[ni] = '\0';

    printf("\n6. Total operations: %d\n", op_count);

    if (strcmp(reconstructed, new_fw) == 0) {
        printf("7. Reconstructed firmware MATCHES expected output.\n");
    } else {
        printf("7. MISMATCH! Reconstructed differs from expected.\n");
        printf("   Expected first line: %.50s\n", new_fw);
        printf("   Got first line:      %.50s\n", reconstructed);
    }

    printf("\n8. Compression test:\n");
    size_t comp_size = 4096;
    uint8_t comp_buf[4096];
    delta_compress((const uint8_t *)new_fw, new_len, comp_buf, &comp_size);
    printf("   Original: %zu bytes, Compressed: %zu bytes (%.1f%%)\n",
           new_len, comp_size, new_len > 0
               ? 100.0 * (double)comp_size / (double)new_len : 0.0);

    free(reconstructed);
    free(patch);

    printf("\n=== Delta demo complete ===\n");
    return 0;
}
