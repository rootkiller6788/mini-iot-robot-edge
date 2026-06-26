#ifndef DEVICE_ATTEST_H
#define DEVICE_ATTEST_H

#include <stdbool.h>
#include <stdint.h>

#define DEVICE_ID_SIZE         32
#define ATTEST_TOKEN_SIZE      512
#define X509_CERT_SIZE         1024
#define DEVICE_KEY_SIZE        32
#define CHALLENGE_SIZE         32
#define RESPONSE_SIZE          64
#define FIRMWARE_HASH_SIZE     32
#define BOOT_STATE_SIZE        32
#define AWS_THING_NAME_MAX     64
#define AZURE_REG_ID_MAX       64
#define JITP_CERT_SIZE         1024

typedef enum {
    ATTEST_NONE,
    ATTEST_CHALLENGE_SENT,
    ATTEST_RESPONSE_READY,
    ATTEST_VERIFIED,
    ATTEST_REJECTED
} AttestState;

typedef enum {
    KEY_SOURCE_FACTORY,
    KEY_SOURCE_TPM,
    KEY_SOURCE_SECURE_ELEMENT,
    KEY_SOURCE_SOFTWARE
} KeySource;

typedef enum {
    PROTOCOL_AWS_IOT,
    PROTOCOL_AZURE_DPS,
    PROTOCOL_CUSTOM
} CloudProtocol;

typedef struct {
    uint8_t device_id[DEVICE_ID_SIZE];
    uint8_t device_key[DEVICE_KEY_SIZE];
    KeySource key_source;
    bool key_protected;
    uint8_t factory_cert[X509_CERT_SIZE];
    int factory_cert_len;
} DeviceIdentity;

typedef struct {
    DeviceIdentity identity;
    uint8_t fw_hash[FIRMWARE_HASH_SIZE];
    uint8_t boot_state[BOOT_STATE_SIZE];
    bool secure_boot_ok;
    uint32_t fw_version;
    uint8_t nonce[CHALLENGE_SIZE];
    AttestState state;
    CloudProtocol protocol;
    uint8_t token[ATTEST_TOKEN_SIZE];
    int token_len;
} DeviceAttestCtx;

typedef struct {
    uint8_t cert_data[X509_CERT_SIZE];
    int cert_len;
    uint8_t subject_pubkey[64];
    uint8_t issuer[64];
    uint8_t serial[16];
    uint32_t not_before;
    uint32_t not_after;
    bool is_ca;
} X509Cert;

void device_attest_init(DeviceAttestCtx *ctx);
void device_identity_generate(DeviceAttestCtx *ctx, const uint8_t *uid, int len);
void device_identity_inject_factory(DeviceAttestCtx *ctx, const uint8_t *key,
                                     const uint8_t *cert, int cert_len);
void device_attest_token_generate(DeviceAttestCtx *ctx);
void device_attest_challenge(DeviceAttestCtx *ctx, const uint8_t *challenge, int len);
bool device_attest_verify_cloud(DeviceAttestCtx *ctx, const uint8_t *response);
void device_attest_set_fw_hash(DeviceAttestCtx *ctx, const uint8_t *hash);
void device_attest_set_boot_state(DeviceAttestCtx *ctx, const uint8_t *state, bool ok);
void device_attest_protocol_aws(DeviceAttestCtx *ctx, const char *thing_name);
void device_attest_protocol_azure(DeviceAttestCtx *ctx, const char *reg_id);
void device_attest_jitp_cert(DeviceAttestCtx *ctx, X509Cert *out_cert);
bool device_attest_verify_device_cert(DeviceAttestCtx *ctx, const X509Cert *cert);
void device_attest_tpm_extend(DeviceAttestCtx *ctx, const uint8_t *pcr_data, int len);
void device_attest_sign_challenge(DeviceAttestCtx *ctx, uint8_t *response, int *rlen);
bool device_attest_validate_token(DeviceAttestCtx *ctx, const uint8_t *token, int len);
void device_attest_derive_session_key(DeviceAttestCtx *ctx,
                                       uint8_t session_key[DEVICE_KEY_SIZE]);

#endif
