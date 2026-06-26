#ifndef SECURE_STORAGE_H
#define SECURE_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

#define SEC_FLASH_PART_SIZE    8192
#define SEC_FLASH_SECTOR       64
#define SEC_FLASH_SECTOR_CNT   128
#define SEC_KDF_SALT_SIZE      16
#define SEC_KDF_DERIVED_SIZE   32
#define SEC_FS_BLOCK_SIZE      64
#define SEC_FS_MAX_FILES       32
#define SEC_FS_NAME_MAX        16
#define SEC_REGION_MAX         8
#define SEC_MONOTONIC_SIZE     8
#define SEC_CRC_POLY           0xEDB88320

typedef enum {
    SEC_ALG_PLAIN,
    SEC_ALG_AES_CTR_128,
    SEC_ALG_AES_CTR_256,
    SEC_ALG_CHACHA20
} SecEncryptAlg;

typedef enum {
    SEC_ACCESS_READ   = (1 << 0),
    SEC_ACCESS_WRITE  = (1 << 1),
    SEC_ACCESS_EXEC   = (1 << 2),
    SEC_ACCESS_DELETE = (1 << 3)
} SecAccess;

typedef enum {
    SEC_EL_ATECC608,
    SEC_EL_ATECC508,
    SEC_EL_STSAFE,
    SEC_EL_SIMULATED
} SecElementType;

typedef struct {
    uint32_t start_addr;
    uint32_t size;
    SecAccess permissions;
    bool encrypted;
    uint32_t monotonic_ctr;
} SecRegion;

typedef struct {
    uint8_t device_secret[32];
    uint8_t salt[SEC_KDF_SALT_SIZE];
    uint8_t derived_key[SEC_KDF_DERIVED_SIZE];
    SecEncryptAlg algorithm;
    bool kdf_initialized;
} SecKDF;

typedef struct {
    char name[SEC_FS_NAME_MAX];
    uint32_t block_start;
    uint32_t block_count;
    uint32_t file_size;
    uint32_t crc;
    bool encrypted;
    bool valid;
} SecFileEntry;

typedef struct {
    uint8_t flash[SEC_FLASH_PART_SIZE];
    SecRegion regions[SEC_REGION_MAX];
    int region_count;
    SecKDF kdf;
    SecFileEntry files[SEC_FS_MAX_FILES];
    int file_count;
    uint64_t monotonic_counters[8];
    bool tamper_detected;
    bool locked;
} SecureStorage;

typedef struct {
    SecElementType type;
    uint8_t serial_num[9];
    uint8_t pubkey[64];
    bool present;
    bool locked;
    uint8_t otp_zone[64];
    uint8_t data_zone[72];
} SecureElement;

void sec_storage_init(SecureStorage *ss, const uint8_t *device_secret);
void sec_storage_kdf_derive(SecureStorage *ss);
void sec_storage_add_region(SecureStorage *ss, uint32_t start, uint32_t size,
                             SecAccess perms, bool encrypted);
bool sec_storage_check_access(SecureStorage *ss, uint32_t addr, SecAccess required);
void sec_storage_write(SecureStorage *ss, uint32_t addr, const uint8_t *data, int len);
void sec_storage_read(SecureStorage *ss, uint32_t addr, uint8_t *data, int len);
void sec_storage_encrypt_region(SecureStorage *ss, int region_idx);
void sec_storage_decrypt_region(SecureStorage *ss, int region_idx);
void sec_storage_format_fs(SecureStorage *ss);
int sec_storage_file_create(SecureStorage *ss, const char *name);
void sec_storage_file_write(SecureStorage *ss, int file_id, const uint8_t *data, int len);
int sec_storage_file_read(SecureStorage *ss, int file_id, uint8_t *data, int max_len);
void sec_storage_file_delete(SecureStorage *ss, int file_id);
uint32_t sec_storage_crc32(const uint8_t *data, int len);
bool sec_storage_tamper_check(SecureStorage *ss);
void sec_storage_monotonic_inc(SecureStorage *ss, int ctr_idx);
uint64_t sec_storage_monotonic_get(SecureStorage *ss, int ctr_idx);
void sec_element_init(SecureElement *se, SecElementType type);
void sec_element_ecdh(SecureElement *se, const uint8_t *peer_pubkey,
                       uint8_t shared_secret[32]);
void sec_element_sign(SecureElement *se, const uint8_t *digest, uint8_t *sig, int *slen);
void sec_element_rng(SecureElement *se, uint8_t *random_bytes, int len);
void sec_element_store_key(SecureElement *se, int slot, const uint8_t *key, int len);

#endif
