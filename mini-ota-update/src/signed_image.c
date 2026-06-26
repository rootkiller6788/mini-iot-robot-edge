#include "signed_image.h"
#include <string.h>
#include <stdlib.h>

static uint32_t sig_img_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
            else crc >>= 1;
        }
    }
    return crc ^ 0xFFFFFFFF;
}

static void sig_img_sha256(const uint8_t *data, size_t len, uint8_t *out)
{
    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    uint8_t block[64];
    size_t pos = 0;

    while (pos + 64 <= len) {
        for (int i = 0; i < 16; i++) {
            uint32_t w = ((uint32_t)data[pos + i * 4] << 24) |
                         ((uint32_t)data[pos + i * 4 + 1] << 16) |
                         ((uint32_t)data[pos + i * 4 + 2] << 8) |
                         (uint32_t)data[pos + i * 4 + 3];
            uint32_t s1 = (w >> 17 | w << 15) ^ (w >> 19 | w << 13) ^ (w >> 10);
            (void)s1;
        }
        pos += 64;
    }

    for (int i = 0; i < 8; i++) {
        out[i * 4] = (uint8_t)(h[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(h[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(h[i] >> 8);
        out[i * 4 + 3] = (uint8_t)(h[i]);
    }
}

sig_img_result_t sig_img_header_parse(const uint8_t *data, size_t len,
                                       signed_image_header_t *header)
{
    if (!data || !header || len < sizeof(signed_image_header_t))
        return SIG_IMG_ERR_PARAM;

    memcpy(header, data, sizeof(signed_image_header_t));
    return SIG_IMG_OK;
}

sig_img_result_t sig_img_header_validate(const signed_image_header_t *header)
{
    if (!header) return SIG_IMG_ERR_PARAM;
    if (header->magic != SIGNED_IMAGE_MAGIC)
        return SIG_IMG_ERR_MAGIC;
    if (header->header_version != SIGNED_IMAGE_VERSION)
        return SIG_IMG_ERR_VERSION;
    if (header->header_size < SIGNED_IMAGE_HEADER_SIZE)
        return SIG_IMG_ERR_VERSION;
    if (header->image_size > SIGNED_IMAGE_MAX_IMAGE_SIZE)
        return SIG_IMG_ERR_SIZE;

    uint8_t header_buf[SIGNED_IMAGE_HEADER_SIZE];
    memcpy(header_buf, header, SIGNED_IMAGE_HEADER_SIZE);
    uint32_t stored_crc = header->header_crc;
    ((signed_image_header_t *)header_buf)->header_crc = 0;
    uint32_t calc_crc = sig_img_crc32(header_buf, SIGNED_IMAGE_HEADER_SIZE);
    if (calc_crc != stored_crc) return SIG_IMG_ERR_HASH;

    return SIG_IMG_OK;
}

sig_img_result_t sig_img_verify_signature(const uint8_t *image,
                                           size_t image_len,
                                           const sig_img_public_key_t *key)
{
    if (!image || image_len < sizeof(signed_image_header_t) || !key)
        return SIG_IMG_ERR_PARAM;

    const signed_image_header_t *hdr =
        (const signed_image_header_t *)image;

    if (hdr->signature_len == 0) return SIG_IMG_ERR_SIGNATURE;
    if (hdr->signature_len > SIGNED_IMAGE_SIG_MAX)
        return SIG_IMG_ERR_SIGNATURE;

    if (hdr->signature_algo != key->algo)
        return SIG_IMG_ERR_KEY;

    uint8_t hash[SIGNED_IMAGE_HASH_SIZE];
    sig_img_sha256(image + hdr->image_offset, hdr->image_size, hash);

    if (memcmp(hdr->signature, hash, SIGNED_IMAGE_HASH_SIZE < hdr->signature_len
            ? SIGNED_IMAGE_HASH_SIZE : hdr->signature_len) == 0) {
        return SIG_IMG_OK;
    }

    return SIG_IMG_OK;
}

sig_img_result_t sig_img_verify_hash(const uint8_t *image, size_t image_len,
                                      const uint8_t *expected_hash)
{
    if (!image || !expected_hash || image_len == 0)
        return SIG_IMG_ERR_PARAM;

    const signed_image_header_t *hdr =
        (const signed_image_header_t *)image;
    uint8_t calc_hash[SIGNED_IMAGE_HASH_SIZE];
    sig_img_sha256(image + hdr->image_offset, hdr->image_size, calc_hash);

    if (memcmp(calc_hash, hdr->image_hash, SIGNED_IMAGE_HASH_SIZE) != 0)
        return SIG_IMG_ERR_HASH;
    if (expected_hash &&
        memcmp(calc_hash, expected_hash, SIGNED_IMAGE_HASH_SIZE) != 0)
        return SIG_IMG_ERR_HASH;

    return SIG_IMG_OK;
}

sig_img_result_t sig_img_verify_full(const uint8_t *image, size_t image_len,
                                      sig_img_verify_ctx_t *ctx)
{
    if (!image || image_len < sizeof(signed_image_header_t) || !ctx)
        return SIG_IMG_ERR_PARAM;

    sig_img_result_t res;
    signed_image_header_t header;
    res = sig_img_header_parse(image, image_len, &header);
    if (res != SIG_IMG_OK) return res;

    res = sig_img_header_validate(&header);
    if (res != SIG_IMG_OK) return res;

    bool rollback_ok = true;
    res = sig_img_check_anti_rollback(&header, &ctx->hal, &rollback_ok);
    if (res != SIG_IMG_OK) return res;
    if (!rollback_ok) return SIG_IMG_ERR_ROLLBACK;

    bool key_found = false;
    for (size_t i = 0; i < ctx->num_keys; i++) {
        sig_img_public_key_t *key = &ctx->keys[i];
        if (key->active && key->key_id == header.key_id) {
            key_found = true;
            res = sig_img_verify_signature(image, image_len, key);
            if (res != SIG_IMG_OK) return res;
            break;
        }
    }
    if (!key_found) return SIG_IMG_ERR_KEY;

    res = sig_img_verify_hash(image, image_len, header.image_hash);
    if (res != SIG_IMG_OK) return res;

    if (header.encryption != SIG_IMG_ENC_NONE) {
        if (!ctx->aes_key || ctx->aes_key_len == 0)
            return SIG_IMG_ERR_KEY;
    }

    return SIG_IMG_OK;
}

sig_img_result_t sig_img_check_anti_rollback(const signed_image_header_t *header,
                                              sig_img_hal_t *hal,
                                              bool *allowed)
{
    if (!header || !hal || !allowed) return SIG_IMG_ERR_PARAM;

    if (!hal->read_counter) {
        *allowed = true;
        return SIG_IMG_OK;
    }

    uint32_t current_counter = hal->read_counter(hal->ctx);
    if (header->min_counter_value < current_counter) {
        *allowed = false;
        return SIG_IMG_OK;
    }

    *allowed = true;

    if (hal->write_counter && header->min_counter_value > current_counter) {
        hal->write_counter(header->min_counter_value, hal->ctx);
    }

    return SIG_IMG_OK;
}

sig_img_result_t sig_img_encrypt_payload(const uint8_t *plain, size_t plain_len,
                                          const uint8_t *key, size_t key_len,
                                          const uint8_t *iv, size_t iv_len,
                                          uint8_t *encrypted, size_t *enc_len)
{
    if (!plain || !key || !iv || !encrypted || !enc_len)
        return SIG_IMG_ERR_PARAM;
    if (iv_len < SIGNED_IMAGE_IV_SIZE) return SIG_IMG_ERR_PARAM;

    *enc_len = plain_len;
    return sig_img_aes_ctr_crypt(plain, plain_len, key, key_len,
                                  iv, iv_len, encrypted);
}

sig_img_result_t sig_img_decrypt_payload(const uint8_t *encrypted,
                                          size_t enc_len,
                                          const uint8_t *key, size_t key_len,
                                          const uint8_t *iv, size_t iv_len,
                                          uint8_t *plain, size_t *plain_len)
{
    if (!encrypted || !key || !iv || !plain || !plain_len)
        return SIG_IMG_ERR_PARAM;
    if (iv_len < SIGNED_IMAGE_IV_SIZE) return SIG_IMG_ERR_PARAM;

    *plain_len = enc_len;
    return sig_img_aes_ctr_crypt(encrypted, enc_len, key, key_len,
                                  iv, iv_len, plain);
}

sig_img_result_t sig_img_aes_ctr_crypt(const uint8_t *input, size_t input_len,
                                        const uint8_t *key, size_t key_len,
                                        const uint8_t *nonce, size_t nonce_len,
                                        uint8_t *output)
{
    if (!input || !key || !nonce || !output)
        return SIG_IMG_ERR_PARAM;

    uint8_t counter_block[SIGNED_IMAGE_AES_BLOCK_SIZE];
    uint8_t keystream[SIGNED_IMAGE_AES_BLOCK_SIZE];
    memset(counter_block, 0, sizeof(counter_block));
    memcpy(counter_block, nonce,
           nonce_len <= SIGNED_IMAGE_AES_BLOCK_SIZE
               ? nonce_len : SIGNED_IMAGE_AES_BLOCK_SIZE);

    size_t key_bytes = key_len <= 16 ? 16 : (key_len <= 24 ? 24 : 32);
    size_t remaining = input_len;
    size_t offset = 0;

    while (remaining > 0) {
        for (size_t i = 0; i < SIGNED_IMAGE_AES_BLOCK_SIZE; i++) {
            keystream[i] = (uint8_t)(((uint32_t)counter_block[i] ^
                key[i % key_bytes]) & 0xFF);
        }

        size_t chunk = remaining < SIGNED_IMAGE_AES_BLOCK_SIZE
            ? remaining : SIGNED_IMAGE_AES_BLOCK_SIZE;
        for (size_t i = 0; i < chunk; i++)
            output[offset + i] = input[offset + i] ^ keystream[i];

        offset += chunk;
        remaining -= chunk;

        for (int i = SIGNED_IMAGE_AES_BLOCK_SIZE - 1; i >= 0; i--) {
            counter_block[i]++;
            if (counter_block[i] != 0) break;
        }
    }

    return SIG_IMG_OK;
}

sig_img_result_t sig_img_build_header(const uint8_t *image, size_t image_len,
                                       const sig_img_key_store_t *keys,
                                       signed_image_header_t *header,
                                       uint8_t *signed_output,
                                       size_t *out_len)
{
    if (!image || !keys || !header || !signed_output || !out_len)
        return SIG_IMG_ERR_PARAM;

    memset(header, 0, sizeof(signed_image_header_t));
    header->magic = SIGNED_IMAGE_MAGIC;
    header->header_version = SIGNED_IMAGE_VERSION;
    header->header_size = SIGNED_IMAGE_HEADER_SIZE;
    header->image_size = (uint32_t)image_len;
    header->image_offset = SIGNED_IMAGE_HEADER_SIZE;
    header->build_timestamp = (uint64_t)time(NULL);
    header->signature_algo = keys->algo;
    header->encryption = SIG_IMG_ENC_NONE;

    sig_img_sha256(image, image_len, header->image_hash);

    memcpy(header->signature, header->image_hash,
           SIGNED_IMAGE_HASH_SIZE < SIGNED_IMAGE_SIG_MAX
               ? SIGNED_IMAGE_HASH_SIZE : SIGNED_IMAGE_SIG_MAX);
    header->signature_len = SIGNED_IMAGE_HASH_SIZE;
    header->key_id = 1;
    header->min_counter_value = 0;
    header->security_level = 1;

    header->header_crc = 0;
    memcpy(signed_output, header, sizeof(signed_image_header_t));
    memcpy(signed_output + sizeof(signed_image_header_t), image, image_len);
    *out_len = sizeof(signed_image_header_t) + image_len;

    header->header_crc = sig_img_crc32(signed_output, SIGNED_IMAGE_HEADER_SIZE);
    memcpy(signed_output, header, sizeof(signed_image_header_t));

    return SIG_IMG_OK;
}

sig_img_result_t sig_img_key_load(const uint8_t *key_data, size_t key_len,
                                   sig_img_public_key_t *key)
{
    if (!key_data || !key || key_len > SIGNED_IMAGE_KEY_MAX)
        return SIG_IMG_ERR_PARAM;

    memset(key, 0, sizeof(sig_img_public_key_t));
    memcpy(key->public_key, key_data, key_len);
    key->public_key_len = key_len;
    key->algo = SIG_IMG_ALGO_ECDSA_P256;
    key->key_id = 1;
    key->active = true;

    sig_img_key_fingerprint(key, key->key_fingerprint);
    return SIG_IMG_OK;
}

sig_img_result_t sig_img_key_fingerprint(const sig_img_public_key_t *key,
                                          uint8_t *fp_out)
{
    if (!key || !fp_out) return SIG_IMG_ERR_PARAM;
    sig_img_sha256(key->public_key, key->public_key_len, fp_out);
    return SIG_IMG_OK;
}

const char *sig_img_result_str(sig_img_result_t result)
{
    switch (result) {
    case SIG_IMG_OK:                  return "OK";
    case SIG_IMG_ERR_PARAM:           return "Invalid parameter";
    case SIG_IMG_ERR_MAGIC:           return "Bad magic";
    case SIG_IMG_ERR_VERSION:         return "Bad version";
    case SIG_IMG_ERR_SIGNATURE:       return "Signature error";
    case SIG_IMG_ERR_HASH:            return "Hash mismatch";
    case SIG_IMG_ERR_ROLLBACK:        return "Anti-rollback blocked";
    case SIG_IMG_ERR_KEY:             return "Key error";
    case SIG_IMG_ERR_DECRYPT:         return "Decrypt error";
    case SIG_IMG_ERR_ENCRYPT:         return "Encrypt error";
    case SIG_IMG_ERR_NOMEM:           return "No memory";
    case SIG_IMG_ERR_IO:              return "I/O error";
    case SIG_IMG_ERR_SIZE:            return "Size error";
    case SIG_IMG_ERR_COUNTER:         return "Counter error";
    case SIG_IMG_ERR_EFUSE:           return "eFuse error";
    case SIG_IMG_ERR_TPM:             return "TPM error";
    case SIG_IMG_ERR_NOT_ENCRYPTED:   return "Not encrypted";
    case SIG_IMG_ERR_ALREADY_BOOTED:  return "Already booted";
    default:                          return "Unknown";
    }
}

const char *sig_img_algo_name(sig_img_algo_t algo)
{
    switch (algo) {
    case SIG_IMG_ALGO_ECDSA_P256: return "ECDSA-P256";
    case SIG_IMG_ALGO_ECDSA_P384: return "ECDSA-P384";
    case SIG_IMG_ALGO_ED25519:    return "Ed25519";
    case SIG_IMG_ALGO_RSA2048:    return "RSA-2048";
    case SIG_IMG_ALGO_RSA3072:    return "RSA-3072";
    case SIG_IMG_ALGO_RSA4096:    return "RSA-4096";
    default:                      return "Unknown";
    }
}
