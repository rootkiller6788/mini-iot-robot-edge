#ifndef PSA_CERTIFIED_H
#define PSA_CERTIFIED_H

#include <stdbool.h>
#include <stdint.h>

#define PSA_LEVEL1            1
#define PSA_LEVEL2            2
#define PSA_LEVEL3            3
#define PSA_KEY_ID_MAX        32
#define PSA_SIGNATURE_MAX     64
#define PSA_HASH_SIZE         32
#define PSA_NONCE_SIZE        32
#define PSA_CHALLENGE_SIZE    32
#define PSA_IAT_MAX_SIZE      512
#define PSA_ATTEST_KEY_SIZE   32
#define PSA_FW_METADATA_SIZE  64

typedef enum {
    PSA_ALG_SHA256,
    PSA_ALG_ECDSA_P256,
    PSA_ALG_ECDSA_P384,
    PSA_ALG_AES_GCM_128,
    PSA_ALG_AES_GCM_256,
    PSA_ALG_HMAC_SHA256
} PSAAlgorithm;

typedef enum {
    PSA_KEY_TYPE_AES_128,
    PSA_KEY_TYPE_AES_256,
    PSA_KEY_TYPE_ECC_P256,
    PSA_KEY_TYPE_ECC_P384,
    PSA_KEY_TYPE_HMAC_256
} PSAKeyType;

typedef enum {
    PSA_KEY_USAGE_EXPORT,
    PSA_KEY_USAGE_SIGN,
    PSA_KEY_USAGE_VERIFY,
    PSA_KEY_USAGE_ENCRYPT,
    PSA_KEY_USAGE_DECRYPT,
    PSA_KEY_USAGE_DERIVE
} PSAKeyUsage;

typedef enum {
    PSA_ATTEST_INIT,
    PSA_ATTEST_TOKEN_GEN,
    PSA_ATTEST_CHALLENGE,
    PSA_ATTEST_VERIFY,
    PSA_ATTEST_ERROR
} PSAAttestState;

typedef struct {
    uint32_t key_id;
    PSAKeyType type;
    PSAKeyUsage usage;
    uint8_t key_data[PSA_KEY_ID_MAX];
    int key_len;
    bool locked;
} PSAKeySlot;

typedef struct {
    uint8_t sw_components[PSA_FW_METADATA_SIZE];
    int sw_comp_len;
    uint8_t boot_seed[16];
    uint32_t security_lifecycle;
    uint8_t impl_id[16];
    uint32_t cert_ref;
} PSASecurityModel;

typedef struct {
    uint32_t level;
    PSASecurityModel model;
    char profile_name[32];
    bool isolation_present;
    bool secure_boot_present;
    bool crypto_present;
    bool attestation_present;
} PSACertifiedCtx;

typedef struct {
    uint8_t token[PSA_IAT_MAX_SIZE];
    int token_len;
    uint8_t challenge[PSA_CHALLENGE_SIZE];
    int challenge_len;
    uint8_t device_id[32];
    uint8_t fw_hash[PSA_HASH_SIZE];
    bool boot_ok;
    PSAAttestState state;
} PSAAttestToken;

void psa_certified_init(PSACertifiedCtx *ctx, uint32_t level);
void psa_set_security_model(PSACertifiedCtx *ctx, const uint8_t *sw_components, int len);
void psa_set_profile(PSACertifiedCtx *ctx, const char *name);
bool psa_validate_level(PSACertifiedCtx *ctx);
void psa_key_generate(PSACertifiedCtx *ctx, PSAKeySlot *slot, PSAKeyType type);
void psa_key_import(PSACertifiedCtx *ctx, PSAKeySlot *slot,
                     const uint8_t *data, int len);
void psa_sign(PSACertifiedCtx *ctx, const PSAKeySlot *slot, PSAAlgorithm alg,
              const uint8_t *hash, uint8_t signature[PSA_SIGNATURE_MAX], int *sig_len);
bool psa_verify(PSACertifiedCtx *ctx, const PSAKeySlot *slot, PSAAlgorithm alg,
                const uint8_t *hash, const uint8_t *sig, int sig_len);
void psa_encrypt(PSACertifiedCtx *ctx, const PSAKeySlot *slot, PSAAlgorithm alg,
                 const uint8_t *plain, int plen, uint8_t *cipher);
void psa_decrypt(PSACertifiedCtx *ctx, const PSAKeySlot *slot, PSAAlgorithm alg,
                 const uint8_t *cipher, int clen, uint8_t *plain);
void psa_attestation_init(PSACertifiedCtx *ctx, PSAAttestToken *token);
void psa_attestation_generate(PSACertifiedCtx *ctx, PSAAttestToken *token,
                               const uint8_t *challenge, int clen);
bool psa_attestation_verify(PSACertifiedCtx *ctx, const PSAAttestToken *token,
                            const uint8_t *expected_hash);
void psa_key_derive(PSACertifiedCtx *ctx, const PSAKeySlot *master,
                     const uint8_t *label, int llen, PSAKeySlot *derived);
void psa_firmware_update_prepare(PSACertifiedCtx *ctx, const uint8_t *fw_meta, int len);
bool psa_firmware_update_verify(PSACertifiedCtx *ctx, const uint8_t *fw,
                                 int len, const uint8_t *sig);

#endif
