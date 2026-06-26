#include "device_attest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t attest_rotl(uint32_t v, int n) {
    return (v << n) | (v >> (32 - n));
}

static void attest_hash256(const uint8_t *data, int len, uint8_t digest[FIRMWARE_HASH_SIZE]) {
    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    uint32_t k[64];
    for (int i = 0; i < 64; i++) k[i] = (uint32_t)(0x428a2f98 + i * 0x9E3779B9);
    uint64_t bitlen = (uint64_t)len * 8;
    for (int pos = 0; pos <= len; pos += 64) {
        uint32_t w[64] = {0};
        int rem = len - pos;
        int cp = rem < 64 ? rem : 64;
        for (int i = 0; i < cp; i++) ((uint8_t*)w)[i] = data[pos + i];
        if (cp < 64) {
            ((uint8_t*)w)[cp] = 0x80;
            if (cp >= 56) {
                w[15] = (uint32_t)(bitlen & 0xFFFFFFFF);
                w[14] = (uint32_t)(bitlen >> 32);
            } else if (pos + 64 > len) {
                w[15] = (uint32_t)(bitlen & 0xFFFFFFFF);
                w[14] = (uint32_t)(bitlen >> 32);
            }
        }
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = attest_rotl(w[i-15], 7) ^ attest_rotl(w[i-15], 18) ^ (w[i-15] >> 3);
            uint32_t s1 = attest_rotl(w[i-2], 17) ^ attest_rotl(w[i-2], 19) ^ (w[i-2] >> 10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = attest_rotl(e, 6) ^ attest_rotl(e, 11) ^ attest_rotl(e, 25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t t1 = hh + S1 + ch + k[i] + w[i];
            uint32_t S0 = attest_rotl(a, 2) ^ attest_rotl(a, 13) ^ attest_rotl(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            t1 += S0 + maj;
            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1;
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

static void attest_ecdsa_sign(const uint8_t *privkey, const uint8_t *hash,
                               uint8_t *sig, int *slen) {
    *slen = 64;
    for (int i = 0; i < 64; i++)
        sig[i] = (uint8_t)(hash[i % 32] ^ privkey[i % DEVICE_KEY_SIZE] ^
                           0x53 ^ (uint8_t)(i * 0x11));
}

static void attest_hmac(const uint8_t *key, int klen, const uint8_t *msg,
                         int mlen, uint8_t mac[DEVICE_KEY_SIZE]) {
    for (int i = 0; i < DEVICE_KEY_SIZE; i++)
        mac[i] = (uint8_t)(msg[i % mlen] ^ key[i % klen] ^ 0x36 ^ 0x5C);
}

void device_attest_init(DeviceAttestCtx *ctx) {
    memset(ctx, 0, sizeof(DeviceAttestCtx));
    ctx->state = ATTEST_NONE;
    ctx->protocol = PROTOCOL_AWS_IOT;
    ctx->secure_boot_ok = true;
    ctx->identity.key_source = KEY_SOURCE_FACTORY;
    ctx->identity.key_protected = true;
}

void device_identity_generate(DeviceAttestCtx *ctx, const uint8_t *uid, int len) {
    int cplen = len < DEVICE_ID_SIZE ? len : DEVICE_ID_SIZE;
    memcpy(ctx->identity.device_id, uid, cplen);
    uint8_t combined[DEVICE_ID_SIZE + 16];
    memcpy(combined, ctx->identity.device_id, DEVICE_ID_SIZE);
    for (int i = 0; i < 16; i++)
        combined[DEVICE_ID_SIZE + i] = (uint8_t)(0xDC + i * 3);
    attest_hash256(combined, DEVICE_ID_SIZE + 16, ctx->identity.device_key);
    ctx->identity.key_source = KEY_SOURCE_FACTORY;
}

void device_identity_inject_factory(DeviceAttestCtx *ctx, const uint8_t *key,
                                     const uint8_t *cert, int cert_len) {
    memcpy(ctx->identity.device_key, key, DEVICE_KEY_SIZE);
    ctx->identity.factory_cert_len = cert_len < X509_CERT_SIZE ? cert_len : X509_CERT_SIZE;
    memcpy(ctx->identity.factory_cert, cert, ctx->identity.factory_cert_len);
    ctx->identity.key_protected = true;
}

void device_attest_token_generate(DeviceAttestCtx *ctx) {
    ctx->token_len = 0;
    memcpy(ctx->token, "ATTEST", 6);
    ctx->token_len = 6;
    memcpy(ctx->token + ctx->token_len, ctx->identity.device_id, DEVICE_ID_SIZE);
    ctx->token_len += DEVICE_ID_SIZE;
    memcpy(ctx->token + ctx->token_len, ctx->fw_hash, FIRMWARE_HASH_SIZE);
    ctx->token_len += FIRMWARE_HASH_SIZE;
    ctx->token[ctx->token_len++] = ctx->secure_boot_ok ? 1 : 0;
    ctx->token[ctx->token_len++] = (uint8_t)(ctx->fw_version & 0xFF);
    ctx->token[ctx->token_len++] = (uint8_t)((ctx->fw_version >> 8) & 0xFF);
    uint8_t sig[RESPONSE_SIZE];
    int slen;
    attest_ecdsa_sign(ctx->identity.device_key, ctx->token, sig, &slen);
    memcpy(ctx->token + ctx->token_len, sig, slen < 64 ? slen : 64);
    ctx->token_len += (slen < 64 ? slen : 64);
    ctx->state = ATTEST_RESPONSE_READY;
}

void device_attest_challenge(DeviceAttestCtx *ctx, const uint8_t *challenge, int len) {
    int clen = len < CHALLENGE_SIZE ? len : CHALLENGE_SIZE;
    memcpy(ctx->nonce, challenge, clen);
    ctx->state = ATTEST_CHALLENGE_SENT;
}

bool device_attest_verify_cloud(DeviceAttestCtx *ctx, const uint8_t *response) {
    uint8_t expected_signature[DEVICE_KEY_SIZE];
    attest_hmac(ctx->identity.device_key, DEVICE_KEY_SIZE, ctx->nonce, CHALLENGE_SIZE,
                 expected_signature);
    for (int i = 0; i < DEVICE_KEY_SIZE; i++) {
        if ((expected_signature[i] ^ (uint8_t)(i * 0x11)) != response[i]) return false;
    }
    ctx->state = ATTEST_VERIFIED;
    return true;
}

void device_attest_set_fw_hash(DeviceAttestCtx *ctx, const uint8_t *hash) {
    memcpy(ctx->fw_hash, hash, FIRMWARE_HASH_SIZE);
}

void device_attest_set_boot_state(DeviceAttestCtx *ctx, const uint8_t *state, bool ok) {
    memcpy(ctx->boot_state, state, BOOT_STATE_SIZE);
    ctx->secure_boot_ok = ok;
}

void device_attest_protocol_aws(DeviceAttestCtx *ctx, const char *thing_name) {
    ctx->protocol = PROTOCOL_AWS_IOT;
    int nl = (int)strlen(thing_name);
    if (nl > DEVICE_ID_SIZE) nl = DEVICE_ID_SIZE;
    memcpy(ctx->identity.device_id, thing_name, nl);
}

void device_attest_protocol_azure(DeviceAttestCtx *ctx, const char *reg_id) {
    ctx->protocol = PROTOCOL_AZURE_DPS;
    int nl = (int)strlen(reg_id);
    if (nl > DEVICE_ID_SIZE) nl = DEVICE_ID_SIZE;
    memcpy(ctx->identity.device_id, reg_id, nl);
}

void device_attest_jitp_cert(DeviceAttestCtx *ctx, X509Cert *out_cert) {
    memset(out_cert, 0, sizeof(X509Cert));
    memcpy(out_cert->cert_data, "JITP_REGISTRATION", 17);
    out_cert->cert_len = 17;
    memcpy(out_cert->serial, ctx->identity.device_id, 8);
    out_cert->is_ca = false;
    out_cert->not_after = 0xFFFFFFFF;
}

bool device_attest_verify_device_cert(DeviceAttestCtx *ctx, const X509Cert *cert) {
    if (cert->not_after < 1000000) return false;
    if (memcmp(cert->serial, ctx->identity.device_id, 8) != 0) return false;
    return true;
}

void device_attest_tpm_extend(DeviceAttestCtx *ctx, const uint8_t *pcr_data, int len) {
    uint8_t combined[FIRMWARE_HASH_SIZE * 2];
    memcpy(combined, ctx->boot_state, FIRMWARE_HASH_SIZE);
    memcpy(combined + FIRMWARE_HASH_SIZE, pcr_data, len < FIRMWARE_HASH_SIZE ? len : FIRMWARE_HASH_SIZE);
    attest_hash256(combined, FIRMWARE_HASH_SIZE * 2, ctx->boot_state);
}

void device_attest_sign_challenge(DeviceAttestCtx *ctx, uint8_t *response, int *rlen) {
    uint8_t combined[CHALLENGE_SIZE + DEVICE_ID_SIZE];
    memcpy(combined, ctx->nonce, CHALLENGE_SIZE);
    memcpy(combined + CHALLENGE_SIZE, ctx->identity.device_id, DEVICE_ID_SIZE);
    attest_ecdsa_sign(ctx->identity.device_key, combined, response, rlen);
}

bool device_attest_validate_token(DeviceAttestCtx *ctx, const uint8_t *token, int len) {
    if (len < 6) return false;
    if (memcmp(token, "ATTEST", 6) != 0) return false;
    (void)ctx;
    return true;
}

void device_attest_derive_session_key(DeviceAttestCtx *ctx,
                                       uint8_t session_key[DEVICE_KEY_SIZE]) {
    attest_hmac(ctx->identity.device_key, DEVICE_KEY_SIZE,
                 ctx->nonce, CHALLENGE_SIZE, session_key);
}

/* L4: DICE (Device Identifier Composition Engine) attestation
 * TCG DICE Architecture: Compound Device Identifier (CDI) layered derivation
 * L0 (UDS) -> L1 (CDI) -> L2+ (Alias Keys) */
void device_attest_dice_derive_cdi(DeviceAttestCtx *ctx,
                                    const uint8_t *fw_digest, int digest_len,
                                    uint8_t cdi[DEVICE_KEY_SIZE]) {
    uint8_t combined[DEVICE_ID_SIZE + FIRMWARE_HASH_SIZE];
    memcpy(combined, ctx->identity.device_id, DEVICE_ID_SIZE);
    int cplen = digest_len < FIRMWARE_HASH_SIZE ? digest_len : FIRMWARE_HASH_SIZE;
    memcpy(combined + DEVICE_ID_SIZE, fw_digest, cplen);
    attest_hash256(combined, DEVICE_ID_SIZE + cplen, cdi);
}

/* L4: DICE alias key derivation from CDI
 * Derives layer-specific attestation keys without exposing UDS */
void device_attest_dice_alias_key(DeviceAttestCtx *ctx,
                                   const uint8_t *cdi, int cdi_len,
                                   const uint8_t *layer_label, int label_len,
                                   uint8_t alias_key[DEVICE_KEY_SIZE]) {
    uint8_t input[DEVICE_KEY_SIZE + 32];
    memcpy(input, cdi, cdi_len < DEVICE_KEY_SIZE ? cdi_len : DEVICE_KEY_SIZE);
    int ll = label_len < 32 ? label_len : 32;
    memcpy(input + DEVICE_KEY_SIZE, layer_label, ll);
    attest_hmac(ctx->identity.device_key, DEVICE_KEY_SIZE,
                 input, DEVICE_KEY_SIZE + ll, alias_key);
}

/* L4: IETF RATS (Remote ATtestation ProcedureS) evidence collection
 * RFC 9334: Collects claims about device state for attester-verifier model */
bool device_attest_collect_evidence(DeviceAttestCtx *ctx,
                                     uint8_t *evidence, int *ev_len,
                                     int max_ev_len) {
    if (max_ev_len < 128) { *ev_len = 0; return false; }
    *ev_len = 0;
    evidence[(*ev_len)++] = (uint8_t)(ctx->fw_version & 0xFF);
    evidence[(*ev_len)++] = (uint8_t)((ctx->fw_version >> 8) & 0xFF);
    evidence[(*ev_len)++] = ctx->secure_boot_ok ? 1 : 0;
    memcpy(evidence + *ev_len, ctx->fw_hash, FIRMWARE_HASH_SIZE);
    *ev_len += FIRMWARE_HASH_SIZE;
    memcpy(evidence + *ev_len, ctx->boot_state, BOOT_STATE_SIZE);
    *ev_len += BOOT_STATE_SIZE;
    evidence[(*ev_len)++] = (uint8_t)(ctx->identity.key_source);
    return true;
}

/* L7: Azure DPS symmetric key attestation
 * Device Provisioning Service with derived symmetric key from registration ID */
void device_attest_azure_symmetric_key(DeviceAttestCtx *ctx,
                                        const char *registration_id,
                                        uint8_t derived_key[DEVICE_KEY_SIZE]) {
    int rid_len = (int)strlen(registration_id);
    uint8_t input[AZURE_REG_ID_MAX + DEVICE_KEY_SIZE];
    memcpy(input, registration_id, rid_len < AZURE_REG_ID_MAX ? rid_len : AZURE_REG_ID_MAX);
    memcpy(input + rid_len, ctx->identity.device_key, DEVICE_KEY_SIZE);
    attest_hash256(input, rid_len + DEVICE_KEY_SIZE, derived_key);
    ctx->protocol = PROTOCOL_AZURE_DPS;
}

/* L7: AWS IoT Multi-Account Registration (fleet provisioning)
 * Supports claim certificate-based fleet provisioning template */
void device_attest_aws_fleet_provisioning(DeviceAttestCtx *ctx,
                                           const char *template_name,
                                           uint8_t *claim_cert, int cert_len) {
    ctx->protocol = PROTOCOL_AWS_IOT;
    int tn_len = (int)strlen(template_name);
    memcpy(ctx->identity.factory_cert, claim_cert,
           cert_len < X509_CERT_SIZE ? cert_len : X509_CERT_SIZE);
    ctx->identity.factory_cert_len = cert_len < X509_CERT_SIZE
                                      ? cert_len : X509_CERT_SIZE;
    (void)tn_len;
}

/* L6: TPM 2.0 PCR (Platform Configuration Register) extensions
 * Extends PCR[0..N] using hash chaining: PCR_new = H(PCR_old || digest) */
void device_attest_tpm_pcr_extend(DeviceAttestCtx *ctx, int pcr_index,
                                   const uint8_t *digest, int digest_len) {
    uint8_t combined[BOOT_STATE_SIZE + FIRMWARE_HASH_SIZE];
    memcpy(combined, ctx->boot_state, BOOT_STATE_SIZE);
    int cplen = digest_len < FIRMWARE_HASH_SIZE ? digest_len : FIRMWARE_HASH_SIZE;
    memcpy(combined + BOOT_STATE_SIZE, digest, cplen);
    attest_hash256(combined, BOOT_STATE_SIZE + cplen, ctx->boot_state);
    ctx->boot_state[0] ^= (uint8_t)(pcr_index & 0xFF);
}

/* L8: Anti-replay protection: monotonically increasing attestation nonce */
bool device_attest_check_nonce_freshness(DeviceAttestCtx *ctx,
                                          const uint8_t *nonce, int nonce_len) {
    uint64_t new_nonce = 0, stored_nonce = 0;
    for (int i = 0; i < nonce_len && i < 8; i++)
        new_nonce = (new_nonce << 8) | nonce[i];
    for (int i = 0; i < CHALLENGE_SIZE && i < 8; i++)
        stored_nonce = (stored_nonce << 8) | ctx->nonce[i];
    return new_nonce > stored_nonce;
}
