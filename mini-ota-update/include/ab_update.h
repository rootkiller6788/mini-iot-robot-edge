#ifndef AB_UPDATE_H
#define AB_UPDATE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AB_UPDATE_MAX_SLOTS         2
#define AB_UPDATE_SLOT_NAME_MAX     32
#define AB_UPDATE_VERSION_MAX       64
#define AB_UPDATE_HASH_SIZE         32
#define AB_UPDATE_BOOT_COUNT_MAX    7
#define AB_UPDATE_SIGNATURE_SIZE    64

typedef enum {
    AB_SLOT_STATE_UNKNOWN = 0,
    AB_SLOT_STATE_INACTIVE,
    AB_SLOT_STATE_BOOTING,
    AB_SLOT_STATE_ACTIVE,
    AB_SLOT_STATE_INVALID
} ab_slot_state_t;

typedef enum {
    AB_RESULT_OK = 0,
    AB_RESULT_ERR_PARAM,
    AB_RESULT_ERR_NOMEM,
    AB_RESULT_ERR_IO,
    AB_RESULT_ERR_CRC,
    AB_RESULT_ERR_SIGNATURE,
    AB_RESULT_ERR_ROLLBACK,
    AB_RESULT_ERR_BOOTLIMIT,
    AB_RESULT_ERR_SLOT_FULL,
    AB_RESULT_ERR_NOT_FOUND,
    AB_RESULT_ERR_STATE
} ab_result_t;

#pragma pack(push, 1)
typedef struct {
    uint8_t slot_id;
    char slot_name[AB_UPDATE_SLOT_NAME_MAX];
    uint32_t slot_offset;
    uint32_t slot_size;
    ab_slot_state_t state;
    uint8_t priority;
    uint8_t bootable;
    uint8_t boot_successful;
    uint8_t boot_attempts;
    uint8_t boot_count_limit;
    uint32_t version_major;
    uint32_t version_minor;
    uint32_t version_patch;
    uint8_t version_extra[16];
    uint8_t image_hash[AB_UPDATE_HASH_SIZE];
    uint32_t image_size;
    uint32_t image_crc;
    uint32_t last_boot_time;
    uint32_t update_timestamp;
    uint8_t reserved[16];
} ab_slot_metadata_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint32_t total_size;
    uint8_t num_slots;
    uint8_t active_slot;
    uint8_t boot_slot;
    uint8_t reserved;
    uint32_t crc32;
    ab_slot_metadata_t slots[AB_UPDATE_MAX_SLOTS];
} ab_partition_header_t;
#pragma pack(pop)

typedef struct ab_update_ctx ab_update_ctx_t;

typedef struct {
    void *(*flash_read)(uint32_t addr, size_t len, void *ctx);
    int (*flash_write)(uint32_t addr, const void *data, size_t len, void *ctx);
    int (*flash_erase)(uint32_t addr, size_t len, void *ctx);
    uint32_t (*get_boot_count)(void *ctx);
    int (*set_boot_count)(uint32_t count, void *ctx);
    int (*reset_boot_count)(void *ctx);
    void (*reboot)(int slot, void *ctx);
    uint32_t (*get_current_time)(void *ctx);
    void *ctx;
} ab_update_hal_t;

ab_update_ctx_t *ab_update_init(const ab_update_hal_t *hal);
void ab_update_deinit(ab_update_ctx_t *ctx);

ab_result_t ab_update_partition_init(ab_update_ctx_t *ctx, uint32_t total_size);
ab_result_t ab_update_partition_load(ab_update_ctx_t *ctx);
ab_result_t ab_update_partition_save(ab_update_ctx_t *ctx);

ab_result_t ab_update_get_active_slot(ab_update_ctx_t *ctx, uint8_t *slot_id);
ab_result_t ab_update_get_standby_slot(ab_update_ctx_t *ctx, uint8_t *slot_id);
ab_result_t ab_update_get_slot_metadata(ab_update_ctx_t *ctx, uint8_t slot_id, ab_slot_metadata_t *meta);

ab_result_t ab_update_select_boot_slot(ab_update_ctx_t *ctx, uint8_t *selected);
ab_result_t ab_update_mark_boot_successful(ab_update_ctx_t *ctx);
ab_result_t ab_update_mark_boot_failed(ab_update_ctx_t *ctx);

ab_result_t ab_update_write_to_standby(ab_update_ctx_t *ctx, const uint8_t *data, size_t len, uint32_t offset);
ab_result_t ab_update_verify_standby(ab_update_ctx_t *ctx, const uint8_t *expected_hash);
ab_result_t ab_update_set_standby_bootable(ab_update_ctx_t *ctx, uint32_t version_major, uint32_t version_minor, uint32_t version_patch);
ab_result_t ab_update_switch_and_reboot(ab_update_ctx_t *ctx);

ab_result_t ab_update_rollback(ab_update_ctx_t *ctx);
ab_result_t ab_update_check_boot_limit(ab_update_ctx_t *ctx, bool *exceeded);

const char *ab_update_result_str(ab_result_t result);

#ifdef __cplusplus
}
#endif

#endif
