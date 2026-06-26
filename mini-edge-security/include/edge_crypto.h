#ifndef EDGE_CRYPTO_H
#define EDGE_CRYPTO_H

#include <stdbool.h>
#include <stdint.h>

#define EDGE_CRYPTO_KEY_MAX      64
#define EDGE_CRYPTO_BLOCK_SIZE   16
#define EDGE_CRYPTO_IV_SIZE      16
#define EDGE_CRYPTO_HASH_SIZE    32
#define EDGE_CRYPTO_HMAC_SIZE    32
#define EDGE_CRYPTO_SIG_MAX      64
#define EDGE_CRYPTO_NONCE_SIZE   12
#define EDGE_CRYPTO_TAG_SIZE     16
#define EDGE_CRYPTO_AAD_MAX      256
#define EDGE_CRYPTO_P256_SIZE    32
#define EDGE_CRYPTO_P384_SIZE    48

/* L1: Core crypto algorithm identifiers */
typedef enum {
    EDGE_CIPHER_AES_CTR_128,
    EDGE_CIPHER_AES_CTR_256,
    EDGE_CIPHER_AES_GCM_128,
    EDGE_CIPHER_AES_GCM_256,
    EDGE_CIPHER_CHACHA20_POLY1305
} EdgeCipherType;

typedef enum {
    EDGE_HASH_SHA256,
    EDGE_HASH_SHA384,
    EDGE_HASH_SHA512
} EdgeHashType;

typedef enum {
    EDGE_SIG_ECDSA_P256,
    EDGE_SIG_ECDSA_P384,
    EDGE_SIG_ED25519
} EdgeSigType;

typedef enum {
    EDGE_KDF_HKDF_SHA256,
    EDGE_KDF_PBKDF2_SHA256
} EdgeKdfType;

/* L1: AES-CTR context with counter management (NIST SP 800-38A) */
typedef struct {
    uint8_t key[EDGE_CRYPTO_KEY_MAX];
    int key_len;
    uint8_t counter[EDGE_CRYPTO_BLOCK_SIZE];
    uint8_t nonce[EDGE_CRYPTO_NONCE_SIZE];
    int nonce_len;
    uint64_t block_count;
    bool initialized;
} EdgeCryptoCtx;

/* L1: GCM authenticated encryption context (NIST SP 800-38D) */
typedef struct {
    uint8_t key[EDGE_CRYPTO_KEY_MAX];
    int key_len;
    uint8_t iv[EDGE_CRYPTO_IV_SIZE];
    int iv_len;
    uint8_t h_key[EDGE_CRYPTO_BLOCK_SIZE];
    uint8_t j0[EDGE_CRYPTO_BLOCK_SIZE];
    bool initialized;
} EdgeGcmCtx;

/* L1: ECDSA key pair (FIPS 186-4, curve P-256) */
typedef struct {
    uint8_t private_key[EDGE_CRYPTO_P256_SIZE];
    uint8_t public_key_x[EDGE_CRYPTO_P256_SIZE];
    uint8_t public_key_y[EDGE_CRYPTO_P256_SIZE];
    EdgeSigType curve_type;
    bool has_private;
} EdgeEcdsaKey;

/* L1: HKDF context (RFC 5869) */
typedef struct {
    uint8_t prk[EDGE_CRYPTO_HASH_SIZE];
    bool extracted;
    EdgeHashType hash_type;
} EdgeHkdfCtx;

/* --- AES-CTR (L5: NIST SP 800-38A CTR mode) --- */
void edge_crypto_init(EdgeCryptoCtx *ctx, const uint8_t *key, int key_len,
                      const uint8_t *nonce, int nonce_len);
void edge_crypto_reset_ctr(EdgeCryptoCtx *ctx);
void edge_aes_ctr_encrypt(EdgeCryptoCtx *ctx, const uint8_t *plain,
                          uint8_t *cipher, int len);
void edge_aes_ctr_decrypt(EdgeCryptoCtx *ctx, const uint8_t *cipher,
                          uint8_t *plain, int len);

/* --- AES-GCM (L5: NIST SP 800-38D authenticated encryption) --- */
void edge_gcm_init(EdgeGcmCtx *gcm, const uint8_t *key, int key_len,
                   const uint8_t *iv, int iv_len);
void edge_gcm_encrypt(EdgeGcmCtx *gcm, const uint8_t *plain, int plen,
                      const uint8_t *aad, int aad_len,
                      uint8_t *cipher, uint8_t tag[EDGE_CRYPTO_TAG_SIZE]);
bool edge_gcm_decrypt(EdgeGcmCtx *gcm, const uint8_t *cipher, int clen,
                      const uint8_t *aad, int aad_len,
                      const uint8_t tag[EDGE_CRYPTO_TAG_SIZE], uint8_t *plain);

/* --- SHA-256 (L4: FIPS 180-4) --- */
void edge_sha256(const uint8_t *data, int len, uint8_t digest[EDGE_CRYPTO_HASH_SIZE]);

/* --- HMAC-SHA256 (L4: RFC 2104 / FIPS 198-1) --- */
void edge_hmac_sha256(const uint8_t *key, int key_len, const uint8_t *msg,
                      int msg_len, uint8_t mac[EDGE_CRYPTO_HMAC_SIZE]);

/* --- HKDF (L5: RFC 5869, HMAC-based Extract-and-Expand) --- */
void edge_hkdf_extract(EdgeHkdfCtx *hkdf, const uint8_t *salt, int salt_len,
                       const uint8_t *ikm, int ikm_len);
int  edge_hkdf_expand(EdgeHkdfCtx *hkdf, const uint8_t *info, int info_len,
                      uint8_t *okm, int okm_len);
int  edge_hkdf_derive(const uint8_t *ikm, int ikm_len,
                      const uint8_t *salt, int salt_len,
                      const uint8_t *info, int info_len,
                      uint8_t *okm, int okm_len);

/* --- ECDSA P-256 (L5: FIPS 186-4 simplified prime-field ECDSA) --- */
void edge_ecdsa_keygen(EdgeEcdsaKey *key);
void edge_ecdsa_sign(const EdgeEcdsaKey *key, const uint8_t *hash, int hash_len,
                     uint8_t *sig, int *sig_len);
bool edge_ecdsa_verify(const EdgeEcdsaKey *key, const uint8_t *hash, int hash_len,
                       const uint8_t *sig, int sig_len);
void edge_ecdsa_derive_public(EdgeEcdsaKey *key);

/* --- ECDH Key Agreement (L5: NIST SP 800-56A) --- */
void edge_ecdh_compute_shared(const EdgeEcdsaKey *local_key,
                              const uint8_t *peer_pub_x,
                              const uint8_t *peer_pub_y,
                              uint8_t shared_secret[EDGE_CRYPTO_P256_SIZE]);

/* --- L8: Constant-time operations (side-channel resistance) --- */
int  edge_constant_time_memcmp(const uint8_t *a, const uint8_t *b, int len);
void edge_constant_time_copy(uint8_t *dst, const uint8_t *src, int len);
void edge_secure_zero(void *buf, int len);

/* --- L8: Timing-resistant modular exponentiation --- */
void edge_modexp_sidechannel_resistant(const uint8_t *base, int base_len,
                                       const uint8_t *exp, int exp_len,
                                       const uint8_t *mod, int mod_len,
                                       uint8_t *result, int *result_len);

/* --- Utility: XOR two buffers --- */
void edge_xor_buf(uint8_t *dst, const uint8_t *a, const uint8_t *b, int len);

#endif
