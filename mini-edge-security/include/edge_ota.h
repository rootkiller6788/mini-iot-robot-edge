#ifndef EDGE_OTA_H
#define EDGE_OTA_H

#include <stdbool.h>
#include <stdint.h>

#define OTA_MAX_CHUNK        256
#define OTA_FLASH_BANK_SIZE  65536
#define OTA_MANIFEST_SIZE    256
#define OTA_FW_HASH_SIZE     32
#define OTA_SIG_SIZE         64
#define OTA_MAX_PAYLOAD      (64 * 1024)
#define OTA_DUAL_BANK        2
#define OTA_DEV_ID_SIZE      16

/* L1: OTA update states */
typedef enum {
    OTA_IDLE,
    OTA_CHECKING,
    OTA_DOWNLOADING,
    OTA_VERIFYING,
    OTA_APPLYING,
    OTA_COMMITTED,
    OTA_ROLLED_BACK,
    OTA_FAILED
} OtaUpdateState;

/* L1: Firmware bank info (L3: Double-bank flash architecture) */
typedef struct {
    uint32_t base_addr;
    uint32_t size;
    uint32_t version;
    bool active;
    bool valid;
    uint8_t fw_hash[OTA_FW_HASH_SIZE];
} FirmwareBank;

/* L1: OTA manifest (signed metadata describing update) */
typedef struct {
    uint32_t fw_version;
    uint32_t fw_size;
    uint8_t fw_hash[OTA_FW_HASH_SIZE];
    uint32_t target_hw_id;
    uint8_t device_id[OTA_DEV_ID_SIZE];
    uint8_t signature[OTA_SIG_SIZE];
    uint32_t min_bootloader_ver;
    uint32_t timestamp;
    uint8_t metadata[OTA_MANIFEST_SIZE];
    int metadata_len;
} OtaManifest;

/* L1: OTA update context */
typedef struct {
    uint8_t device_id[OTA_DEV_ID_SIZE];
    FirmwareBank banks[OTA_DUAL_BANK];
    int active_bank;
    uint32_t current_version;
    uint32_t bytes_received;
    uint32_t total_size;
    OtaUpdateState update_state;
    uint8_t receive_buffer[OTA_MAX_PAYLOAD];
    OtaManifest current_manifest;
    bool manifest_verified;
    int retry_count;
    int max_retries;
} OtaUpdateCtx;

/* L5: OTA update lifecycle */
void edge_ota_init(OtaUpdateCtx *ctx, const uint8_t *dev_id, int id_len);
void edge_ota_manifest_init(OtaManifest *m, uint32_t version, uint32_t size);
void edge_ota_manifest_set_hash(OtaManifest *m, const uint8_t *hash);
void edge_ota_manifest_sign(OtaManifest *m, const uint8_t *priv_key);
bool edge_ota_manifest_verify(OtaUpdateCtx *ctx, const OtaManifest *m);

/* L6: OTA download pipeline */
bool edge_ota_begin_update(OtaUpdateCtx *ctx, const OtaManifest *m);
bool edge_ota_receive_chunk(OtaUpdateCtx *ctx, const uint8_t *chunk, int len);
bool edge_ota_verify_and_apply(OtaUpdateCtx *ctx);
bool edge_ota_commit_update(OtaUpdateCtx *ctx);

/* L6: Rollback and recovery */
bool edge_ota_rollback_update(OtaUpdateCtx *ctx);
bool edge_ota_check_version(OtaUpdateCtx *ctx, uint32_t version);

/* L4: NIST SP 800-193: Platform Firmware Resiliency guidelines */
bool edge_ota_validate_golden_image(OtaUpdateCtx *ctx);
bool edge_ota_check_firmware_integrity(OtaUpdateCtx *ctx, int bank_idx);
void edge_ota_repair_corrupted_bank(OtaUpdateCtx *ctx, int src_bank, int dst_bank);

/* L7: AWS IoT Jobs / Azure Device Update integration helpers */
void edge_ota_build_job_document(OtaUpdateCtx *ctx, uint8_t *doc, int *doc_len);
bool edge_ota_parse_job_response(OtaUpdateCtx *ctx, const uint8_t *response, int rlen);

/* L9: Delta/binary diff update preparation */
int  edge_ota_compute_delta(const uint8_t *old_fw, int old_len,
                            const uint8_t *new_fw, int new_len,
                            uint8_t *delta, int max_delta);

#endif
