#ifndef SIGNED_IMAGE_H
#define SIGNED_IMAGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SIGNED_IMAGE_MAGIC           0x5349474E
#define SIGNED_IMAGE_VERSION         1
#define SIGNED_IMAGE_SIG_MAX         72
#define SIGNED_IMAGE_KEY_MAX         64
#define SIGNED_IMAGE_HASH_SIZE       32
#define SIGNED_IMAGE_NONCE_SIZE      16
#define SIGNED_IMAGE_IV_SIZE         16
#define SIGNED_IMAGE_AES_BLOCK_SIZE  16
#define SIGNED_IMAGE_MAX_IMAGE_SIZE  (32 * 1024 * 1024)
#define SIGNED_IMAGE_HEADER_SIZE     512

typedef enum {
    SIG_IMG_OK = 0,
    SIG_IMG_ERR_PARAM,
    SIG_IMG_ERR_MAGIC,
    SIG_IMG_ERR_VERSION,
    SIG_IMG_ERR_SIGNATURE,
    SIG_IMG_ERR_HASH,
    SIG_IMG_ERR_ROLLBACK,
    SIG_IMG_ERR_KEY,
    SIG_IMG_ERR_DECRYPT,
    SIG_IMG_ERR_ENCRYPT,
    SIG_IMG_ERR_NOMEM,
    SIG_IMG_ERR_IO,
    SIG_IMG_ERR_SIZE,
    SIG_IMG_ERR_COUNTER,
    SIG_IMG_ERR_EFUSE,
    SIG_IMG_ERR_TPM,
    SIG_IMG_ERR_NOT_ENCRYPTED,
    SIG_IMG_ERR_ALREADY_BOOTED
} sig_img_result_t;

typedef enum {
    SIG_IMG_ALGO_ECDSA_P256 = 0,
    SIG_IMG_ALGO_ECDSA_P384,
    SIG_IMG_ALGO_ED25519,
    SIG_IMG_ALGO_RSA2048,
    SIG_IMG_ALGO_RSA3072,
    SIG_IMG_ALGO_RSA4096
} sig_img_algo_t;

typedef enum {
    SIG_IMG_ENC_NONE = 0,
    SIG_IMG_ENC_AES128_CTR,
    SIG_IMG_ENC_AES256_CTR,
    SIG_IMG_ENC_AES128_GCM,
    SIG_IMG_ENC_AES256_GCM
} sig_img_encryption_t;

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint16_t header_version;
    uint16_t header_size;
    uint32_t image_size;
    uint32_t image_offset;
    uint32_t version_major;
    uint32_t version_minor;
    uint32_t version_patch;
    uint64_t build_timestamp;
    uint8_t image_hash[SIGNED_IMAGE_HASH_SIZE];
    uint8_t signature[SIGNED_IMAGE_SIG_MAX];
    uint16_t signature_len;
    uint8_t signature_algo;
    uint8_t encryption;
    uint8_t iv[SIGNED_IMAGE_IV_SIZE];
    uint8_t key_fingerprint[SIGNED_IMAGE_HASH_SIZE];
    uint32_t key_id;
    uint32_t min_counter_value;
    uint32_t max_rollback_index;
    uint8_t hw_id[16];
    uint8_t hw_id_mask[16];
    uint8_t security_level;
    uint8_t flags;
    uint16_t product_id;
    uint16_t vendor_id;
    uint8_t reserved[64];
    uint32_t header_crc;
} signed_image_header_t;
#pragma pack(pop)

typedef struct {
    uint8_t public_key[SIGNED_IMAGE_KEY_MAX];
    size_t public_key_len;
    sig_img_algo_t algo;
    uint8_t key_fingerprint[SIGNED_IMAGE_HASH_SIZE];
    uint32_t key_id;
    bool active;
    uint32_t not_before;
    uint32_t not_after;
} sig_img_public_key_t;

typedef struct {
    uint8_t *key_data;
    size_t key_len;
    sig_img_algo_t algo;
    uint8_t *aes_key;
    size_t aes_key_len;
} sig_img_key_store_t;

typedef struct {
    uint32_t (*read_counter)(void *ctx);
    int (*write_counter)(uint32_t value, void *ctx);
    bool (*is_efuse_available)(void *ctx);
    int (*burn_efuse_bit)(uint32_t bit_index, void *ctx);
    int (*tpm_sign)(const uint8_t *hash, size_t hash_len,
                    uint8_t *sig, size_t *sig_len, void *ctx);
    int (*tpm_get_counter)(uint32_t *counter, void *ctx);
    void *ctx;
} sig_img_hal_t;

typedef struct {
    sig_img_public_key_t *keys;
    size_t num_keys;
    sig_img_hal_t hal;
    uint8_t *aes_key;
    size_t aes_key_len;
} sig_img_verify_ctx_t;

sig_img_result_t sig_img_header_parse(const uint8_t *data, size_t len,
                                       signed_image_header_t *header);
sig_img_result_t sig_img_header_validate(const signed_image_header_t *header);

sig_img_result_t sig_img_verify_signature(const uint8_t *image, size_t image_len,
                                           const sig_img_public_key_t *key);
sig_img_result_t sig_img_verify_hash(const uint8_t *image, size_t image_len,
                                      const uint8_t *expected_hash);

sig_img_result_t sig_img_verify_full(const uint8_t *image, size_t image_len,
                                      sig_img_verify_ctx_t *ctx);

sig_img_result_t sig_img_check_anti_rollback(const signed_image_header_t *header,
                                              sig_img_hal_t *hal, bool *allowed);

sig_img_result_t sig_img_encrypt_payload(const uint8_t *plain, size_t plain_len,
                                          const uint8_t *key, size_t key_len,
                                          const uint8_t *iv, size_t iv_len,
                                          uint8_t *encrypted, size_t *enc_len);
sig_img_result_t sig_img_decrypt_payload(const uint8_t *encrypted, size_t enc_len,
                                          const uint8_t *key, size_t key_len,
                                          const uint8_t *iv, size_t iv_len,
                                          uint8_t *plain, size_t *plain_len);

sig_img_result_t sig_img_aes_ctr_crypt(const uint8_t *input, size_t input_len,
                                        const uint8_t *key, size_t key_len,
                                        const uint8_t *nonce, size_t nonce_len,
                                        uint8_t *output);

sig_img_result_t sig_img_build_header(const uint8_t *image, size_t image_len,
                                       const sig_img_key_store_t *keys,
                                       signed_image_header_t *header,
                                       uint8_t *signed_output, size_t *out_len);

sig_img_result_t sig_img_key_load(const uint8_t *key_data, size_t key_len,
                                   sig_img_public_key_t *key);
sig_img_result_t sig_img_key_fingerprint(const sig_img_public_key_t *key,
                                          uint8_t *fp_out);

const char *sig_img_result_str(sig_img_result_t result);
const char *sig_img_algo_name(sig_img_algo_t algo);

#ifdef __cplusplus
}
#endif

#endif
