#ifndef SECURE_BOOT_MCU_H
#define SECURE_BOOT_MCU_H

#include <stdbool.h>
#include <stdint.h>

#define BOOT_ROM_SIZE          4096
#define SPL_MAX_SIZE           (64 * 1024)
#define IMAGE_MAX_SIZE         (256 * 1024)
#define SIGNATURE_SIZE         256
#define PUBKEY_HASH_SIZE       32
#define BOOT_STATE_SIZE        32
#define IMAGE_HEADER_SIZE      64
#define OTP_FUSE_COUNT         16
#define CHAIN_DEPTH            4
#define MAX_CERTS              3

typedef enum {
    BOOT_STAGE_ROM,
    BOOT_STAGE_SPL,
    BOOT_STAGE_UBOOT,
    BOOT_STAGE_KERNEL,
    BOOT_STAGE_APP
} BootStage;

typedef enum {
    BOOT_OK,
    BOOT_ERR_SIG,
    BOOT_ERR_HASH,
    BOOT_ERR_VERSION,
    BOOT_ERR_OTP,
    BOOT_ERR_LOCKED,
    BOOT_ERR_ROLLBACK,
    BOOT_ERR_DEBUG
} BootError;

typedef enum {
    BOOT_POLICY_FAIL_STOP,
    BOOT_POLICY_FAIL_RECOVER,
    BOOT_POLICY_FAIL_REPORT,
    BOOT_POLICY_FAIL_DEBUG
} BootFailPolicy;

typedef enum {
    DEBUG_LOCKED,
    DEBUG_UNLOCKED_TEMP,
    DEBUG_UNLOCKED_PERM
} DebugAuthState;

typedef struct {
    uint8_t magic[4];
    uint32_t version;
    uint32_t image_size;
    uint32_t load_addr;
    uint32_t entry_point;
    uint8_t image_hash[32];
    uint8_t pubkey_hash[PUBKEY_HASH_SIZE];
    uint32_t flags;
    uint32_t rollback_ctr;
    uint8_t reserved[12];
} ImageHeader;

typedef struct {
    ImageHeader header;
    uint8_t payload[IMAGE_MAX_SIZE];
    uint8_t signature[SIGNATURE_SIZE];
} SignedImage;

typedef struct {
    uint8_t rom_hash[32];
    uint8_t pubkey_hash[PUBKEY_HASH_SIZE];
    bool secure_boot_enabled;
    BootFailPolicy policy;
    DebugAuthState debug_state;
    BootStage current_stage;
    uint32_t min_version[CHAIN_DEPTH];
    BootError last_error;
    uint8_t boot_state[BOOT_STATE_SIZE];
} SecureBootCtx;

typedef struct {
    uint8_t cert_data[512];
    uint32_t cert_len;
    uint8_t subject_pubkey[64];
    uint8_t issuer_hash[32];
} BootCertificate;

void secure_boot_init(SecureBootCtx *ctx);
BootError secure_boot_rom_verify(SecureBootCtx *ctx, const uint8_t *spl, int len);
BootError secure_boot_chain_verify(SecureBootCtx *ctx, BootStage from, BootStage to,
                                    const uint8_t *image, int len);
BootError secure_boot_verify_image(SecureBootCtx *ctx, const SignedImage *img);
void secure_boot_set_policy(SecureBootCtx *ctx, BootFailPolicy policy);
void secure_boot_lock_debug(SecureBootCtx *ctx);
DebugAuthState secure_boot_debug_auth(SecureBootCtx *ctx, const uint8_t *challenge,
                                       const uint8_t *response, int len);
void secure_boot_get_state(SecureBootCtx *ctx, uint8_t state[BOOT_STATE_SIZE]);
bool secure_boot_check_rollback(SecureBootCtx *ctx, uint32_t version);
void secure_boot_cert_chain_verify(SecureBootCtx *ctx, const BootCertificate *certs,
                                    int count);
void secure_boot_otp_write(SecureBootCtx *ctx, int fuse_idx, uint32_t value);
uint32_t secure_boot_otp_read(SecureBootCtx *ctx, int fuse_idx);

#endif
