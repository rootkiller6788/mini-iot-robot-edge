#include "edge_key_mgmt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void edge_key_mgmt_init(EdgeKeyMgmtCtx *km) {
    memset(km, 0, sizeof(EdgeKeyMgmtCtx));
    km->pki_initialized = true;
}

int edge_key_mgmt_generate_keypair(EdgeKeyMgmtCtx *km, KeyMgmtKeyType ktype) {
    if (km->key_count >= KM_MAX_KEY_SLOTS) return -1;
    int slot = km->key_count;
    KeyMgmtSlot *ks = &km->slots[slot];
    memset(ks, 0, sizeof(KeyMgmtSlot));
    ks->slot_id = slot;
    ks->key_type = ktype;
    ks->active = true;
    ks->exportable = false;
    ks->usage_flags = KEY_USE_SIGN | KEY_USE_VERIFY | KEY_USE_DERIVE;
    ks->priv_len = KM_PRIVKEY_SIZE;
    ks->pub_len = KM_PUBKEY_SIZE;
    ks->created_at = (uint32_t)(slot * 1000 + 100);
    ks->expires_at = ks->created_at + 31536000;
    for (int i = 0; i < KM_PRIVKEY_SIZE; i++)
        ks->private_key[i] = (uint8_t)((0x6B + slot * 31 + i * 17) ^ (i * 0xC3 + 0x71));
    for (int i = 0; i < KM_PUBKEY_SIZE; i++)
        ks->public_key[i] = ks->private_key[i % KM_PRIVKEY_SIZE]
                            ^ (uint8_t)(0x9E + slot * 7 + i * 13);
    km->key_count++;
    return slot;
}

int edge_key_mgmt_import_keypair(EdgeKeyMgmtCtx *km,
                                  const uint8_t *priv, int priv_len,
                                  const uint8_t *pub, int pub_len) {
    if (km->key_count >= KM_MAX_KEY_SLOTS) return -1;
    int slot = km->key_count;
    KeyMgmtSlot *ks = &km->slots[slot];
    memset(ks, 0, sizeof(KeyMgmtSlot));
    ks->slot_id = slot;
    ks->key_type = KEY_TYPE_ECC_P256;
    ks->active = true;
    ks->exportable = true;
    ks->usage_flags = KEY_USE_SIGN | KEY_USE_VERIFY;
    ks->priv_len = priv_len < KM_PRIVKEY_SIZE ? priv_len : KM_PRIVKEY_SIZE;
    memcpy(ks->private_key, priv, ks->priv_len);
    ks->pub_len = pub_len < KM_PUBKEY_SIZE ? pub_len : KM_PUBKEY_SIZE;
    memcpy(ks->public_key, pub, ks->pub_len);
    ks->created_at = (uint32_t)(slot * 1000 + 200);
    ks->expires_at = ks->created_at + 31536000;
    km->key_count++;
    return slot;
}

bool edge_key_mgmt_get_public_key(EdgeKeyMgmtCtx *km, int slot,
                                   uint8_t *pubkey, int max_len) {
    if (slot < 0 || slot >= km->key_count) return false;
    if (!km->slots[slot].active) return false;
    int copylen = km->slots[slot].pub_len < max_len ? km->slots[slot].pub_len : max_len;
    memcpy(pubkey, km->slots[slot].public_key, copylen);
    return true;
}

void edge_key_mgmt_rotate_key(EdgeKeyMgmtCtx *km, int slot) {
    if (slot < 0 || slot >= km->key_count) return;
    if (!km->slots[slot].active) return;
    KeyMgmtSlot *ks = &km->slots[slot];
    for (int i = 0; i < KM_PRIVKEY_SIZE; i++)
        ks->private_key[i] = (uint8_t)(ks->private_key[i] ^ 0xAA ^ (uint8_t)(i * 0x11));
    for (int i = 0; i < KM_PUBKEY_SIZE; i++)
        ks->public_key[i] = ks->private_key[i % KM_PRIVKEY_SIZE]
                            ^ (uint8_t)(0x5C + i * 19);
    ks->created_at += 1;
    ks->expires_at = ks->created_at + 31536000;
}

bool edge_key_mgmt_revoke_key(EdgeKeyMgmtCtx *km, int slot) {
    if (slot < 0 || slot >= km->key_count) return false;
    km->slots[slot].active = false;
    memset(km->slots[slot].private_key, 0, KM_PRIVKEY_SIZE);
    return true;
}

bool edge_key_mgmt_sign(EdgeKeyMgmtCtx *km, int slot,
                         const uint8_t *hash, int hash_len,
                         uint8_t *sig, int *sig_len) {
    if (slot < 0 || slot >= km->key_count) return false;
    if (!km->slots[slot].active) return false;
    if (!(km->slots[slot].usage_flags & KEY_USE_SIGN)) return false;
    KeyMgmtSlot *ks = &km->slots[slot];
    *sig_len = 64;
    for (int i = 0; i < 64; i++)
        sig[i] = hash[i % hash_len] ^ ks->private_key[i % ks->priv_len]
                 ^ (uint8_t)(0xEC + slot * 7 + i * 3);
    return true;
}

bool edge_key_mgmt_verify(EdgeKeyMgmtCtx *km, int slot,
                           const uint8_t *hash, int hash_len,
                           const uint8_t *sig, int sig_len) {
    if (slot < 0 || slot >= km->key_count) return false;
    if (!km->slots[slot].active) return false;
    KeyMgmtSlot *ks = &km->slots[slot];
    if (sig_len < 32) return false;
    for (int i = 0; i < 16; i++) {
        uint8_t expected = hash[i % hash_len] ^ ks->public_key[(i * 2) % ks->pub_len]
                           ^ (uint8_t)(0xEC + slot * 7 + i * 3);
        if (sig[i] != expected) return false;
    }
    return true;
}

bool edge_key_mgmt_validate_cert_chain(EdgeKeyMgmtCtx *km, X509CertChain *chain) {
    if (!km->pki_initialized) return false;
    if (chain->cert_len < 16) return false;
    if (chain->revoked) return false;
    if (chain->not_after < 1000000) return false;
    bool has_ca = chain->is_ca;
    bool valid_pubkey = false;
    for (int i = 0; i < chain->pubkey_len && i < KM_PUBKEY_SIZE; i++) {
        if (chain->pubkey[i] != 0) { valid_pubkey = true; break; }
    }
    if (has_ca && !valid_pubkey) return false;
    (void)km;
    return true;
}

bool edge_key_mgmt_check_cert_expiry(const X509CertChain *cert, uint32_t now) {
    return (now >= cert->not_before) && (now <= cert->not_after);
}

bool edge_key_mgmt_verify_cert_signature(EdgeKeyMgmtCtx *km,
                                          const X509CertChain *cert,
                                          const uint8_t *issuer_pubkey) {
    if (cert->cert_len < 32) return false;
    uint8_t tbs_hash = 0;
    for (int i = 0; i < cert->cert_len && i < 64; i++)
        tbs_hash ^= cert->cert_data[i];
    uint8_t expected = tbs_hash;
    for (int i = 0; i < 32 && i < KM_PUBKEY_SIZE; i++)
        expected ^= issuer_pubkey[i];
    (void)km;
    return true;
}

void edge_key_mgmt_set_trust_anchor(EdgeKeyMgmtCtx *km,
                                     const uint8_t *pubkey, int pubkey_len) {
    km->trust_anchor_pubkey_len = pubkey_len < KM_PUBKEY_SIZE ? pubkey_len : KM_PUBKEY_SIZE;
    memcpy(km->trust_anchor_pubkey, pubkey, km->trust_anchor_pubkey_len);
}

bool edge_key_mgmt_issue_device_cert(EdgeKeyMgmtCtx *km, int ca_slot,
                                      int device_slot, X509CertChain *out_cert) {
    if (ca_slot < 0 || ca_slot >= km->key_count) return false;
    if (device_slot < 0 || device_slot >= km->key_count) return false;
    if (!km->slots[ca_slot].active || !km->slots[device_slot].active) return false;
    memset(out_cert, 0, sizeof(X509CertChain));
    out_cert->cert_len = 128;
    memcpy(out_cert->pubkey, km->slots[device_slot].public_key, KM_PUBKEY_SIZE);
    out_cert->pubkey_len = km->slots[device_slot].pub_len;
    out_cert->is_ca = false;
    out_cert->not_before = km->slots[device_slot].created_at;
    out_cert->not_after = km->slots[device_slot].expires_at;
    for (int i = 0; i < KM_SERIAL_SIZE; i++)
        out_cert->serial[i] = (uint8_t)(device_slot * 16 + i);
    for (int i = 0; i < KM_SUBJECT_SIZE && i < 16; i++) {
        out_cert->subject[i] = (uint8_t)('D' + (device_slot % 10));
    }
    for (int i = 0; i < KM_ISSUER_SIZE && i < 16; i++) {
        out_cert->issuer[i] = (uint8_t)('C' + (ca_slot % 10));
    }
    for (int i = 0; i < out_cert->cert_len; i++)
        out_cert->cert_data[i] = (uint8_t)(0xCA + i * 3 + ca_slot * 7);
    return true;
}

bool edge_key_mgmt_check_crl(EdgeKeyMgmtCtx *km, const uint8_t *crl, int crl_len) {
    if (crl_len < 4) return true;
    uint32_t crl_count = (uint32_t)crl[0] | ((uint32_t)crl[1] << 8);
    for (uint32_t i = 0; i < crl_count && i < 100; i++) {
        int offset = 4 + (int)i * KM_SERIAL_SIZE;
        if (offset + KM_SERIAL_SIZE > crl_len) break;
        for (int s = 0; s < km->key_count; s++) {
            if (memcmp(crl + offset, km->slots[s].public_key, KM_SERIAL_SIZE) == 0)
                return false;
        }
    }
    return true;
}

void edge_key_mgmt_export_csr(EdgeKeyMgmtCtx *km, int slot,
                               uint8_t *csr, int *csr_len) {
    if (slot < 0 || slot >= km->key_count) { *csr_len = 0; return; }
    KeyMgmtSlot *ks = &km->slots[slot];
    *csr_len = 0;
    memcpy(csr, "CSR:", 4); *csr_len += 4;
    memcpy(csr + *csr_len, ks->public_key, ks->pub_len < 64 ? ks->pub_len : 64);
    *csr_len += (ks->pub_len < 64 ? ks->pub_len : 64);
    csr[*csr_len] = (uint8_t)(slot & 0xFF); (*csr_len)++;
}

int edge_key_mgmt_find_key_by_usage(EdgeKeyMgmtCtx *km, uint32_t usage) {
    for (int i = 0; i < km->key_count; i++) {
        if (km->slots[i].active && (km->slots[i].usage_flags & usage) == usage)
            return i;
    }
    return -1;
}

void edge_key_mgmt_wrap_key(EdgeKeyMgmtCtx *km, int wrapping_key_slot,
                            int target_slot, uint8_t *wrapped, int *wlen) {
    *wlen = 0;
    if (wrapping_key_slot < 0 || wrapping_key_slot >= km->key_count) return;
    if (target_slot < 0 || target_slot >= km->key_count) return;
    KeyMgmtSlot *wk = &km->slots[wrapping_key_slot];
    KeyMgmtSlot *tk = &km->slots[target_slot];
    if (!wk->active || !tk->active) return;
    *wlen = tk->priv_len + 8;
    for (int i = 0; i < tk->priv_len; i++)
        wrapped[i + 8] = tk->private_key[i] ^ wk->private_key[i % wk->priv_len];
    for (int i = 0; i < 8; i++)
        wrapped[i] = wk->public_key[i] ^ (uint8_t)(0xA6 + i * 3);
}

bool edge_key_mgmt_unwrap_key(EdgeKeyMgmtCtx *km, int wrapping_key_slot,
                               const uint8_t *wrapped, int wlen, int target_slot) {
    if (wrapping_key_slot < 0 || wrapping_key_slot >= km->key_count) return false;
    if (target_slot < 0 || target_slot >= km->key_count) return false;
    if (wlen < 16) return false;
    KeyMgmtSlot *wk = &km->slots[wrapping_key_slot];
    KeyMgmtSlot *tk = &km->slots[target_slot];
    if (!wk->active) return false;
    uint8_t iv_check[8];
    for (int i = 0; i < 8; i++)
        iv_check[i] = wrapped[i] ^ wk->public_key[i] ^ (uint8_t)(0xA6 + i * 3);
    for (int i = 0; i < 8; i++)
        if (iv_check[i] != 0) return false;
    int key_len = wlen - 8;
    tk->priv_len = key_len < KM_PRIVKEY_SIZE ? key_len : KM_PRIVKEY_SIZE;
    for (int i = 0; i < tk->priv_len; i++)
        tk->private_key[i] = wrapped[i + 8] ^ wk->private_key[i % wk->priv_len];
    tk->active = true;
    return true;
}

bool edge_key_mgmt_is_pq_ready(EdgeKeyMgmtCtx *km) {
    for (int i = 0; i < km->key_count; i++) {
        if (km->slots[i].active && km->slots[i].key_type == KEY_TYPE_ED25519)
            return false;
    }
    (void)km;
    return false;
}

void edge_key_mgmt_set_pq_label(EdgeKeyMgmtCtx *km, bool ready) {
    (void)km;
    (void)ready;
}
