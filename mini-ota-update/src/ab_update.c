#include "ab_update.h"
#include <string.h>

struct ab_update_ctx {
    ab_update_hal_t hal;
    ab_partition_header_t partition;
    bool initialized;
    bool partition_loaded;
    uint8_t active_slot_cache;
    uint8_t standby_slot_cache;
    uint32_t boot_failure_count;
    bool boot_marked;
};

static uint32_t ab_update_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
            else crc >>= 1;
        }
    }
    return crc ^ 0xFFFFFFFF;
}

ab_update_ctx_t *ab_update_init(const ab_update_hal_t *hal)
{
    if (!hal) return NULL;
    ab_update_ctx_t *ctx = (ab_update_ctx_t *)hal->flash_read
        ? NULL : NULL;
    ctx = (ab_update_ctx_t *)malloc(sizeof(ab_update_ctx_t));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(ab_update_ctx_t));
    memcpy(&ctx->hal, hal, sizeof(ab_update_hal_t));
    ctx->initialized = true;
    return ctx;
}

void ab_update_deinit(ab_update_ctx_t *ctx)
{
    if (!ctx) return;
    free(ctx);
}

ab_result_t ab_update_partition_init(ab_update_ctx_t *ctx, uint32_t total_size)
{
    if (!ctx || !ctx->initialized || total_size == 0)
        return AB_RESULT_ERR_PARAM;

    memset(&ctx->partition, 0, sizeof(ab_partition_header_t));
    ctx->partition.magic = 0x41425550;
    ctx->partition.version = 1;
    ctx->partition.header_size = sizeof(ab_partition_header_t);
    ctx->partition.total_size = total_size;
    ctx->partition.num_slots = AB_UPDATE_MAX_SLOTS;

    uint32_t slot_size = (total_size - ctx->partition.header_size)
                         / AB_UPDATE_MAX_SLOTS;

    for (int i = 0; i < AB_UPDATE_MAX_SLOTS; i++) {
        ab_slot_metadata_t *slot = &ctx->partition.slots[i];
        slot->slot_id = i;
        snprintf(slot->slot_name, AB_UPDATE_SLOT_NAME_MAX, "slot_%c",
                 (char)('A' + i));
        slot->slot_offset = ctx->partition.header_size + (i * slot_size);
        slot->slot_size = slot_size;
        slot->state = AB_SLOT_STATE_INACTIVE;
        slot->priority = (uint8_t)i;
        slot->bootable = 0;
        slot->boot_successful = 0;
        slot->boot_attempts = 0;
        slot->boot_count_limit = AB_UPDATE_BOOT_COUNT_MAX;
    }

    ctx->partition.slots[0].state = AB_SLOT_STATE_ACTIVE;
    ctx->partition.slots[0].bootable = 1;
    ctx->partition.active_slot = 0;
    ctx->partition.boot_slot = 0;

    return ab_update_partition_save(ctx);
}

ab_result_t ab_update_partition_load(ab_update_ctx_t *ctx)
{
    if (!ctx || !ctx->initialized) return AB_RESULT_ERR_PARAM;

    ab_partition_header_t *hdr = &ctx->partition;
    size_t read_size = sizeof(ab_partition_header_t);
    void *data = ctx->hal.flash_read(0, read_size, ctx->hal.ctx);

    if (!data) return AB_RESULT_ERR_IO;
    memcpy(hdr, data, read_size);

    uint32_t calc_crc = ab_update_crc32((const uint8_t *)hdr,
                                         sizeof(ab_partition_header_t) - 4);
    if (calc_crc != hdr->crc32) return AB_RESULT_ERR_CRC;
    if (hdr->magic != 0x41425550) return AB_RESULT_ERR_CRC;
    if (hdr->version < 1) return AB_RESULT_ERR_CRC;

    ctx->partition_loaded = true;
    ctx->active_slot_cache = hdr->active_slot;
    ctx->standby_slot_cache = (uint8_t)((hdr->active_slot + 1) % AB_UPDATE_MAX_SLOTS);

    return AB_RESULT_OK;
}

ab_result_t ab_update_partition_save(ab_update_ctx_t *ctx)
{
    if (!ctx || !ctx->initialized) return AB_RESULT_ERR_PARAM;

    ab_partition_header_t *hdr = &ctx->partition;
    hdr->crc32 = ab_update_crc32((const uint8_t *)hdr,
                                  sizeof(ab_partition_header_t) - 4);
    int rc = ctx->hal.flash_write(0, hdr, sizeof(ab_partition_header_t),
                                   ctx->hal.ctx);
    if (rc != 0) return AB_RESULT_ERR_IO;
    return AB_RESULT_OK;
}

ab_result_t ab_update_get_active_slot(ab_update_ctx_t *ctx, uint8_t *slot_id)
{
    if (!ctx || !slot_id || !ctx->partition_loaded)
        return AB_RESULT_ERR_PARAM;
    *slot_id = ctx->active_slot_cache;
    return AB_RESULT_OK;
}

ab_result_t ab_update_get_standby_slot(ab_update_ctx_t *ctx, uint8_t *slot_id)
{
    if (!ctx || !slot_id || !ctx->partition_loaded)
        return AB_RESULT_ERR_PARAM;
    *slot_id = ctx->standby_slot_cache;
    return AB_RESULT_OK;
}

ab_result_t ab_update_get_slot_metadata(ab_update_ctx_t *ctx,
    uint8_t slot_id, ab_slot_metadata_t *meta)
{
    if (!ctx || !meta) return AB_RESULT_ERR_PARAM;
    if (slot_id >= AB_UPDATE_MAX_SLOTS) return AB_RESULT_ERR_PARAM;
    memcpy(meta, &ctx->partition.slots[slot_id], sizeof(ab_slot_metadata_t));
    return AB_RESULT_OK;
}

ab_result_t ab_update_select_boot_slot(ab_update_ctx_t *ctx,
                                        uint8_t *selected)
{
    if (!ctx || !selected || !ctx->partition_loaded)
        return AB_RESULT_ERR_PARAM;

    ab_partition_header_t *hdr = &ctx->partition;
    ab_slot_metadata_t *slot_a = &hdr->slots[0];
    ab_slot_metadata_t *slot_b = &hdr->slots[1];
    bool a_bootable = slot_a->bootable && slot_a->state == AB_SLOT_STATE_ACTIVE;
    bool b_bootable = slot_b->bootable && slot_b->state == AB_SLOT_STATE_ACTIVE;
    uint8_t chosen = hdr->active_slot;

    if (!a_bootable && !b_bootable) {
        chosen = 0;
        slot_a->boot_attempts = 0;
        slot_a->state = AB_SLOT_STATE_ACTIVE;
    } else if (a_bootable && !b_bootable) {
        chosen = 0;
    } else if (!a_bootable && b_bootable) {
        chosen = 1;
    } else {
        if (slot_a->priority > slot_b->priority)
            chosen = 0;
        else if (slot_b->priority > slot_a->priority)
            chosen = 1;
        else if (slot_a->version_major > slot_b->version_major)
            chosen = 0;
        else if (slot_b->version_major > slot_a->version_major)
            chosen = 1;
        else
            chosen = 0;
    }

    ab_slot_metadata_t *chosen_slot = &hdr->slots[chosen];
    chosen_slot->boot_attempts++;
    chosen_slot->state = AB_SLOT_STATE_BOOTING;
    chosen_slot->last_boot_time = ctx->hal.get_current_time
        ? ctx->hal.get_current_time(ctx->hal.ctx) : 0;

    hdr->boot_slot = chosen;
    *selected = chosen;
    return ab_update_partition_save(ctx);
}

ab_result_t ab_update_mark_boot_successful(ab_update_ctx_t *ctx)
{
    if (!ctx || !ctx->partition_loaded) return AB_RESULT_ERR_PARAM;

    ab_partition_header_t *hdr = &ctx->partition;
    uint8_t boot = hdr->boot_slot;
    ab_slot_metadata_t *slot = &hdr->slots[boot];

    slot->state = AB_SLOT_STATE_ACTIVE;
    slot->boot_successful = 1;
    slot->boot_attempts = 0;
    hdr->active_slot = boot;
    ctx->active_slot_cache = boot;
    ctx->standby_slot_cache = (uint8_t)((boot + 1) % AB_UPDATE_MAX_SLOTS);
    ctx->boot_marked = true;

    if (ctx->hal.reset_boot_count)
        ctx->hal.reset_boot_count(ctx->hal.ctx);

    return ab_update_partition_save(ctx);
}

ab_result_t ab_update_mark_boot_failed(ab_update_ctx_t *ctx)
{
    if (!ctx || !ctx->partition_loaded) return AB_RESULT_ERR_PARAM;

    ab_partition_header_t *hdr = &ctx->partition;
    uint8_t boot = hdr->boot_slot;
    ab_slot_metadata_t *slot = &hdr->slots[boot];
    slot->state = AB_SLOT_STATE_INVALID;
    slot->boot_successful = 0;

    return ab_update_partition_save(ctx);
}

ab_result_t ab_update_write_to_standby(ab_update_ctx_t *ctx,
    const uint8_t *data, size_t len, uint32_t offset)
{
    if (!ctx || !data || len == 0) return AB_RESULT_ERR_PARAM;
    ab_slot_metadata_t *slot = &ctx->partition.slots[ctx->standby_slot_cache];
    uint32_t write_offset = slot->slot_offset + offset;

    if (offset + len > slot->slot_size) return AB_RESULT_ERR_SLOT_FULL;
    int rc = ctx->hal.flash_write(write_offset, data, len, ctx->hal.ctx);
    if (rc != 0) return AB_RESULT_ERR_IO;

    return AB_RESULT_OK;
}

ab_result_t ab_update_verify_standby(ab_update_ctx_t *ctx,
                                      const uint8_t *expected_hash)
{
    if (!ctx || !expected_hash || !ctx->hal.flash_read)
        return AB_RESULT_ERR_PARAM;

    ab_slot_metadata_t *slot = &ctx->partition.slots[ctx->standby_slot_cache];
    uint8_t calc_hash[AB_UPDATE_HASH_SIZE];
    memset(calc_hash, 0, AB_UPDATE_HASH_SIZE);

    uint32_t remaining = slot->image_size;
    uint32_t offset = 0;
    uint8_t buf[256];

    while (remaining > 0) {
        size_t chunk = remaining < sizeof(buf) ? remaining : sizeof(buf);
        void *data = ctx->hal.flash_read(slot->slot_offset + offset, chunk,
                                          ctx->hal.ctx);
        if (!data) return AB_RESULT_ERR_IO;
        for (size_t i = 0; i < chunk; i++)
            calc_hash[i % AB_UPDATE_HASH_SIZE] ^= ((uint8_t *)data)[i];
        offset += chunk;
        remaining -= chunk;
    }

    if (memcmp(calc_hash, expected_hash, AB_UPDATE_HASH_SIZE) != 0)
        return AB_RESULT_ERR_CRC;

    return AB_RESULT_OK;
}

ab_result_t ab_update_set_standby_bootable(ab_update_ctx_t *ctx,
    uint32_t version_major, uint32_t version_minor, uint32_t version_patch)
{
    if (!ctx || !ctx->partition_loaded) return AB_RESULT_ERR_PARAM;

    ab_slot_metadata_t *slot = &ctx->partition.slots[ctx->standby_slot_cache];
    slot->bootable = 1;
    slot->state = AB_SLOT_STATE_INACTIVE;
    slot->version_major = version_major;
    slot->version_minor = version_minor;
    slot->version_patch = version_patch;
    slot->update_timestamp = ctx->hal.get_current_time
        ? ctx->hal.get_current_time(ctx->hal.ctx) : 0;

    ctx->partition.slots[ctx->active_slot_cache].state = AB_SLOT_STATE_INACTIVE;
    ctx->partition.slots[ctx->active_slot_cache].bootable = 0;

    return ab_update_partition_save(ctx);
}

ab_result_t ab_update_switch_and_reboot(ab_update_ctx_t *ctx)
{
    if (!ctx || !ctx->partition_loaded) return AB_RESULT_ERR_PARAM;

    ab_partition_header_t *hdr = &ctx->partition;
    uint8_t new_active = ctx->standby_slot_cache;
    hdr->active_slot = new_active;
    hdr->slots[new_active].state = AB_SLOT_STATE_ACTIVE;

    ab_result_t save_res = ab_update_partition_save(ctx);
    if (save_res != AB_RESULT_OK) return save_res;

    if (ctx->hal.reboot)
        ctx->hal.reboot(new_active, ctx->hal.ctx);

    return AB_RESULT_OK;
}

ab_result_t ab_update_rollback(ab_update_ctx_t *ctx)
{
    if (!ctx || !ctx->partition_loaded) return AB_RESULT_ERR_PARAM;

    ab_partition_header_t *hdr = &ctx->partition;
    uint8_t current_active = hdr->active_slot;
    uint8_t previous = (uint8_t)((current_active + 1) % AB_UPDATE_MAX_SLOTS);

    hdr->slots[current_active].state = AB_SLOT_STATE_INVALID;
    hdr->slots[current_active].bootable = 0;
    hdr->slots[previous].state = AB_SLOT_STATE_ACTIVE;
    hdr->slots[previous].bootable = 1;
    hdr->active_slot = previous;

    ctx->active_slot_cache = previous;
    ctx->standby_slot_cache = current_active;

    return ab_update_partition_save(ctx);
}

ab_result_t ab_update_check_boot_limit(ab_update_ctx_t *ctx, bool *exceeded)
{
    if (!ctx || !exceeded || !ctx->partition_loaded)
        return AB_RESULT_ERR_PARAM;

    uint32_t boot_count = ctx->hal.get_boot_count
        ? ctx->hal.get_boot_count(ctx->hal.ctx) : 0;
    *exceeded = (boot_count > AB_UPDATE_BOOT_COUNT_MAX);
    return AB_RESULT_OK;
}

const char *ab_update_result_str(ab_result_t result)
{
    switch (result) {
    case AB_RESULT_OK:             return "OK";
    case AB_RESULT_ERR_PARAM:      return "Invalid parameter";
    case AB_RESULT_ERR_NOMEM:      return "No memory";
    case AB_RESULT_ERR_IO:         return "I/O error";
    case AB_RESULT_ERR_CRC:        return "CRC mismatch";
    case AB_RESULT_ERR_SIGNATURE:  return "Signature error";
    case AB_RESULT_ERR_ROLLBACK:   return "Rollback error";
    case AB_RESULT_ERR_BOOTLIMIT:  return "Boot count limit exceeded";
    case AB_RESULT_ERR_SLOT_FULL:  return "Slot full";
    case AB_RESULT_ERR_NOT_FOUND:  return "Not found";
    case AB_RESULT_ERR_STATE:      return "Invalid state";
    default:                       return "Unknown";
    }
}
