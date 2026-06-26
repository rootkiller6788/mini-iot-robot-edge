#ifndef EDGE_KEY_MGMT_H
#define EDGE_KEY_MGMT_H

#include <stdbool.h>
#include <stdint.h>

#define KM_MAX_KEY_SLOTS     16
#define KM_MAX_CERT_SIZE      1024
#define KM_MAX_CHAIN_DEPTH    5
#define KM_PUBKEY_SIZE        64
#define KM_PRIVKEY_SIZE       32
#define KM_SERIAL_SIZE        16
#define KM_SUBJECT_SIZE       64
#define KM_ISSUER_SIZE        64

/* L1: Key types */
typedef enum {
    KEY_TYPE_ECC_P256,
    KEY_TYPE_ECC_P384,
    KEY_TYPE_RSA_2048,
    KEY_TYPE_RSA_4096,
    KEY_TYPE_ED25519
} KeyMgmtKeyType;

/* L1: Key usage flags */
typedef enum {
    KEY_USE_SIGN     = (1 << 0),
    KEY_USE_VERIFY   = (1 << 1),
    KEY_USE_ENCRYPT  = (1 << 2),
    KEY_USE_DECRYPT  = (1 << 3),
    KEY_USE_DERIVE   = (1 << 4),
    KEY_USE_WRAP     = (1 << 5),
    KEY_USE_UNWRAP   = (1 << 6)
} KeyMgmtKeyUsage;

/* L1: Key slot in secure key store */
typedef struct {
    int slot_id;
    KeyMgmtKeyType key_type;
    uint8_t private_key[KM_PRIVKEY_SIZE];
    uint8_t public_key[KM_PUBKEY_SIZE];
    int priv_len;
    int pub_len;
    uint32_t usage_flags;
    bool active;
    bool exportable;
    uint32_t created_at;
    uint32_t expires_at;
} KeyMgmtSlot;

/* L1: X.509 certificate (simplified, RFC 5280) */
typedef struct {
    uint8_t cert_data[KM_MAX_CERT_SIZE];
    int cert_len;
    uint8_t subject[KM_SUBJECT_SIZE];
    uint8_t issuer[KM_ISSUER_SIZE];
    uint8_t serial[KM_SERIAL_SIZE];
    uint8_t pubkey[KM_PUBKEY_SIZE];
    int pubkey_len;
    uint32_t not_before;
    uint32_t not_after;
    bool is_ca;
    bool revoked;
} X509CertChain;

/* L1: PKI context for edge device key management */
typedef struct {
    KeyMgmtSlot slots[KM_MAX_KEY_SLOTS];
    int key_count;
    X509CertChain cert_chain[KM_MAX_CHAIN_DEPTH];
    int chain_depth;
    uint8_t trust_anchor_pubkey[KM_PUBKEY_SIZE];
    int trust_anchor_pubkey_len;
    bool pki_initialized;
} EdgeKeyMgmtCtx;

/* L5: Key lifecycle management */
void edge_key_mgmt_init(EdgeKeyMgmtCtx *km);
int  edge_key_mgmt_generate_keypair(EdgeKeyMgmtCtx *km, KeyMgmtKeyType ktype);
int  edge_key_mgmt_import_keypair(EdgeKeyMgmtCtx *km,
                                   const uint8_t *priv, int priv_len,
                                   const uint8_t *pub, int pub_len);
bool edge_key_mgmt_get_public_key(EdgeKeyMgmtCtx *km, int slot,
                                  uint8_t *pubkey, int max_len);
void edge_key_mgmt_rotate_key(EdgeKeyMgmtCtx *km, int slot);
bool edge_key_mgmt_revoke_key(EdgeKeyMgmtCtx *km, int slot);
bool edge_key_mgmt_sign(EdgeKeyMgmtCtx *km, int slot,
                         const uint8_t *hash, int hash_len,
                         uint8_t *sig, int *sig_len);
bool edge_key_mgmt_verify(EdgeKeyMgmtCtx *km, int slot,
                           const uint8_t *hash, int hash_len,
                           const uint8_t *sig, int sig_len);

/* L4: X.509 certificate chain validation (RFC 5280) */
bool edge_key_mgmt_validate_cert_chain(EdgeKeyMgmtCtx *km, X509CertChain *chain);
bool edge_key_mgmt_check_cert_expiry(const X509CertChain *cert, uint32_t now);
bool edge_key_mgmt_verify_cert_signature(EdgeKeyMgmtCtx *km,
                                          const X509CertChain *cert,
                                          const uint8_t *issuer_pubkey);

/* L3: Key hierarchy: Trust Anchor -> Sub CA -> Device */
void edge_key_mgmt_set_trust_anchor(EdgeKeyMgmtCtx *km,
                                    const uint8_t *pubkey, int pubkey_len);
bool edge_key_mgmt_issue_device_cert(EdgeKeyMgmtCtx *km, int ca_slot,
                                     int device_slot, X509CertChain *out_cert);

/* L7: PKI operational tasks */
bool edge_key_mgmt_check_crl(EdgeKeyMgmtCtx *km, const uint8_t *crl, int crl_len);
void edge_key_mgmt_export_csr(EdgeKeyMgmtCtx *km, int slot,
                               uint8_t *csr, int *csr_len);
int  edge_key_mgmt_find_key_by_usage(EdgeKeyMgmtCtx *km, uint32_t usage);

/* L8: Key wrapping (RFC 3394 AES Key Wrap) */
void edge_key_mgmt_wrap_key(EdgeKeyMgmtCtx *km, int wrapping_key_slot,
                            int target_slot, uint8_t *wrapped, int *wlen);
bool edge_key_mgmt_unwrap_key(EdgeKeyMgmtCtx *km, int wrapping_key_slot,
                              const uint8_t *wrapped, int wlen, int target_slot);

/* L9: Post-quantum readiness note */
bool edge_key_mgmt_is_pq_ready(EdgeKeyMgmtCtx *km);
void edge_key_mgmt_set_pq_label(EdgeKeyMgmtCtx *km, bool ready);

#endif
