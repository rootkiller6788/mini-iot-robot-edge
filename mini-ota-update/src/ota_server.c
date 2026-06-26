#include "ota_server.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    ota_server_config_t config;
    bool initialized;
    char cached_token[OTA_SERVER_AUTH_TOKEN_MAX];
    size_t release_count;
    ota_firmware_release_t *releases;
    size_t rollout_count;
    ota_rollout_plan_t *rollouts;
    size_t group_count;
    ota_device_group_t *groups;
} ota_server_ctx_t;

static ota_server_ctx_t g_ota_server;

static char *ota_server_encode_json(const char *key, const char *value)
{
    size_t len = strlen(key) + strlen(value) + 8;
    char *buf = (char *)malloc(len);
    if (buf) snprintf(buf, len, "\"%s\":\"%s\"", key, value);
    return buf;
}

static uint32_t ota_server_hash(const char *str)
{
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + c;
    return hash;
}

ota_server_result_t ota_server_init(const ota_server_config_t *config)
{
    if (!config || !config->base_url) return OTA_SERVER_ERR_PARAM;
    memset(&g_ota_server, 0, sizeof(g_ota_server));
    memcpy(&g_ota_server.config, config, sizeof(ota_server_config_t));
    g_ota_server.initialized = true;
    return OTA_SERVER_OK;
}

void ota_server_deinit(void)
{
    if (g_ota_server.releases) free(g_ota_server.releases);
    if (g_ota_server.rollouts) free(g_ota_server.rollouts);
    if (g_ota_server.groups) free(g_ota_server.groups);
    memset(&g_ota_server, 0, sizeof(g_ota_server));
}

ota_server_result_t ota_server_register_device(const ota_device_info_t *device,
                                                char *token_out, size_t token_size)
{
    if (!device || !token_out || token_size == 0)
        return OTA_SERVER_ERR_PARAM;
    if (!g_ota_server.initialized) return OTA_SERVER_ERR_SERVER;

    uint32_t hash = ota_server_hash(device->device_id);
    snprintf(token_out, token_size, "ota_token_%08x_%08x",
             hash, device->last_seen);

    if (g_ota_server.config.api_key) {
        snprintf(g_ota_server.cached_token, OTA_SERVER_AUTH_TOKEN_MAX,
                 "%s", token_out);
    }
    return OTA_SERVER_OK;
}

ota_server_result_t ota_server_unregister_device(const char *device_id)
{
    if (!device_id) return OTA_SERVER_ERR_PARAM;
    if (!g_ota_server.initialized) return OTA_SERVER_ERR_SERVER;
    memset(g_ota_server.cached_token, 0, sizeof(g_ota_server.cached_token));
    return OTA_SERVER_OK;
}

ota_server_result_t ota_server_update_device_info(const ota_device_info_t *device)
{
    if (!device) return OTA_SERVER_ERR_PARAM;
    return OTA_SERVER_OK;
}

ota_server_result_t ota_server_check_update(const char *device_id,
                                             const char *current_version,
                                             ota_channel_t channel,
                                             ota_firmware_release_t *release)
{
    if (!device_id || !current_version || !release)
        return OTA_SERVER_ERR_PARAM;
    if (!g_ota_server.initialized) return OTA_SERVER_ERR_SERVER;

    memset(release, 0, sizeof(ota_firmware_release_t));
    for (size_t i = 0; i < g_ota_server.release_count; i++) {
        ota_firmware_release_t *r = &g_ota_server.releases[i];
        if (r->channel == channel || channel == OTA_CHANNEL_STABLE) {
            if (strcmp(r->version, current_version) > 0) {
                memcpy(release, r, sizeof(ota_firmware_release_t));
                return OTA_SERVER_OK;
            }
        }
    }

    return OTA_SERVER_ERR_NOT_FOUND;
}

ota_server_result_t ota_server_get_release(const char *device_id,
                                            ota_firmware_release_t *release)
{
    if (!device_id || !release) return OTA_SERVER_ERR_PARAM;

    for (size_t i = 0; i < g_ota_server.release_count; i++) {
        ota_firmware_release_t *r = &g_ota_server.releases[i];
        uint32_t h = ota_server_hash(device_id) % 100;
        if (h <= r->rollout_percentage) {
            memcpy(release, r, sizeof(ota_firmware_release_t));
            return OTA_SERVER_OK;
        }
    }
    return OTA_SERVER_ERR_NOT_FOUND;
}

ota_server_result_t ota_server_begin_download(const char *device_id,
                                               ota_download_session_t *session)
{
    if (!device_id || !session) return OTA_SERVER_ERR_PARAM;

    memset(session, 0, sizeof(ota_download_session_t));
    snprintf(session->session_id, OTA_SERVER_SESSION_MAX,
             "dl_%s_%lu", device_id, (unsigned long)time(NULL));
    session->content_length = 1024 * 1024;
    session->resume_supported = true;

    strncpy(session->auth_header, g_ota_server.cached_token,
            OTA_SERVER_AUTH_TOKEN_MAX - 1);
    return OTA_SERVER_OK;
}

ota_server_result_t ota_server_resume_download(const char *session_id,
                                                uint64_t offset,
                                                ota_download_session_t *session)
{
    if (!session_id || !session) return OTA_SERVER_ERR_PARAM;
    strncpy(session->session_id, session_id, OTA_SERVER_SESSION_MAX - 1);
    session->content_length = (uint64_t)(offset + 1024 * 1024);
    session->resume_supported = true;
    return OTA_SERVER_OK;
}

ota_server_result_t ota_server_cancel_download(const char *session_id)
{
    if (!session_id) return OTA_SERVER_ERR_PARAM;
    return OTA_SERVER_OK;
}

ota_server_result_t ota_server_download_chunk(const char *session_id,
                                               uint64_t offset, uint32_t size,
                                               uint8_t *buffer, uint32_t *received)
{
    if (!session_id || !buffer || !received) return OTA_SERVER_ERR_PARAM;
    uint32_t remaining = size;
    if (remaining > OTA_SERVER_RANGE_SIZE)
        remaining = OTA_SERVER_RANGE_SIZE;
    for (uint32_t i = 0; i < remaining; i++)
        buffer[i] = (uint8_t)((offset + i) & 0xFF);
    *received = remaining;
    return OTA_SERVER_OK;
}

ota_server_result_t ota_server_report_update_status(const char *device_id,
                                                     const char *version,
                                                     bool success,
                                                     const char *error_msg)
{
    if (!device_id || !version) return OTA_SERVER_ERR_PARAM;
    (void)success; (void)error_msg;
    return OTA_SERVER_OK;
}

ota_server_result_t ota_server_create_release(ota_firmware_release_t *release)
{
    if (!release) return OTA_SERVER_ERR_PARAM;

    size_t new_count = g_ota_server.release_count + 1;
    ota_firmware_release_t *new_releases = (ota_firmware_release_t *)
        realloc(g_ota_server.releases, new_count * sizeof(ota_firmware_release_t));
    if (!new_releases) return OTA_SERVER_ERR_STORAGE;

    memcpy(&new_releases[g_ota_server.release_count], release,
           sizeof(ota_firmware_release_t));
    new_releases[g_ota_server.release_count].created_at = (uint32_t)time(NULL);
    new_releases[g_ota_server.release_count].updated_at = (uint32_t)time(NULL);

    g_ota_server.releases = new_releases;
    g_ota_server.release_count = new_count;
    return OTA_SERVER_OK;
}

ota_server_result_t ota_server_upload_firmware(const char *release_id,
                                                const uint8_t *data, size_t len)
{
    if (!release_id || !data || len == 0) return OTA_SERVER_ERR_PARAM;

    for (size_t i = 0; i < g_ota_server.release_count; i++) {
        ota_firmware_release_t *r = &g_ota_server.releases[i];
        if (strcmp(r->version, release_id) == 0) {
            r->size = (uint32_t)len;
            r->updated_at = (uint32_t)time(NULL);
            return OTA_SERVER_OK;
        }
    }
    return OTA_SERVER_ERR_NOT_FOUND;
}

ota_server_result_t ota_server_set_channel(const char *release_id,
                                            ota_channel_t channel)
{
    if (!release_id) return OTA_SERVER_ERR_PARAM;
    for (size_t i = 0; i < g_ota_server.release_count; i++) {
        if (strcmp(g_ota_server.releases[i].version, release_id) == 0) {
            g_ota_server.releases[i].channel = channel;
            return OTA_SERVER_OK;
        }
    }
    return OTA_SERVER_ERR_NOT_FOUND;
}

ota_server_result_t ota_server_create_rollout_plan(ota_rollout_plan_t *plan)
{
    if (!plan) return OTA_SERVER_ERR_PARAM;

    size_t new_count = g_ota_server.rollout_count + 1;
    ota_rollout_plan_t *new_plans = (ota_rollout_plan_t *)
        realloc(g_ota_server.rollouts, new_count * sizeof(ota_rollout_plan_t));
    if (!new_plans) return OTA_SERVER_ERR_STORAGE;

    memcpy(&new_plans[g_ota_server.rollout_count], plan,
           sizeof(ota_rollout_plan_t));
    new_plans[g_ota_server.rollout_count].state = OTA_ROLLOUT_ACTIVE;
    new_plans[g_ota_server.rollout_count].started_at = (uint32_t)time(NULL);
    new_plans[g_ota_server.rollout_count].current_percentage =
        plan->rollout_step;

    g_ota_server.rollouts = new_plans;
    g_ota_server.rollout_count = new_count;
    return OTA_SERVER_OK;
}

ota_server_result_t ota_server_advance_rollout(const char *plan_id)
{
    if (!plan_id) return OTA_SERVER_ERR_PARAM;
    for (size_t i = 0; i < g_ota_server.rollout_count; i++) {
        if (strcmp(g_ota_server.rollouts[i].product, plan_id) == 0) {
            ota_rollout_plan_t *p = &g_ota_server.rollouts[i];
            uint8_t next = p->current_percentage + p->rollout_step;
            if (next >= 100) {
                p->current_percentage = 100;
                p->state = OTA_ROLLOUT_COMPLETED;
            } else {
                p->current_percentage = next;
            }
            return OTA_SERVER_OK;
        }
    }
    return OTA_SERVER_ERR_NOT_FOUND;
}

ota_server_result_t ota_server_pause_rollout(const char *plan_id)
{
    if (!plan_id) return OTA_SERVER_ERR_PARAM;
    for (size_t i = 0; i < g_ota_server.rollout_count; i++) {
        if (strcmp(g_ota_server.rollouts[i].product, plan_id) == 0) {
            g_ota_server.rollouts[i].state = OTA_ROLLOUT_PAUSED;
            return OTA_SERVER_OK;
        }
    }
    return OTA_SERVER_ERR_NOT_FOUND;
}

ota_server_result_t ota_server_rollback_rollout(const char *plan_id)
{
    if (!plan_id) return OTA_SERVER_ERR_PARAM;
    for (size_t i = 0; i < g_ota_server.rollout_count; i++) {
        if (strcmp(g_ota_server.rollouts[i].product, plan_id) == 0) {
            g_ota_server.rollouts[i].state = OTA_ROLLOUT_ROLLEDBACK;
            g_ota_server.rollouts[i].current_percentage = 0;
            return OTA_SERVER_OK;
        }
    }
    return OTA_SERVER_ERR_NOT_FOUND;
}

ota_server_result_t ota_server_get_device_group(const char *group_name,
                                                 ota_device_group_t *group)
{
    if (!group_name || !group) return OTA_SERVER_ERR_PARAM;
    for (size_t i = 0; i < g_ota_server.group_count; i++) {
        if (strcmp(g_ota_server.groups[i].group_name, group_name) == 0) {
            memcpy(group, &g_ota_server.groups[i], sizeof(ota_device_group_t));
            return OTA_SERVER_OK;
        }
    }
    return OTA_SERVER_ERR_NOT_FOUND;
}

const char *ota_server_result_str(ota_server_result_t result)
{
    switch (result) {
    case OTA_SERVER_OK:            return "OK";
    case OTA_SERVER_ERR_PARAM:     return "Invalid parameter";
    case OTA_SERVER_ERR_AUTH:      return "Authentication error";
    case OTA_SERVER_ERR_NETWORK:   return "Network error";
    case OTA_SERVER_ERR_NOT_FOUND: return "Not found";
    case OTA_SERVER_ERR_SERVER:    return "Server error";
    case OTA_SERVER_ERR_STORAGE:   return "Storage error";
    case OTA_SERVER_ERR_VERSION:   return "Version error";
    case OTA_SERVER_ERR_ROLLOUT:   return "Rollout error";
    case OTA_SERVER_ERR_BUSY:      return "Server busy";
    case OTA_SERVER_ERR_BAD_REQUEST: return "Bad request";
    default:                       return "Unknown";
    }
}
