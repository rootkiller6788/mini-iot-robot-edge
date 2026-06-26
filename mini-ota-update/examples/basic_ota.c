#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ab_update.h"
#include "signed_image.h"
#include "ota_client.h"

static char flash_buffer[8 * 1024 * 1024];

static void *mock_flash_read(uint32_t addr, size_t len, void *ctx)
{
    (void)ctx;
    if (addr + len > sizeof(flash_buffer)) return NULL;
    void *buf = malloc(len);
    if (buf) memcpy(buf, flash_buffer + addr, len);
    return buf;
}

static int mock_flash_write(uint32_t addr, const void *data, size_t len,
                             void *ctx)
{
    (void)ctx;
    if (addr + len > sizeof(flash_buffer)) return -1;
    memcpy(flash_buffer + addr, data, len);
    return 0;
}

static int mock_flash_erase(uint32_t addr, size_t len, void *ctx)
{
    (void)ctx;
    if (addr + len > sizeof(flash_buffer)) return -1;
    memset(flash_buffer + addr, 0xFF, len);
    return 0;
}

static uint32_t mock_get_boot_count(void *ctx)
{
    (void)ctx;
    return 0;
}

static int mock_set_boot_count(uint32_t count, void *ctx)
{
    (void)ctx;
    (void)count;
    return 0;
}

static int mock_reset_boot_count(void *ctx)
{
    (void)ctx;
    return 0;
}

static void mock_reboot(int slot, void *ctx)
{
    (void)ctx;
    printf("[BOOT] Rebooting into slot %d...\n", slot);
}

static uint32_t mock_get_time(void *ctx)
{
    (void)ctx;
    return 1711059200;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("=== Basic OTA Update Demo ===\n\n");

    memset(flash_buffer, 0xFF, sizeof(flash_buffer));

    ab_update_hal_t hal = {
        .flash_read = mock_flash_read,
        .flash_write = mock_flash_write,
        .flash_erase = mock_flash_erase,
        .get_boot_count = mock_get_boot_count,
        .set_boot_count = mock_set_boot_count,
        .reset_boot_count = mock_reset_boot_count,
        .reboot = mock_reboot,
        .get_current_time = mock_get_time,
        .ctx = NULL
    };

    ab_update_ctx_t *ab = ab_update_init(&hal);
    if (!ab) {
        fprintf(stderr, "Failed to init A/B update\n");
        return 1;
    }

    printf("1. Initializing A/B partition...\n");
    ab_result_t res = ab_update_partition_init(ab, sizeof(flash_buffer));
    printf("   Result: %s\n", ab_update_result_str(res));

    res = ab_update_partition_load(ab);
    printf("2. Loading partition table: %s\n", ab_update_result_str(res));

    uint8_t active_slot, standby_slot;
    ab_update_get_active_slot(ab, &active_slot);
    ab_update_get_standby_slot(ab, &standby_slot);
    printf("3. Active: %d, Standby: %d\n", active_slot, standby_slot);

    uint8_t slot_id;
    ab_update_select_boot_slot(ab, &slot_id);
    printf("4. Boot slot selected: %d\n", slot_id);

    ab_update_mark_boot_successful(ab);
    printf("5. Boot marked successful\n");

    const char *v1 = "v1.0.0-FIRMWARE_DATA_";
    ab_update_write_to_standby(ab, (const uint8_t *)v1, strlen(v1), 0);
    printf("6. Written v1.0.0 to standby slot\n");

    uint8_t expected_hash[AB_UPDATE_HASH_SIZE] = {0};
    for (int i = 0; i < (int)strlen(v1); i++)
        expected_hash[i % AB_UPDATE_HASH_SIZE] ^= v1[i];

    res = ab_update_verify_standby(ab, expected_hash);
    printf("7. Verify standby: %s\n", ab_update_result_str(res));

    ab_update_set_standby_bootable(ab, 1, 0, 0);
    printf("8. Standby set bootable\n");

    bool exceeded;
    ab_update_check_boot_limit(ab, &exceeded);
    printf("9. Boot limit exceeded: %s\n", exceeded ? "yes" : "no");

    printf("\n10. Switching to new firmware and rebooting...\n");
    ab_update_switch_and_reboot(ab);

    ab_update_deinit(ab);
    printf("\n=== Demo complete ===\n");
    return 0;
}
