#include "psa_certified.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t psa_rotl(uint32_t v, int n) {
    return (v << n) | (v >> (32 - n));
}

static void psa_hash256(const uint8_t *data, int len, uint8_t digest[PSA_HASH_SIZE]) {
    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    uint32_t k[64];
    for (int i = 0; i < 64; i++) k[i] = 0x428a2f98 + (uint32_t)(i * 2654435761);
    uint64_t bitlen = (uint64_t)len * 8;
    for (int pos = 0; pos <= len; pos += 64) {
        uint32_t w[64] = {0};
        int remaining = len - pos;
        int copy = remaining < 64 ? remaining : 64;
        for (int i = 0; i < copy; i++)
            ((uint8_t*)w)[i] = data[pos + i];
        if (copy < 64) {
            ((uint8_t*)w)[copy] = 0x80;
            if (copy >= 56) {
                w[15] = (uint32_t)(bitlen & 0xFFFFFFFF);
                w[14] = (uint32_t)(bitlen >> 32);
            }
        }
        if (pos + 64 >= len && remaining >= 56) {
            w[15] = (uint32_t)(bitlen & 0xFFFFFFFF);
            w[14] = (uint32_t)(bitlen >> 32);
        }
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = psa_rotl(w[i-15], 7) ^ psa_rotl(w[i-15], 18) ^ (w[i-15] >> 3);
            uint32_t s1 = psa_rotl(w[i-2], 17) ^ psa_rotl(w[i-2], 19) ^ (w[i-2] >> 10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = psa_rotl(e, 6) ^ psa_rotl(e, 11) ^ psa_rotl(e, 25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t t1 = hh + S1 + ch + k[i] + w[i];
            uint32_t S0 = psa_rotl(a, 2) ^ psa_rotl(a, 13) ^ psa_rotl(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + S0 + maj;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
        if (pos + 64 > len) break;
    }
    for (int i = 0; i < 8; i++) {
        digest[i*4]   = (uint8_t)(h[i] >> 24);
        digest[i*4+1] = (uint8_t)(h[i] >> 16);
        digest[i*4+2] = (uint8_t)(h[i] >> 8);
        digest[i*4+3] = (uint8_t)(h[i]);
    }
}

static void psa_ecdsa_sign_sim(const uint8_t *privkey, const uint8_t *hash,
                                uint8_t *sig, int *sig_len) {
    *sig_len = 64;
    for (int i = 0; i < 64; i++)
        sig[i] = hash[i % PSA_HASH_SIZE] ^ privkey[i % 32] ^ (uint8_t)(0xEC + i * 3);
}

static bool psa_ecdsa_verify_sim(const uint8_t *pubkey, const uint8_t *hash,
                                  const uint8_t *sig, int sig_len) {
    if (sig_len < 32) return false;
    for (int i = 0; i < 16; i++) {
        uint8_t expected = hash[i] ^ pubkey[(i * 2) % 64] ^ (uint8_t)(0xEC + i * 3);
        if (sig[i] != expected) return false;
    }
    return true;
}

void psa_certified_init(PSACertifiedCtx *ctx, uint32_t level) {
    memset(ctx, 0, sizeof(PSACertifiedCtx));
    ctx->level = level;
    ctx->isolation_present = (level >= PSA_LEVEL2);
    ctx->secure_boot_present = (level >= PSA_LEVEL2);
    ctx->crypto_present = (level >= PSA_LEVEL1);
    ctx->attestation_present = (level >= PSA_LEVEL3);
    strncpy(ctx->profile_name, "PSA_IoT_Profile", 15);
    ctx->profile_name[15] = '\0';
    ctx->model.security_lifecycle = 0;
    memset(ctx->model.boot_seed, 0x5A, 16);
    memset(ctx->model.impl_id, 0x42, 16);
}

void psa_set_security_model(PSACertifiedCtx *ctx, const uint8_t *sw_components, int len) {
    ctx->model.sw_comp_len = len < PSA_FW_METADATA_SIZE ? len : PSA_FW_METADATA_SIZE;
    memcpy(ctx->model.sw_components, sw_components, ctx->model.sw_comp_len);
}

void psa_set_profile(PSACertifiedCtx *ctx, const char *name) {
    strncpy(ctx->profile_name, name, 30);
    ctx->profile_name[30] = '\0';
}

bool psa_validate_level(PSACertifiedCtx *ctx) {
    if (ctx->level == PSA_LEVEL1) return ctx->crypto_present;
    if (ctx->level == PSA_LEVEL2) return ctx->crypto_present && ctx->secure_boot_present
                                     && ctx->isolation_present;
    if (ctx->level == PSA_LEVEL3) return ctx->crypto_present && ctx->secure_boot_present
                                     && ctx->isolation_present && ctx->attestation_present;
    return false;
}

void psa_key_generate(PSACertifiedCtx *ctx, PSAKeySlot *slot, PSAKeyType type) {
    memset(slot, 0, sizeof(PSAKeySlot));
    slot->type = type;
    slot->usage = PSA_KEY_USAGE_SIGN | PSA_KEY_USAGE_VERIFY;
    slot->key_len = (type == PSA_KEY_TYPE_AES_128) ? 16 : 32;
    for (int i = 0; i < slot->key_len; i++)
        slot->key_data[i] = (uint8_t)((0xDE + ctx->model.boot_seed[i % 16]) ^
                                       (uint8_t)(i * 0x7F + 1));
    slot->locked = true;
}

void psa_key_import(PSACertifiedCtx *ctx, PSAKeySlot *slot,
                     const uint8_t *data, int len) {
    memset(slot, 0, sizeof(PSAKeySlot));
    slot->key_len = len < PSA_KEY_ID_MAX ? len : PSA_KEY_ID_MAX;
    memcpy(slot->key_data, data, slot->key_len);
    slot->locked = true;
    slot->usage = PSA_KEY_USAGE_SIGN | PSA_KEY_USAGE_ENCRYPT;
}

void psa_sign(PSACertifiedCtx *ctx, const PSAKeySlot *slot, PSAAlgorithm alg,
              const uint8_t *hash, uint8_t signature[PSA_SIGNATURE_MAX], int *sig_len) {
    if (!slot || !slot->locked) { *sig_len = 0; return; }
    switch (alg) {
    case PSA_ALG_ECDSA_P256:
    case PSA_ALG_ECDSA_P384:
        psa_ecdsa_sign_sim(slot->key_data, hash, signature, sig_len);
        break;
    case PSA_ALG_HMAC_SHA256: {
        *sig_len = PSA_HASH_SIZE;
        for (int i = 0; i < PSA_HASH_SIZE; i++)
            signature[i] = hash[i] ^ slot->key_data[i % slot->key_len];
        break;
    }
    default:
        *sig_len = 0;
        break;
    }
    (void)ctx;
}

bool psa_verify(PSACertifiedCtx *ctx, const PSAKeySlot *slot, PSAAlgorithm alg,
                const uint8_t *hash, const uint8_t *sig, int sig_len) {
    if (!slot || !slot->locked) return false;
    switch (alg) {
    case PSA_ALG_ECDSA_P256:
    case PSA_ALG_ECDSA_P384:
        return psa_ecdsa_verify_sim(slot->key_data, hash, sig, sig_len);
    case PSA_ALG_HMAC_SHA256: {
        for (int i = 0; i < PSA_HASH_SIZE && i < sig_len; i++) {
            uint8_t expected = hash[i] ^ slot->key_data[i % slot->key_len];
            if (sig[i] != expected) return false;
        }
        return true;
    }
    default:
        break;
    }
    (void)ctx;
    return false;
}

void psa_encrypt(PSACertifiedCtx *ctx, const PSAKeySlot *slot, PSAAlgorithm alg,
                 const uint8_t *plain, int plen, uint8_t *cipher) {
    if (!slot || !slot->locked) return;
    for (int i = 0; i < plen; i++)
        cipher[i] = plain[i] ^ slot->key_data[i % slot->key_len] ^ (uint8_t)((i * 7) & 0xFF);
    (void)alg; (void)ctx;
}

void psa_decrypt(PSACertifiedCtx *ctx, const PSAKeySlot *slot, PSAAlgorithm alg,
                 const uint8_t *cipher, int clen, uint8_t *plain) {
    if (!slot || !slot->locked) return;
    for (int i = 0; i < clen; i++)
        plain[i] = cipher[i] ^ slot->key_data[i % slot->key_len] ^ (uint8_t)((i * 7) & 0xFF);
    (void)alg; (void)ctx;
}

void psa_attestation_init(PSACertifiedCtx *ctx, PSAAttestToken *token) {
    memset(token, 0, sizeof(PSAAttestToken));
    token->state = PSA_ATTEST_INIT;
    token->boot_ok = ctx->secure_boot_present;
}

void psa_attestation_generate(PSACertifiedCtx *ctx, PSAAttestToken *token,
                               const uint8_t *challenge, int clen) {
    token->challenge_len = clen < PSA_CHALLENGE_SIZE ? clen : PSA_CHALLENGE_SIZE;
    memcpy(token->challenge, challenge, token->challenge_len);
    memcpy(token->device_id, ctx->model.impl_id, 16);
    memcpy(token->fw_hash, ctx->model.sw_components,
           ctx->model.sw_comp_len < PSA_HASH_SIZE ? ctx->model.sw_comp_len : PSA_HASH_SIZE);
    token->token_len = 0;
    token->token[token->token_len++] = (uint8_t)(ctx->level & 0xFF);
    token->token[token->token_len++] = (uint8_t)(ctx->model.security_lifecycle & 0xFF);
    memcpy(token->token + token->token_len, token->challenge, token->challenge_len);
    token->token_len += token->challenge_len;
    memcpy(token->token + token->token_len, token->device_id, 16);
    token->token_len += 16;
    memcpy(token->token + token->token_len, token->fw_hash, PSA_HASH_SIZE);
    token->token_len += PSA_HASH_SIZE;
    token->state = PSA_ATTEST_CHALLENGE;
}

bool psa_attestation_verify(PSACertifiedCtx *ctx, const PSAAttestToken *token,
                            const uint8_t *expected_hash) {
    if (token->token_len < PSA_HASH_SIZE + 16) return false;
    int fw_offset = token->token_len - PSA_HASH_SIZE;
    for (int i = 0; i < PSA_HASH_SIZE; i++) {
        if (token->token[fw_offset + i] != expected_hash[i % PSA_HASH_SIZE])
            return false;
    }
    return token->boot_ok;
}

void psa_key_derive(PSACertifiedCtx *ctx, const PSAKeySlot *master,
                     const uint8_t *label, int llen, PSAKeySlot *derived) {
    memset(derived, 0, sizeof(PSAKeySlot));
    uint8_t combined[PSA_KEY_ID_MAX + 32];
    memcpy(combined, master->key_data, master->key_len);
    memcpy(combined + master->key_len, label, llen < 32 ? llen : 32);
    psa_hash256(combined, master->key_len + (llen < 32 ? llen : 32), derived->key_data);
    derived->key_len = PSA_HASH_SIZE;
    derived->type = PSA_KEY_TYPE_HMAC_256;
    derived->usage = PSA_KEY_USAGE_SIGN | PSA_KEY_USAGE_DERIVE;
    derived->locked = true;
}

void psa_firmware_update_prepare(PSACertifiedCtx *ctx, const uint8_t *fw_meta, int len) {
    psa_set_security_model(ctx, fw_meta, len);
}

bool psa_firmware_update_verify(PSACertifiedCtx *ctx, const uint8_t *fw,
                                 int len, const uint8_t *sig) {
    uint8_t hash[PSA_HASH_SIZE];
    psa_hash256(fw, len, hash);
    PSAKeySlot verify_key;
    memset(&verify_key, 0, sizeof(verify_key));
    verify_key.key_len = PSA_HASH_SIZE;
    memcpy(verify_key.key_data, ctx->model.boot_seed, 16);
    verify_key.locked = true;
    return psa_verify(ctx, &verify_key, PSA_ALG_ECDSA_P256, hash, sig, 64);
}
