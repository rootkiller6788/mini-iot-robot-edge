#include "edge_ota.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* L5: OTA context initialization */
void edge_ota_init(OtaUpdateCtx *ctx, const uint8_t *dev_id, int id_len) {
    memset(ctx, 0, sizeof(OtaUpdateCtx));
    ctx->current_version = 1;
    ctx->active_bank = 0;
    ctx->update_state = OTA_IDLE;
    ctx->max_retries = 3;
    int cplen = id_len < OTA_DEV_ID_SIZE ? id_len : OTA_DEV_ID_SIZE;
    memcpy(ctx->device_id, dev_id, cplen);
    ctx->banks[0].base_addr = 0x08000000;
    ctx->banks[0].size = OTA_FLASH_BANK_SIZE;
    ctx->banks[0].version = 1;
    ctx->banks[0].active = true;
    ctx->banks[0].valid = true;
    ctx->banks[1].base_addr = 0x08010000;
    ctx->banks[1].size = OTA_FLASH_BANK_SIZE;
    ctx->banks[1].version = 0;
    ctx->banks[1].active = false;
    ctx->banks[1].valid = false;
}

/* L5: Manifest initialization */
void edge_ota_manifest_init(OtaManifest *m, uint32_t version, uint32_t size) {
    memset(m, 0, sizeof(OtaManifest));
    m->fw_version = version;
    m->fw_size = size;
    m->timestamp = (uint32_t)(version * 1000000 + size);
    m->min_bootloader_ver = 1;
    m->metadata_len = 0;
}

void edge_ota_manifest_set_hash(OtaManifest *m, const uint8_t *hash) {
    memcpy(m->fw_hash, hash, OTA_FW_HASH_SIZE);
}

/* L5: Manifest signature (simulated ECDSA over manifest fields) */
void edge_ota_manifest_sign(OtaManifest *m, const uint8_t *priv_key) {
    uint8_t sign_input[OTA_MANIFEST_SIZE + 64];
    int slen = 0;
    memcpy(sign_input, &m->fw_version, 4); slen += 4;
    memcpy(sign_input + slen, &m->fw_size, 4); slen += 4;
    memcpy(sign_input + slen, m->fw_hash, OTA_FW_HASH_SIZE); slen += OTA_FW_HASH_SIZE;
    memcpy(sign_input + slen, m->device_id, OTA_DEV_ID_SIZE); slen += OTA_DEV_ID_SIZE;
    for (int i = 0; i < OTA_SIG_SIZE && i < slen; i++)
        m->signature[i] = sign_input[i] ^ priv_key[i % 32] ^ (uint8_t)(0xA5 + i * 3);
}

/* L5: Manifest verification */
bool edge_ota_manifest_verify(OtaUpdateCtx *ctx, const OtaManifest *m) {
    if (m->fw_version <= ctx->current_version) return false;
    if (m->fw_size == 0 || m->fw_size > OTA_MAX_PAYLOAD) return false;
    if (memcmp(m->device_id, ctx->device_id, OTA_DEV_ID_SIZE) != 0) return false;
    if (m->min_bootloader_ver > 1) return false;
    uint8_t hash_zero[OTA_FW_HASH_SIZE];
    memset(hash_zero, 0, OTA_FW_HASH_SIZE);
    if (memcmp(m->fw_hash, hash_zero, OTA_FW_HASH_SIZE) == 0) return false;
    return true;
}

/* L6: Begin OTA update - transition to downloading state */
bool edge_ota_begin_update(OtaUpdateCtx *ctx, const OtaManifest *m) {
    if (ctx->update_state != OTA_IDLE) return false;
    if (!edge_ota_manifest_verify(ctx, m)) return false;
    memcpy(&ctx->current_manifest, m, sizeof(OtaManifest));
    ctx->update_state = OTA_DOWNLOADING;
    ctx->bytes_received = 0;
    ctx->total_size = m->fw_size;
    ctx->manifest_verified = true;
    ctx->retry_count = 0;
    memset(ctx->receive_buffer, 0, OTA_MAX_PAYLOAD);
    return true;
}

/* L6: Receive firmware chunk */
bool edge_ota_receive_chunk(OtaUpdateCtx *ctx, const uint8_t *chunk, int len) {
    if (ctx->update_state != OTA_DOWNLOADING) return false;
    if (ctx->bytes_received + len > OTA_MAX_PAYLOAD) {
        ctx->update_state = OTA_FAILED;
        return false;
    }
    if (len <= 0 || len > OTA_MAX_CHUNK) return false;
    memcpy(ctx->receive_buffer + ctx->bytes_received, chunk, len);
    ctx->bytes_received += len;
    if (ctx->bytes_received >= ctx->total_size)
        ctx->update_state = OTA_VERIFYING;
    return true;
}

/* L5: Verify firmware integrity and apply to inactive bank */
bool edge_ota_verify_and_apply(OtaUpdateCtx *ctx) {
    if (ctx->update_state != OTA_VERIFYING) return false;
    if (ctx->bytes_received != ctx->total_size) {
        ctx->update_state = OTA_FAILED;
        return false;
    }
    uint32_t computed_hash = 0;
    for (uint32_t i = 0; i < ctx->bytes_received; i++)
        computed_hash = computed_hash * 31 + ctx->receive_buffer[i];
    uint32_t manifest_hash = 0;
    for (int i = 0; i < OTA_FW_HASH_SIZE; i++)
        manifest_hash = manifest_hash * 31 + ctx->current_manifest.fw_hash[i];
    if (computed_hash != manifest_hash) {
        ctx->update_state = OTA_FAILED;
        return false;
    }
    int target_bank = 1 - ctx->active_bank;
    ctx->banks[target_bank].version = ctx->current_manifest.fw_version;
    memcpy(ctx->banks[target_bank].fw_hash,
           ctx->current_manifest.fw_hash, OTA_FW_HASH_SIZE);
    ctx->banks[target_bank].valid = true;
    ctx->banks[target_bank].active = false;
    ctx->update_state = OTA_APPLYING;
    return true;
}

/* L6: Commit update - swap active bank */
bool edge_ota_commit_update(OtaUpdateCtx *ctx) {
    if (ctx->update_state != OTA_APPLYING) return false;
    int old_active = ctx->active_bank;
    int new_active = 1 - old_active;
    ctx->banks[old_active].active = false;
    ctx->banks[new_active].active = true;
    ctx->active_bank = new_active;
    ctx->current_version = ctx->banks[new_active].version;
    ctx->update_state = OTA_COMMITTED;
    return true;
}

/* L6: Rollback on failure */
bool edge_ota_rollback_update(OtaUpdateCtx *ctx) {
    if (ctx->update_state == OTA_IDLE || ctx->update_state == OTA_COMMITTED)
        return false;
    int target = 1 - ctx->active_bank;
    memset(&ctx->banks[target], 0, sizeof(FirmwareBank));
    ctx->update_state = OTA_IDLE;
    ctx->bytes_received = 0;
    ctx->manifest_verified = false;
    return true;
}

/* L4: Anti-rollback version check (NIST SP 800-193 ��3.2) */
bool edge_ota_check_version(OtaUpdateCtx *ctx, uint32_t version) {
    return version >= ctx->current_version;
}

/* L4: Golden image validation (NIST SP 800-193 ��4.1) */
bool edge_ota_validate_golden_image(OtaUpdateCtx *ctx) {
    for (int b = 0; b < OTA_DUAL_BANK; b++) {
        if (ctx->banks[b].valid && ctx->banks[b].version > 0)
            return true;
    }
    return false;
}

/* L4: Firmware integrity check */
bool edge_ota_check_firmware_integrity(OtaUpdateCtx *ctx, int bank_idx) {
    if (bank_idx < 0 || bank_idx >= OTA_DUAL_BANK) return false;
    if (!ctx->banks[bank_idx].valid) return false;
    uint8_t zero_hash[OTA_FW_HASH_SIZE];
    memset(zero_hash, 0, OTA_FW_HASH_SIZE);
    return memcmp(ctx->banks[bank_idx].fw_hash, zero_hash, OTA_FW_HASH_SIZE) != 0;
}

/* L4: Repair corrupted bank from golden copy */
void edge_ota_repair_corrupted_bank(OtaUpdateCtx *ctx, int src_bank, int dst_bank) {
    if (src_bank < 0 || src_bank >= OTA_DUAL_BANK) return;
    if (dst_bank < 0 || dst_bank >= OTA_DUAL_BANK) return;
    if (!ctx->banks[src_bank].valid) return;
    ctx->banks[dst_bank].version = ctx->banks[src_bank].version;
    memcpy(ctx->banks[dst_bank].fw_hash,
           ctx->banks[src_bank].fw_hash, OTA_FW_HASH_SIZE);
    ctx->banks[dst_bank].valid = true;
}

/* L7: AWS IoT Jobs document builder */
void edge_ota_build_job_document(OtaUpdateCtx *ctx, uint8_t *doc, int *doc_len) {
    int dl = 0;
    const char *pre = "{\"jobId\": \"ota-update-";
    int pl = (int)strlen(pre);
    memcpy(doc + dl, pre, pl); dl += pl;
    for (int i = 0; i < 8 && i < OTA_DEV_ID_SIZE; i++) {
        doc[dl++] = (uint8_t)((ctx->device_id[i] % 16) + 'A');
    }
    const char *post = "\", \"status\": \"IN_PROGRESS\"}";
    int pol = (int)strlen(post);
    memcpy(doc + dl, post, pol); dl += pol;
    *doc_len = dl;
}

/* L7: Parse AWS IoT Jobs response */
bool edge_ota_parse_job_response(OtaUpdateCtx *ctx, const uint8_t *response, int rlen) {
    if (rlen < 20) return false;
    const char *marker = "\"status\":\"QUEUED\"";
    int mlen = (int)strlen(marker);
    for (int i = 0; i <= rlen - mlen; i++) {
        if (memcmp(response + i, marker, mlen) == 0) return true;
    }
    (void)ctx;
    return false;
}

/* L9: Binary delta computation (simplified bsdiff-like approach)
 * Identifies changed byte ranges and encodes as (offset, len, new_data)
 * Format: [4B offset][2B len][len bytes new_data]... */
int edge_ota_compute_delta(const uint8_t *old_fw, int old_len,
                           const uint8_t *new_fw, int new_len,
                           uint8_t *delta, int max_delta) {
    int max_len = old_len > new_len ? old_len : new_len;
    int dpos = 0, run_start = -1, run_len = 0;
    for (int i = 0; i < max_len && dpos < max_delta - 10; i++) {
        uint8_t ob = i < old_len ? old_fw[i] : 0;
        uint8_t nb = i < new_len ? new_fw[i] : 0;
        if (ob != nb) {
            if (run_start < 0) { run_start = i; run_len = 1; }
            else { run_len++; }
        } else {
            if (run_start >= 0 && run_len > 0) {
                delta[dpos++] = (uint8_t)(run_start & 0xFF);
                delta[dpos++] = (uint8_t)((run_start >> 8) & 0xFF);
                delta[dpos++] = (uint8_t)((run_start >> 16) & 0xFF);
                delta[dpos++] = (uint8_t)((run_start >> 24) & 0xFF);
                delta[dpos++] = (uint8_t)(run_len & 0xFF);
                delta[dpos++] = (uint8_t)((run_len >> 8) & 0xFF);
                for (int j = 0; j < run_len && dpos < max_delta; j++)
                    delta[dpos++] = (run_start + j < new_len) ? new_fw[run_start + j] : 0;
                run_start = -1; run_len = 0;
            }
        }
    }
    if (run_start >= 0 && run_len > 0 && dpos < max_delta - 6) {
        delta[dpos++] = (uint8_t)(run_start & 0xFF);
        delta[dpos++] = (uint8_t)((run_start >> 8) & 0xFF);
        delta[dpos++] = (uint8_t)((run_start >> 16) & 0xFF);
        delta[dpos++] = (uint8_t)((run_start >> 24) & 0xFF);
        delta[dpos++] = (uint8_t)(run_len & 0xFF);
        delta[dpos++] = (uint8_t)((run_len >> 8) & 0xFF);
        for (int j = 0; j < run_len && dpos < max_delta; j++)
            delta[dpos++] = (run_start + j < new_len) ? new_fw[run_start + j] : 0;
    }
    return dpos;
}
