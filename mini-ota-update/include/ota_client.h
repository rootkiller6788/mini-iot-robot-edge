#ifndef OTA_CLIENT_H
#define OTA_CLIENT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "ab_update.h"
#include "signed_image.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OTA_CLIENT_URL_MAX           256
#define OTA_CLIENT_VERSION_MAX       64
#define OTA_CLIENT_DEVICE_ID_MAX     64
#define OTA_CLIENT_CACHE_SIZE        (64 * 1024)
#define OTA_CLIENT_CHUNK_SIZE        4096
#define OTA_CLIENT_RESUME_MAGIC      0x4F544152
#define OTA_CLIENT_MAX_RETRIES       3
#define OTA_CLIENT_RETRY_DELAY_MS    5000
#define OTA_CLIENT_PROGRESS_INTERVAL 10

typedef enum {
    OTA_CLIENT_OK = 0,
    OTA_CLIENT_ERR_PARAM,
    OTA_CLIENT_ERR_NETWORK,
    OTA_CLIENT_ERR_STORAGE,
    OTA_CLIENT_ERR_CHECKSUM,
    OTA_CLIENT_ERR_SIGNATURE,
    OTA_CLIENT_ERR_INSTALL,
    OTA_CLIENT_ERR_IN_PROGRESS,
    OTA_CLIENT_ERR_NO_UPDATE,
    OTA_CLIENT_ERR_POWER_LOW,
    OTA_CLIENT_ERR_TIME_WINDOW,
    OTA_CLIENT_ERR_BUSY,
    OTA_CLIENT_ERR_CANCELLED,
    OTA_CLIENT_ERR_ABORTED
} ota_client_result_t;

typedef enum {
    OTA_CLIENT_STATE_IDLE = 0,
    OTA_CLIENT_STATE_CHECKING,
    OTA_CLIENT_STATE_DOWNLOADING,
    OTA_CLIENT_STATE_VERIFYING,
    OTA_CLIENT_STATE_INSTALLING,
    OTA_CLIENT_STATE_COMPLETE,
    OTA_CLIENT_STATE_FAILED,
    OTA_CLIENT_STATE_RESUMING
} ota_client_state_t;

typedef enum {
    OTA_POLICY_NONE = 0,
    OTA_POLICY_ANYTIME,
    OTA_POLICY_TIME_WINDOW,
    OTA_POLICY_BATTERY_REQUIRED,
    OTA_POLICY_USER_CONSENT,
    OTA_POLICY_MAINTENANCE_MODE
} ota_update_policy_t;

typedef struct {
    uint32_t resume_magic;
    char session_id[64];
    char firmware_url[OTA_CLIENT_URL_MAX];
    char version[OTA_CLIENT_VERSION_MAX];
    uint64_t total_size;
    uint64_t downloaded_size;
    uint8_t hash_sha256[32];
    uint32_t chunk_size;
    uint32_t last_chunk;
    uint8_t last_chunk_hash[32];
    uint32_t retry_count;
    uint32_t last_error;
    uint32_t timestamp;
    uint8_t reserved[64];
} ota_client_resume_state_t;

typedef struct {
    ota_update_policy_t policy;
    uint32_t time_window_start_hour;
    uint32_t time_window_start_min;
    uint32_t time_window_end_hour;
    uint32_t time_window_end_min;
    uint32_t min_battery_percent;
    bool require_charger;
    uint32_t max_retries;
    uint32_t retry_delay_ms;
    bool allow_cellular;
    bool background_download;
    bool auto_install;
    uint32_t install_delay_s;
    uint32_t reboot_delay_s;
    bool power_safe_mode;
} ota_update_policy_config_t;

typedef struct {
    uint32_t bytes_downloaded;
    uint32_t bytes_total;
    uint32_t percent;
    uint32_t speed_bytes_per_sec;
    uint32_t elapsed_seconds;
    uint32_t estimated_remaining;
    ota_client_state_t state;
} ota_progress_info_t;

typedef void (*ota_progress_cb_t)(const ota_progress_info_t *info, void *user_data);
typedef void (*ota_event_cb_t)(ota_client_state_t state, int32_t error, void *user_data);
typedef int (*ota_download_cb_t)(const uint8_t *url, uint64_t offset,
                                  uint32_t size, uint8_t *buffer,
                                  uint32_t *received, void *user_data);
typedef int (*ota_upload_cb_t)(const uint8_t *url, const uint8_t *data,
                                size_t len, void *user_data);
typedef uint32_t (*ota_time_fn_t)(void);
typedef uint32_t (*ota_battery_fn_t)(void);

typedef struct {
    ota_download_cb_t download;
    ota_upload_cb_t upload;
    ota_progress_cb_t progress;
    ota_event_cb_t event;
    ota_time_fn_t get_time;
    ota_battery_fn_t get_battery;
    void *user_data;
} ota_client_cb_t;

typedef struct ota_client_ctx ota_client_ctx_t;

ota_client_ctx_t *ota_client_init(const ota_client_cb_t *callbacks);
void ota_client_deinit(ota_client_ctx_t *ctx);

ota_client_result_t ota_client_set_policy(ota_client_ctx_t *ctx,
                                           const ota_update_policy_config_t *policy);
ota_client_result_t ota_client_get_policy(ota_client_ctx_t *ctx,
                                           ota_update_policy_config_t *policy);

ota_client_result_t ota_client_set_update_url(ota_client_ctx_t *ctx,
                                               const char *url);
ota_client_result_t ota_client_set_device_id(ota_client_ctx_t *ctx,
                                              const char *device_id);
ota_client_result_t ota_client_set_current_version(ota_client_ctx_t *ctx,
                                                    const char *version);

ota_client_result_t ota_client_check_for_update(ota_client_ctx_t *ctx,
                                                 char *new_version, size_t ver_size,
                                                 bool *update_available);

ota_client_result_t ota_client_download_firmware(ota_client_ctx_t *ctx);
ota_client_result_t ota_client_download_get_progress(ota_client_ctx_t *ctx,
                                                      ota_progress_info_t *info);
ota_client_result_t ota_client_download_cancel(ota_client_ctx_t *ctx);

ota_client_result_t ota_client_verify_firmware(ota_client_ctx_t *ctx);
ota_client_result_t ota_client_install_firmware(ota_client_ctx_t *ctx,
                                                 bool reboot_after);

ota_client_result_t ota_client_save_resume_state(ota_client_ctx_t *ctx);
ota_client_result_t ota_client_load_resume_state(ota_client_ctx_t *ctx);
ota_client_result_t ota_client_clear_resume_state(ota_client_ctx_t *ctx);

ota_client_result_t ota_client_check_policy_window(ota_client_ctx_t *ctx,
                                                    bool *in_window);
ota_client_result_t ota_client_check_battery(ota_client_ctx_t *ctx,
                                              bool *sufficient);
ota_client_result_t ota_client_get_state(ota_client_ctx_t *ctx,
                                          ota_client_state_t *state);

const char *ota_client_result_str(ota_client_result_t result);
const char *ota_client_state_str(ota_client_state_t state);

#ifdef __cplusplus
}
#endif

#endif
