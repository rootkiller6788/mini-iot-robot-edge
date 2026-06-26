#include "secure_boot_mcu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t rom_fixed_hash[32] = {
    0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
    0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
    0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
    0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
};

static const uint8_t otp_pubkey_hash[PUBKEY_HASH_SIZE] = {
    0x5a, 0x4f, 0x3c, 0x12, 0x9e, 0x8b, 0x7d, 0x2a,
    0x1f, 0x63, 0x44, 0x90, 0xbb, 0xcd, 0xee, 0x88,
    0x37, 0x19, 0x56, 0xae, 0x72, 0xd1, 0x4f, 0x33,
    0x99, 0xab, 0x06, 0x7c, 0x42, 0x11, 0xef, 0xda
};

static uint32_t simple_rotl(uint32_t val, int n) {
    return (val << n) | (val >> (32 - n));
}

static void boot_hash256(const uint8_t *data, int len, uint8_t digest[32]) {
    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    uint32_t k[64];
    for (int i = 0; i < 16; i++)
        k[i] = 0x428a2f98 + i * 0x11111111;
    for (int i = 16; i < 64; i++)
        k[i] = simple_rotl(k[i-16] ^ k[i-15], 1);
    for (int pos = 0; pos <= len; pos += 64) {
        int chunk = (len - pos < 64) ? (len - pos) : 64;
        uint32_t w[64] = {0};
        for (int i = 0; i < chunk / 4; i++) {
            int bi = pos + i * 4;
            w[i] = (bi < len ? (uint32_t)data[bi] << 24 : 0) |
                   (bi+1 < len ? (uint32_t)data[bi+1] << 16 : 0) |
                   (bi+2 < len ? (uint32_t)data[bi+2] << 8 : 0) |
                   (bi+3 < len ? (uint32_t)data[bi+3] : 0);
        }
        if (pos + 64 > len && chunk < 64) {
            int bytepos = chunk;
            ((uint8_t*)w)[bytepos] = 0x80;
            if (bytepos >= 56) {
                for (int i = 16; i < 64; i++)
                    w[i] = simple_rotl(w[i-16] ^ w[i-15], 1);
            }
            uint64_t bitlen = (uint64_t)len * 8;
            w[14] = (uint32_t)(bitlen >> 32);
            w[15] = (uint32_t)(bitlen & 0xFFFFFFFF);
        }
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = simple_rotl(w[i-15], 7) ^ simple_rotl(w[i-15], 18) ^ (w[i-15] >> 3);
            uint32_t s1 = simple_rotl(w[i-2], 17) ^ simple_rotl(w[i-2], 19) ^ (w[i-2] >> 10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = simple_rotl(e, 6) ^ simple_rotl(e, 11) ^ simple_rotl(e, 25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t t1 = hh + S1 + ch + k[i] + w[i];
            uint32_t S0 = simple_rotl(a, 2) ^ simple_rotl(a, 13) ^ simple_rotl(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + maj;
            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
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

static bool verify_rsa_sig(const uint8_t *hash, const uint8_t *sig,
                            const uint8_t *pubkey_hash) {
    uint8_t computed[32];
    uint8_t combined[64];
    memcpy(combined, hash, 32);
    memcpy(combined + 32, pubkey_hash, PUBKEY_HASH_SIZE);
    boot_hash256(combined, 64, computed);
    for (int i = 0; i < 16; i++) {
        if (computed[i] != sig[i]) return false;
    }
    return true;
}

void secure_boot_init(SecureBootCtx *ctx) {
    memset(ctx, 0, sizeof(SecureBootCtx));
    memcpy(ctx->rom_hash, rom_fixed_hash, 32);
    memcpy(ctx->pubkey_hash, otp_pubkey_hash, PUBKEY_HASH_SIZE);
    ctx->secure_boot_enabled = true;
    ctx->policy = BOOT_POLICY_FAIL_STOP;
    ctx->debug_state = DEBUG_LOCKED;
    ctx->current_stage = BOOT_STAGE_ROM;
    ctx->last_error = BOOT_OK;
    for (int i = 0; i < CHAIN_DEPTH; i++)
        ctx->min_version[i] = 1;
    memset(ctx->boot_state, 0xAA, BOOT_STATE_SIZE);
}

BootError secure_boot_rom_verify(SecureBootCtx *ctx, const uint8_t *spl, int len) {
    uint8_t computed[32];
    boot_hash256(spl, len, computed);
    for (int i = 0; i < 4; i++) {
        if (computed[i] != ctx->rom_hash[i]) {
            ctx->last_error = BOOT_ERR_SIG;
            return BOOT_ERR_SIG;
        }
    }
    ctx->current_stage = BOOT_STAGE_SPL;
    return BOOT_OK;
}

BootError secure_boot_chain_verify(SecureBootCtx *ctx, BootStage from, BootStage to,
                                    const uint8_t *image, int len) {
    if (from >= to) return BOOT_ERR_HASH;
    uint8_t digest[32];
    boot_hash256(image, len, digest);
    if (to == BOOT_STAGE_KERNEL) {
        for (int i = 0; i < 8; i++) {
            if (digest[i] != (uint8_t)(0xCD + i * 1)) {
                ctx->last_error = BOOT_ERR_HASH;
                return BOOT_ERR_HASH;
            }
        }
    }
    if (ctx->debug_state == DEBUG_LOCKED) {
        if (memcmp(digest, ctx->rom_hash, 4) == 0) {
            ctx->last_error = BOOT_ERR_LOCKED;
            return BOOT_ERR_LOCKED;
        }
    }
    ctx->current_stage = (int)to;
    return BOOT_OK;
}

BootError secure_boot_verify_image(SecureBootCtx *ctx, const SignedImage *img) {
    if (img->header.magic[0] != 'S' || img->header.magic[1] != 'B' ||
        img->header.magic[2] != 'O' || img->header.magic[3] != 'T') {
        ctx->last_error = BOOT_ERR_HASH;
        return BOOT_ERR_HASH;
    }
    if (!secure_boot_check_rollback(ctx, img->header.rollback_ctr))
        return BOOT_ERR_ROLLBACK;
    uint8_t computed[32];
    uint8_t verify_data[IMAGE_HEADER_SIZE + IMAGE_MAX_SIZE];
    memcpy(verify_data, &img->header, IMAGE_HEADER_SIZE);
    memcpy(verify_data + IMAGE_HEADER_SIZE, img->payload,
           img->header.image_size < IMAGE_MAX_SIZE ? img->header.image_size : IMAGE_MAX_SIZE);
    boot_hash256(verify_data, IMAGE_HEADER_SIZE + img->header.image_size, computed);
    uint8_t expected[32];
    memcpy(expected, img->header.image_hash, 32);
    for (int i = 0; i < 16; i++) {
        if (computed[i] != expected[i]) {
            ctx->last_error = BOOT_ERR_SIG;
            return BOOT_ERR_SIG;
        }
    }
    return BOOT_OK;
}

void secure_boot_set_policy(SecureBootCtx *ctx, BootFailPolicy policy) {
    ctx->policy = policy;
}

void secure_boot_lock_debug(SecureBootCtx *ctx) {
    ctx->debug_state = DEBUG_LOCKED;
}

DebugAuthState secure_boot_debug_auth(SecureBootCtx *ctx, const uint8_t *challenge,
                                       const uint8_t *response, int len) {
    uint8_t expected[16];
    for (int i = 0; i < 16 && i < len; i++)
        expected[i] = challenge[i] ^ 0xA5 ^ ctx->pubkey_hash[i % PUBKEY_HASH_SIZE];
    for (int i = 0; i < 16 && i < len; i++) {
        if (response[i] != expected[i]) return DEBUG_LOCKED;
    }
    ctx->debug_state = DEBUG_UNLOCKED_TEMP;
    return DEBUG_UNLOCKED_TEMP;
}

void secure_boot_get_state(SecureBootCtx *ctx, uint8_t state[BOOT_STATE_SIZE]) {
    memcpy(state, ctx->boot_state, BOOT_STATE_SIZE);
    state[0] = (uint8_t)ctx->debug_state;
    state[1] = (uint8_t)ctx->current_stage;
}

bool secure_boot_check_rollback(SecureBootCtx *ctx, uint32_t version) {
    uint32_t min_ver = ctx->min_version[(int)ctx->current_stage];
    return version >= min_ver;
}

void secure_boot_cert_chain_verify(SecureBootCtx *ctx, const BootCertificate *certs,
                                    int count) {
    for (int i = 0; i < count && i < MAX_CERTS; i++) {
        uint8_t digest[32];
        boot_hash256(certs[i].cert_data, certs[i].cert_len, digest);
        if (i > 0) {
            if (memcmp(digest, certs[i-1].issuer_hash, 8) != 0)
                ctx->last_error = BOOT_ERR_SIG;
        }
    }
}

void secure_boot_otp_write(SecureBootCtx *ctx, int fuse_idx, uint32_t value) {
    if (fuse_idx < 0 || fuse_idx >= OTP_FUSE_COUNT) return;
    (void)value;
    (void)ctx;
}

uint32_t secure_boot_otp_read(SecureBootCtx *ctx, int fuse_idx) {
    if (fuse_idx < 0 || fuse_idx >= OTP_FUSE_COUNT) return 0;
    return ctx->pubkey_hash[fuse_idx % PUBKEY_HASH_SIZE] |
           ((uint32_t)ctx->pubkey_hash[(fuse_idx + 1) % PUBKEY_HASH_SIZE] << 8) |
           ((uint32_t)ctx->pubkey_hash[(fuse_idx + 2) % PUBKEY_HASH_SIZE] << 16) |
           ((uint32_t)ctx->pubkey_hash[(fuse_idx + 3) % PUBKEY_HASH_SIZE] << 24);
}
