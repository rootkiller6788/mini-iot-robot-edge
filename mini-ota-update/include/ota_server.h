#ifndef OTA_SERVER_H
#define OTA_SERVER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OTA_SERVER_URL_MAX          256
#define OTA_SERVER_AUTH_TOKEN_MAX   128
#define OTA_SERVER_DEVICE_ID_MAX    64
#define OTA_SERVER_PRODUCT_MAX      64
#define OTA_SERVER_VERSION_MAX      64
#define OTA_SERVER_CHANNEL_MAX      32
#define OTA_SERVER_SESSION_MAX      64
#define OTA_SERVER_RANGE_SIZE       (2 * 1024 * 1024)

typedef enum {
    OTA_SERVER_OK = 0,
    OTA_SERVER_ERR_PARAM,
    OTA_SERVER_ERR_AUTH,
    OTA_SERVER_ERR_NETWORK,
    OTA_SERVER_ERR_NOT_FOUND,
    OTA_SERVER_ERR_SERVER,
    OTA_SERVER_ERR_STORAGE,
    OTA_SERVER_ERR_VERSION,
    OTA_SERVER_ERR_ROLLOUT,
    OTA_SERVER_ERR_BUSY,
    OTA_SERVER_ERR_BAD_REQUEST
} ota_server_result_t;

typedef enum {
    OTA_CHANNEL_DEV = 0,
    OTA_CHANNEL_BETA,
    OTA_CHANNEL_STABLE
} ota_channel_t;

typedef enum {
    OTA_ROLLOUT_PAUSED = 0,
    OTA_ROLLOUT_ACTIVE,
    OTA_ROLLOUT_COMPLETED,
    OTA_ROLLOUT_ROLLEDBACK
} ota_rollout_state_t;

typedef struct {
    char device_id[OTA_SERVER_DEVICE_ID_MAX];
    char product[OTA_SERVER_PRODUCT_MAX];
    char current_version[OTA_SERVER_VERSION_MAX];
    uint32_t last_seen;
    ota_channel_t channel;
    bool enrolled;
    uint8_t max_rollout_pct;
} ota_device_info_t;

typedef struct {
    char session_id[OTA_SERVER_SESSION_MAX];
    char url[OTA_SERVER_URL_MAX];
    uint64_t content_length;
    uint8_t checksum_sha256[32];
    bool resume_supported;
    char auth_header[OTA_SERVER_AUTH_TOKEN_MAX];
} ota_download_session_t;

typedef struct {
    char product[OTA_SERVER_PRODUCT_MAX];
    char version[OTA_SERVER_VERSION_MAX];
    ota_channel_t channel;
    uint32_t size;
    uint8_t hash_sha256[32];
    uint32_t min_hardware_version;
    uint32_t max_hardware_version;
    uint8_t rollout_percentage;
    ota_rollout_state_t rollout_state;
    uint32_t created_at;
    uint32_t updated_at;
    bool mandatory;
    char changelog[512];
    char download_url[OTA_SERVER_URL_MAX];
    bool force_update;
    uint32_t force_after;
} ota_firmware_release_t;

typedef struct {
    char product[OTA_SERVER_PRODUCT_MAX];
    uint8_t rollout_step;
    uint8_t target_percentage;
    uint32_t step_duration_hours;
    uint8_t success_rate_threshold;
    ota_rollout_state_t state;
    uint32_t started_at;
    uint8_t current_percentage;
} ota_rollout_plan_t;

typedef struct {
    char group_name[64];
    char product[OTA_SERVER_PRODUCT_MAX];
    uint32_t device_count;
    uint32_t updated_count;
    uint32_t failed_count;
    uint8_t rollout_pct;
} ota_device_group_t;

typedef void *(*ota_http_get_fn)(const char *url, const char *auth,
                                  uint8_t **response, size_t *response_len,
                                  uint32_t range_start, uint32_t range_end,
                                  void *ctx);
typedef void (*ota_http_free_fn)(void *handle, void *ctx);

typedef struct {
    const char *base_url;
    const char *api_key;
    ota_http_get_fn http_get;
    ota_http_free_fn http_free;
    void *ctx;
} ota_server_config_t;

ota_server_result_t ota_server_init(const ota_server_config_t *config);
void ota_server_deinit(void);

ota_server_result_t ota_server_register_device(const ota_device_info_t *device,
                                                char *token_out, size_t token_size);
ota_server_result_t ota_server_unregister_device(const char *device_id);
ota_server_result_t ota_server_update_device_info(const ota_device_info_t *device);

ota_server_result_t ota_server_check_update(const char *device_id,
                                             const char *current_version,
                                             ota_channel_t channel,
                                             ota_firmware_release_t *release);
ota_server_result_t ota_server_get_release(const char *device_id,
                                            ota_firmware_release_t *release);

ota_server_result_t ota_server_begin_download(const char *device_id,
                                               ota_download_session_t *session);
ota_server_result_t ota_server_resume_download(const char *session_id,
                                                uint64_t offset,
                                                ota_download_session_t *session);
ota_server_result_t ota_server_cancel_download(const char *session_id);

ota_server_result_t ota_server_download_chunk(const char *session_id,
                                               uint64_t offset, uint32_t size,
                                               uint8_t *buffer, uint32_t *received);
ota_server_result_t ota_server_report_update_status(const char *device_id,
                                                     const char *version,
                                                     bool success,
                                                     const char *error_msg);

ota_server_result_t ota_server_create_release(ota_firmware_release_t *release);
ota_server_result_t ota_server_upload_firmware(const char *release_id,
                                                const uint8_t *data, size_t len);
ota_server_result_t ota_server_set_channel(const char *release_id,
                                            ota_channel_t channel);

ota_server_result_t ota_server_create_rollout_plan(ota_rollout_plan_t *plan);
ota_server_result_t ota_server_advance_rollout(const char *plan_id);
ota_server_result_t ota_server_pause_rollout(const char *plan_id);
ota_server_result_t ota_server_rollback_rollout(const char *plan_id);

ota_server_result_t ota_server_get_device_group(const char *group_name,
                                                 ota_device_group_t *group);

const char *ota_server_result_str(ota_server_result_t result);

#ifdef __cplusplus
}
#endif

#endif
