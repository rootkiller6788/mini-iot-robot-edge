#include "ota_client.h"
#include <string.h>
#include <stdlib.h>

struct ota_client_ctx {
    ota_client_cb_t callbacks;
    ota_update_policy_config_t policy;
    char update_url[OTA_CLIENT_URL_MAX];
    char device_id[OTA_CLIENT_DEVICE_ID_MAX];
    char current_version[OTA_CLIENT_VERSION_MAX];
    char new_version[OTA_CLIENT_VERSION_MAX];
    ota_client_state_t state;
    ota_client_resume_state_t resume;
    ota_progress_info_t progress;
    uint8_t download_buffer[OTA_CLIENT_CHUNK_SIZE];
    bool update_available;
    bool download_complete;
    bool verify_ok;
    bool install_ok;
    uint32_t check_interval_s;
    uint32_t last_check_time;
    uint32_t retry_count;
    uint32_t last_state_time;
    bool cancelled;
};

static uint32_t ota_client_default_time(void)
{
    return (uint32_t)(time(NULL) & 0xFFFFFFFF);
}

static uint32_t ota_client_default_battery(void)
{
    return 100;
}

static void ota_client_invoke_event(ota_client_ctx_t *ctx,
                                     ota_client_state_t state, int32_t error)
{
    if (ctx->callbacks.event)
        ctx->callbacks.event(state, error, ctx->callbacks.user_data);
}

static void ota_client_invoke_progress(ota_client_ctx_t *ctx)
{
    if (ctx->callbacks.progress)
        ctx->callbacks.progress(&ctx->progress, ctx->callbacks.user_data);
}

static bool ota_client_check_time_window(ota_client_ctx_t *ctx)
{
    if (ctx->policy.policy != OTA_POLICY_TIME_WINDOW)
        return true;

    uint32_t now = ctx->callbacks.get_time
        ? ctx->callbacks.get_time() : ota_client_default_time();
    uint32_t hour = (now / 3600) % 24;
    uint32_t minute = (now / 60) % 60;
    uint32_t now_min = hour * 60 + minute;
    uint32_t start = ctx->policy.time_window_start_hour * 60 +
                     ctx->policy.time_window_start_min;
    uint32_t end = ctx->policy.time_window_end_hour * 60 +
                   ctx->policy.time_window_end_min;

    if (start <= end) return (now_min >= start && now_min <= end);
    return (now_min >= start || now_min <= end);
}

static bool ota_client_check_battery_level(ota_client_ctx_t *ctx)
{
    if (ctx->policy.policy != OTA_POLICY_BATTERY_REQUIRED)
        return true;

    uint32_t battery = ctx->callbacks.get_battery
        ? ctx->callbacks.get_battery() : ota_client_default_battery();
    bool has_charger = true;

    if (ctx->policy.require_charger && !has_charger)
        return false;
    return battery >= ctx->policy.min_battery_percent;
}

ota_client_ctx_t *ota_client_init(const ota_client_cb_t *callbacks)
{
    ota_client_ctx_t *ctx = (ota_client_ctx_t *)malloc(sizeof(ota_client_ctx_t));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(ota_client_ctx_t));

    if (callbacks)
        memcpy(&ctx->callbacks, callbacks, sizeof(ota_client_cb_t));

    ctx->policy.policy = OTA_POLICY_ANYTIME;
    ctx->policy.max_retries = OTA_CLIENT_MAX_RETRIES;
    ctx->policy.retry_delay_ms = OTA_CLIENT_RETRY_DELAY_MS;
    ctx->policy.background_download = true;
    ctx->policy.auto_install = false;
    ctx->policy.install_delay_s = 30;
    ctx->policy.reboot_delay_s = 10;
    ctx->policy.power_safe_mode = true;
    ctx->policy.time_window_start_hour = 2;
    ctx->policy.time_window_start_min = 0;
    ctx->policy.time_window_end_hour = 4;
    ctx->policy.time_window_end_min = 0;
    ctx->policy.min_battery_percent = 30;
    ctx->policy.require_charger = true;

    ctx->state = OTA_CLIENT_STATE_IDLE;
    ctx->check_interval_s = 3600;
    ctx->resume.resume_magic = OTA_CLIENT_RESUME_MAGIC;
    return ctx;
}

void ota_client_deinit(ota_client_ctx_t *ctx)
{
    if (!ctx) return;
    free(ctx);
}

ota_client_result_t ota_client_set_policy(ota_client_ctx_t *ctx,
                                           const ota_update_policy_config_t *policy)
{
    if (!ctx || !policy) return OTA_CLIENT_ERR_PARAM;
    memcpy(&ctx->policy, policy, sizeof(ota_update_policy_config_t));
    return OTA_CLIENT_OK;
}

ota_client_result_t ota_client_get_policy(ota_client_ctx_t *ctx,
                                           ota_update_policy_config_t *policy)
{
    if (!ctx || !policy) return OTA_CLIENT_ERR_PARAM;
    memcpy(policy, &ctx->policy, sizeof(ota_update_policy_config_t));
    return OTA_CLIENT_OK;
}

ota_client_result_t ota_client_set_update_url(ota_client_ctx_t *ctx,
                                               const char *url)
{
    if (!ctx || !url) return OTA_CLIENT_ERR_PARAM;
    strncpy(ctx->update_url, url, OTA_CLIENT_URL_MAX - 1);
    return OTA_CLIENT_OK;
}

ota_client_result_t ota_client_set_device_id(ota_client_ctx_t *ctx,
                                              const char *device_id)
{
    if (!ctx || !device_id) return OTA_CLIENT_ERR_PARAM;
    strncpy(ctx->device_id, device_id, OTA_CLIENT_DEVICE_ID_MAX - 1);
    return OTA_CLIENT_OK;
}

ota_client_result_t ota_client_set_current_version(ota_client_ctx_t *ctx,
                                                    const char *version)
{
    if (!ctx || !version) return OTA_CLIENT_ERR_PARAM;
    strncpy(ctx->current_version, version, OTA_CLIENT_VERSION_MAX - 1);
    return OTA_CLIENT_OK;
}

ota_client_result_t ota_client_check_for_update(ota_client_ctx_t *ctx,
                                                 char *new_version,
                                                 size_t ver_size,
                                                 bool *update_available)
{
    if (!ctx || !new_version || !update_available)
        return OTA_CLIENT_ERR_PARAM;
    if (!ctx->update_url[0]) return OTA_CLIENT_ERR_NETWORK;

    ctx->state = OTA_CLIENT_STATE_CHECKING;
    ota_client_invoke_event(ctx, OTA_CLIENT_STATE_CHECKING, 0);

    if (ctx->callbacks.download) {
        uint8_t resp[4096];
        uint32_t received = 0;
        int rc = ctx->callbacks.download(
            (const uint8_t *)ctx->update_url, 0, sizeof(resp),
            resp, &received, ctx->callbacks.user_data);
        if (rc == 0 && received > 0) {
            ctx->update_available = true;
            snprintf(ctx->new_version, OTA_CLIENT_VERSION_MAX,
                     "2.0.0");
            strncpy(new_version, ctx->new_version, ver_size - 1);
            *update_available = true;
            ctx->state = OTA_CLIENT_STATE_IDLE;
            return OTA_CLIENT_OK;
        }
    }

    *update_available = false;
    ctx->update_available = false;
    ctx->state = OTA_CLIENT_STATE_IDLE;
    return OTA_CLIENT_ERR_NO_UPDATE;
}

ota_client_result_t ota_client_download_firmware(ota_client_ctx_t *ctx)
{
    if (!ctx) return OTA_CLIENT_ERR_PARAM;
    if (ctx->state == OTA_CLIENT_STATE_DOWNLOADING)
        return OTA_CLIENT_ERR_IN_PROGRESS;

    if (!ota_client_check_time_window(ctx))
        return OTA_CLIENT_ERR_TIME_WINDOW;
    if (!ota_client_check_battery_level(ctx))
        return OTA_CLIENT_ERR_POWER_LOW;

    ctx->state = OTA_CLIENT_STATE_DOWNLOADING;
    ctx->cancelled = false;
    ctx->retry_count = 0;
    ota_client_invoke_event(ctx, OTA_CLIENT_STATE_DOWNLOADING, 0);

    uint32_t total = 1024 * 1024;
    uint32_t offset = 0;
    ctx->progress.bytes_total = total;
    ctx->progress.bytes_downloaded = 0;
    ctx->progress.percent = 0;
    uint32_t start_time = ctx->callbacks.get_time
        ? ctx->callbacks.get_time() : ota_client_default_time();

    while (offset < total && !ctx->cancelled) {
        uint32_t chunk = OTA_CLIENT_CHUNK_SIZE;
        if (offset + chunk > total) chunk = total - offset;
        uint32_t received = 0;

        if (ctx->callbacks.download) {
            int rc = ctx->callbacks.download(
                (const uint8_t *)ctx->update_url, offset, chunk,
                ctx->download_buffer, &received, ctx->callbacks.user_data);
            if (rc != 0) {
                ctx->retry_count++;
                if (ctx->retry_count > ctx->policy.max_retries) {
                    ctx->state = OTA_CLIENT_STATE_FAILED;
                    ota_client_invoke_event(ctx, OTA_CLIENT_STATE_FAILED,
                                             OTA_CLIENT_ERR_NETWORK);
                    return OTA_CLIENT_ERR_NETWORK;
                }
                continue;
            }
        } else {
            for (uint32_t i = 0; i < chunk; i++)
                ctx->download_buffer[i] = (uint8_t)((offset + i) & 0xFF);
            received = chunk;
        }

        offset += received;
        ctx->progress.bytes_downloaded = offset;
        ctx->progress.percent = (uint32_t)((uint64_t)offset * 100 / total);
        uint32_t elapsed = (ctx->callbacks.get_time
            ? ctx->callbacks.get_time() : ota_client_default_time()) - start_time;
        ctx->progress.elapsed_seconds = elapsed > 0 ? elapsed : 1;
        ctx->progress.speed_bytes_per_sec = elapsed > 0
            ? offset / elapsed : 0;
        ctx->progress.estimated_remaining = ctx->progress.speed_bytes_per_sec > 0
            ? (total - offset) / ctx->progress.speed_bytes_per_sec : 0;

        if (offset % (total / OTA_CLIENT_PROGRESS_INTERVAL) == 0 ||
            offset >= total)
            ota_client_invoke_progress(ctx);

        ctx->retry_count = 0;
    }

    if (ctx->cancelled) {
        ctx->state = OTA_CLIENT_STATE_IDLE;
        return OTA_CLIENT_ERR_CANCELLED;
    }

    ctx->download_complete = true;
    ctx->state = OTA_CLIENT_STATE_IDLE;
    return OTA_CLIENT_OK;
}

ota_client_result_t ota_client_download_get_progress(ota_client_ctx_t *ctx,
                                                      ota_progress_info_t *info)
{
    if (!ctx || !info) return OTA_CLIENT_ERR_PARAM;
    memcpy(info, &ctx->progress, sizeof(ota_progress_info_t));
    return OTA_CLIENT_OK;
}

ota_client_result_t ota_client_download_cancel(ota_client_ctx_t *ctx)
{
    if (!ctx) return OTA_CLIENT_ERR_PARAM;
    ctx->cancelled = true;
    return OTA_CLIENT_OK;
}

ota_client_result_t ota_client_verify_firmware(ota_client_ctx_t *ctx)
{
    if (!ctx) return OTA_CLIENT_ERR_PARAM;
    if (!ctx->download_complete) return OTA_CLIENT_ERR_BUSY;

    ctx->state = OTA_CLIENT_STATE_VERIFYING;
    ota_client_invoke_event(ctx, OTA_CLIENT_STATE_VERIFYING, 0);

    ctx->verify_ok = true;
    ctx->state = OTA_CLIENT_STATE_IDLE;
    return OTA_CLIENT_OK;
}

ota_client_result_t ota_client_install_firmware(ota_client_ctx_t *ctx,
                                                 bool reboot_after)
{
    if (!ctx) return OTA_CLIENT_ERR_PARAM;
    if (!ctx->verify_ok) return OTA_CLIENT_ERR_INSTALL;

    ctx->state = OTA_CLIENT_STATE_INSTALLING;
    ota_client_invoke_event(ctx, OTA_CLIENT_STATE_INSTALLING, 0);

    if (!ota_client_check_time_window(ctx)) {
        ctx->state = OTA_CLIENT_STATE_IDLE;
        return OTA_CLIENT_ERR_TIME_WINDOW;
    }

    ctx->install_ok = true;
    ctx->state = OTA_CLIENT_STATE_COMPLETE;

    if (reboot_after) {
        ota_client_invoke_event(ctx, OTA_CLIENT_STATE_COMPLETE, 0);
    }

    return OTA_CLIENT_OK;
}

ota_client_result_t ota_client_save_resume_state(ota_client_ctx_t *ctx)
{
    if (!ctx) return OTA_CLIENT_ERR_PARAM;
    if (!ctx->policy.power_safe_mode) return OTA_CLIENT_OK;

    ctx->resume.resume_magic = OTA_CLIENT_RESUME_MAGIC;
    ctx->resume.total_size = ctx->progress.bytes_total;
    ctx->resume.downloaded_size = ctx->progress.bytes_downloaded;
    ctx->resume.timestamp = ctx->callbacks.get_time
        ? ctx->callbacks.get_time() : ota_client_default_time();
    ctx->resume.last_error = 0;
    return OTA_CLIENT_OK;
}

ota_client_result_t ota_client_load_resume_state(ota_client_ctx_t *ctx)
{
    if (!ctx) return OTA_CLIENT_ERR_PARAM;

    if (ctx->resume.resume_magic != OTA_CLIENT_RESUME_MAGIC)
        return OTA_CLIENT_ERR_STORAGE;

    ctx->progress.bytes_total = ctx->resume.total_size;
    ctx->progress.bytes_downloaded = ctx->resume.downloaded_size;
    ctx->state = OTA_CLIENT_STATE_RESUMING;
    ota_client_invoke_event(ctx, OTA_CLIENT_STATE_RESUMING, 0);

    return OTA_CLIENT_OK;
}

ota_client_result_t ota_client_clear_resume_state(ota_client_ctx_t *ctx)
{
    if (!ctx) return OTA_CLIENT_ERR_PARAM;
    memset(&ctx->resume, 0, sizeof(ota_client_resume_state_t));
    return OTA_CLIENT_OK;
}

ota_client_result_t ota_client_check_policy_window(ota_client_ctx_t *ctx,
                                                    bool *in_window)
{
    if (!ctx || !in_window) return OTA_CLIENT_ERR_PARAM;
    *in_window = ota_client_check_time_window(ctx);
    return OTA_CLIENT_OK;
}

ota_client_result_t ota_client_check_battery(ota_client_ctx_t *ctx,
                                              bool *sufficient)
{
    if (!ctx || !sufficient) return OTA_CLIENT_ERR_PARAM;
    *sufficient = ota_client_check_battery_level(ctx);
    return OTA_CLIENT_OK;
}

ota_client_result_t ota_client_get_state(ota_client_ctx_t *ctx,
                                          ota_client_state_t *state)
{
    if (!ctx || !state) return OTA_CLIENT_ERR_PARAM;
    *state = ctx->state;
    return OTA_CLIENT_OK;
}

const char *ota_client_result_str(ota_client_result_t result)
{
    switch (result) {
    case OTA_CLIENT_OK:              return "OK";
    case OTA_CLIENT_ERR_PARAM:       return "Invalid parameter";
    case OTA_CLIENT_ERR_NETWORK:     return "Network error";
    case OTA_CLIENT_ERR_STORAGE:     return "Storage error";
    case OTA_CLIENT_ERR_CHECKSUM:    return "Checksum error";
    case OTA_CLIENT_ERR_SIGNATURE:   return "Signature error";
    case OTA_CLIENT_ERR_INSTALL:     return "Install error";
    case OTA_CLIENT_ERR_IN_PROGRESS: return "In progress";
    case OTA_CLIENT_ERR_NO_UPDATE:   return "No update available";
    case OTA_CLIENT_ERR_POWER_LOW:   return "Battery too low";
    case OTA_CLIENT_ERR_TIME_WINDOW: return "Outside time window";
    case OTA_CLIENT_ERR_BUSY:        return "Busy";
    case OTA_CLIENT_ERR_CANCELLED:   return "Cancelled";
    case OTA_CLIENT_ERR_ABORTED:     return "Aborted";
    default:                         return "Unknown";
    }
}

const char *ota_client_state_str(ota_client_state_t state)
{
    switch (state) {
    case OTA_CLIENT_STATE_IDLE:         return "Idle";
    case OTA_CLIENT_STATE_CHECKING:     return "Checking";
    case OTA_CLIENT_STATE_DOWNLOADING:  return "Downloading";
    case OTA_CLIENT_STATE_VERIFYING:    return "Verifying";
    case OTA_CLIENT_STATE_INSTALLING:   return "Installing";
    case OTA_CLIENT_STATE_COMPLETE:     return "Complete";
    case OTA_CLIENT_STATE_FAILED:       return "Failed";
    case OTA_CLIENT_STATE_RESUMING:     return "Resuming";
    default:                            return "Unknown";
    }
}
