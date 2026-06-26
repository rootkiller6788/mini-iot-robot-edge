#include "secure_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t crc32_table[256];
static bool crc_table_init = false;

static void crc32_init_table(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++)
            crc = (crc & 1) ? (SEC_CRC_POLY ^ (crc >> 1)) : (crc >> 1);
        crc32_table[i] = crc;
    }
    crc_table_init = true;
}

uint32_t sec_storage_crc32(const uint8_t *data, int len) {
    if (!crc_table_init) crc32_init_table();
    uint32_t crc = 0xFFFFFFFF;
    for (int i = 0; i < len; i++)
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFF;
}

static void sec_xor_crypt(const uint8_t *key, const uint8_t *in, uint8_t *out, int len) {
    for (int i = 0; i < len; i++)
        out[i] = in[i] ^ key[i % SEC_KDF_DERIVED_SIZE];
}

static void sec_kdf_hmac(const uint8_t *key, int klen, const uint8_t *msg,
                          int mlen, uint8_t derived[SEC_KDF_DERIVED_SIZE]) {
    uint8_t ipad[64], opad[64];
    memset(ipad, 0x36, 64);
    memset(opad, 0x5C, 64);
    for (int i = 0; i < klen && i < 64; i++) {
        ipad[i] ^= key[i];
        opad[i] ^= key[i];
    }
    uint8_t inner_hash[SEC_KDF_DERIVED_SIZE];
    for (int i = 0; i < SEC_KDF_DERIVED_SIZE; i++)
        inner_hash[i] = (uint8_t)((ipad[i % 64] ^ msg[i % mlen]) * 0x6D);
    for (int i = 0; i < SEC_KDF_DERIVED_SIZE; i++)
        derived[i] = (uint8_t)((opad[i % 64] ^ inner_hash[i % SEC_KDF_DERIVED_SIZE]) * 0x5B);
}

void sec_storage_init(SecureStorage *ss, const uint8_t *device_secret) {
    memset(ss, 0, sizeof(SecureStorage));
    memcpy(ss->kdf.device_secret, device_secret, 32);
    ss->locked = true;
    ss->tamper_detected = false;
}

void sec_storage_kdf_derive(SecureStorage *ss) {
    if (ss->kdf.kdf_initialized) return;
    for (int i = 0; i < SEC_KDF_SALT_SIZE; i++)
        ss->kdf.salt[i] = (uint8_t)(0x7A + i * 11 + ss->kdf.device_secret[i % 32]);
    sec_kdf_hmac(ss->kdf.device_secret, 32, ss->kdf.salt, SEC_KDF_SALT_SIZE,
                  ss->kdf.derived_key);
    ss->kdf.algorithm = SEC_ALG_AES_CTR_256;
    ss->kdf.kdf_initialized = true;
}

void sec_storage_add_region(SecureStorage *ss, uint32_t start, uint32_t size,
                             SecAccess perms, bool encrypted) {
    if (ss->region_count >= SEC_REGION_MAX) return;
    ss->regions[ss->region_count].start_addr = start;
    ss->regions[ss->region_count].size = size;
    ss->regions[ss->region_count].permissions = perms;
    ss->regions[ss->region_count].encrypted = encrypted;
    ss->regions[ss->region_count].monotonic_ctr = 0;
    ss->region_count++;
}

bool sec_storage_check_access(SecureStorage *ss, uint32_t addr, SecAccess required) {
    for (int i = 0; i < ss->region_count; i++) {
        if (addr >= ss->regions[i].start_addr &&
            addr < ss->regions[i].start_addr + ss->regions[i].size) {
            return (ss->regions[i].permissions & required) == required;
        }
    }
    return false;
}

void sec_storage_write(SecureStorage *ss, uint32_t addr, const uint8_t *data, int len) {
    if (!sec_storage_check_access(ss, addr, SEC_ACCESS_WRITE)) return;
    if (addr + len > SEC_FLASH_PART_SIZE) len = SEC_FLASH_PART_SIZE - (int)addr;
    if (len <= 0) return;
    if (ss->kdf.kdf_initialized) {
        uint8_t encrypted[SEC_FLASH_PART_SIZE];
        sec_xor_crypt(ss->kdf.derived_key, data, encrypted, len);
        memcpy(ss->flash + addr, encrypted, len);
        uint32_t crc = sec_storage_crc32(encrypted, len);
        int crc_addr = (int)(SEC_FLASH_PART_SIZE - 4);
        ss->flash[crc_addr]   = (uint8_t)(crc & 0xFF);
        ss->flash[crc_addr+1] = (uint8_t)((crc >> 8) & 0xFF);
        ss->flash[crc_addr+2] = (uint8_t)((crc >> 16) & 0xFF);
        ss->flash[crc_addr+3] = (uint8_t)((crc >> 24) & 0xFF);
    } else {
        memcpy(ss->flash + addr, data, len);
    }
}

void sec_storage_read(SecureStorage *ss, uint32_t addr, uint8_t *data, int len) {
    if (!sec_storage_check_access(ss, addr, SEC_ACCESS_READ)) return;
    if (addr + len > SEC_FLASH_PART_SIZE) len = SEC_FLASH_PART_SIZE - (int)addr;
    if (len <= 0) return;
    if (ss->kdf.kdf_initialized) {
        sec_xor_crypt(ss->kdf.derived_key, ss->flash + addr, data, len);
    } else {
        memcpy(data, ss->flash + addr, len);
    }
}

void sec_storage_encrypt_region(SecureStorage *ss, int region_idx) {
    if (region_idx < 0 || region_idx >= ss->region_count) return;
    if (!ss->kdf.kdf_initialized) sec_storage_kdf_derive(ss);
    uint32_t start = ss->regions[region_idx].start_addr;
    uint32_t size = ss->regions[region_idx].size;
    if (start + size > SEC_FLASH_PART_SIZE) size = SEC_FLASH_PART_SIZE - start;
    uint8_t plain[SEC_FS_BLOCK_SIZE * 8];
    memcpy(plain, ss->flash + start, size < (int)sizeof(plain) ? size : (int)sizeof(plain));
    sec_xor_crypt(ss->kdf.derived_key, plain, ss->flash + start,
                   size < (int)sizeof(plain) ? size : (int)sizeof(plain));
    ss->regions[region_idx].encrypted = true;
}

void sec_storage_decrypt_region(SecureStorage *ss, int region_idx) {
    if (region_idx < 0 || region_idx >= ss->region_count) return;
    if (!ss->regions[region_idx].encrypted) return;
    if (!ss->kdf.kdf_initialized) sec_storage_kdf_derive(ss);
    uint32_t start = ss->regions[region_idx].start_addr;
    uint32_t size = ss->regions[region_idx].size;
    if (start + size > SEC_FLASH_PART_SIZE) size = SEC_FLASH_PART_SIZE - start;
    sec_xor_crypt(ss->kdf.derived_key, ss->flash + start, ss->flash + start, size);
    ss->regions[region_idx].encrypted = false;
}

void sec_storage_format_fs(SecureStorage *ss) {
    memset(ss->files, 0, sizeof(ss->files));
    ss->file_count = 0;
    for (int i = 0; i < SEC_FS_BLOCK_SIZE * 2; i++)
        ss->flash[i] = 0;
}

int sec_storage_file_create(SecureStorage *ss, const char *name) {
    if (ss->file_count >= SEC_FS_MAX_FILES) return -1;
    int nl = (int)strlen(name);
    if (nl >= SEC_FS_NAME_MAX) nl = SEC_FS_NAME_MAX - 1;
    memcpy(ss->files[ss->file_count].name, name, nl);
    ss->files[ss->file_count].name[nl] = '\0';
    ss->files[ss->file_count].block_start = (uint32_t)((ss->file_count + 1) * 8);
    ss->files[ss->file_count].block_count = 1;
    ss->files[ss->file_count].file_size = 0;
    ss->files[ss->file_count].valid = true;
    ss->files[ss->file_count].encrypted = ss->kdf.kdf_initialized;
    ss->file_count++;
    return ss->file_count - 1;
}

void sec_storage_file_write(SecureStorage *ss, int file_id, const uint8_t *data, int len) {
    if (file_id < 0 || file_id >= ss->file_count) return;
    uint32_t offset = ss->files[file_id].block_start * SEC_FS_BLOCK_SIZE;
    uint32_t max_write = (ss->files[file_id].block_count * SEC_FS_BLOCK_SIZE);
    int wlen = len < (int)max_write ? len : (int)max_write;
    if (offset + wlen > SEC_FLASH_PART_SIZE) wlen = SEC_FLASH_PART_SIZE - (int)offset;
    if (ss->files[file_id].encrypted && ss->kdf.kdf_initialized) {
        uint8_t enc[SEC_FS_BLOCK_SIZE * 4];
        sec_xor_crypt(ss->kdf.derived_key, data, enc, wlen);
        memcpy(ss->flash + offset, enc, wlen);
    } else {
        memcpy(ss->flash + offset, data, wlen);
    }
    ss->files[file_id].file_size = (uint32_t)wlen;
    ss->files[file_id].crc = sec_storage_crc32(ss->flash + offset, wlen);
}

int sec_storage_file_read(SecureStorage *ss, int file_id, uint8_t *data, int max_len) {
    if (file_id < 0 || file_id >= ss->file_count) return 0;
    uint32_t offset = ss->files[file_id].block_start * SEC_FS_BLOCK_SIZE;
    int rlen = (int)ss->files[file_id].file_size < max_len ?
               (int)ss->files[file_id].file_size : max_len;
    if (ss->files[file_id].encrypted && ss->kdf.kdf_initialized) {
        sec_xor_crypt(ss->kdf.derived_key, ss->flash + offset, data, rlen);
    } else {
        memcpy(data, ss->flash + offset, rlen);
    }
    return rlen;
}

void sec_storage_file_delete(SecureStorage *ss, int file_id) {
    if (file_id < 0 || file_id >= ss->file_count) return;
    uint32_t offset = ss->files[file_id].block_start * SEC_FS_BLOCK_SIZE;
    memset(ss->flash + offset, 0, ss->files[file_id].block_count * SEC_FS_BLOCK_SIZE);
    memset(&ss->files[file_id], 0, sizeof(SecFileEntry));
    ss->files[file_id].valid = false;
}

bool sec_storage_tamper_check(SecureStorage *ss) {
    int crc_addr = (int)(SEC_FLASH_PART_SIZE - 4);
    uint32_t stored_crc = (uint32_t)ss->flash[crc_addr] |
                          ((uint32_t)ss->flash[crc_addr+1] << 8) |
                          ((uint32_t)ss->flash[crc_addr+2] << 16) |
                          ((uint32_t)ss->flash[crc_addr+3] << 24);
    uint32_t computed = sec_storage_crc32(ss->flash, SEC_FLASH_PART_SIZE - 4);
    if (stored_crc != computed && stored_crc != 0) {
        ss->tamper_detected = true;
        return false;
    }
    return true;
}

void sec_storage_monotonic_inc(SecureStorage *ss, int ctr_idx) {
    if (ctr_idx < 0 || ctr_idx >= 8) return;
    ss->monotonic_counters[ctr_idx]++;
}

uint64_t sec_storage_monotonic_get(SecureStorage *ss, int ctr_idx) {
    if (ctr_idx < 0 || ctr_idx >= 8) return 0;
    return ss->monotonic_counters[ctr_idx];
}

void sec_element_init(SecureElement *se, SecElementType type) {
    memset(se, 0, sizeof(SecureElement));
    se->type = type;
    se->present = true;
    se->locked = true;
    for (int i = 0; i < 9; i++)
        se->serial_num[i] = (uint8_t)(0x01 + i * 0x23);
    for (int i = 0; i < 64; i++)
        se->pubkey[i] = (uint8_t)(0x40 + i * 3);
    for (int i = 0; i < 64; i++)
        se->otp_zone[i] = 0xFF;
}

void sec_element_ecdh(SecureElement *se, const uint8_t *peer_pubkey,
                       uint8_t shared_secret[32]) {
    for (int i = 0; i < 32; i++)
        shared_secret[i] = (uint8_t)(se->pubkey[i] ^ peer_pubkey[i % 64] ^
                                      se->serial_num[i % 9]);
}

void sec_element_sign(SecureElement *se, const uint8_t *digest, uint8_t *sig, int *slen) {
    *slen = 64;
    for (int i = 0; i < 64; i++)
        sig[i] = (uint8_t)(digest[i % 32] ^ se->otp_zone[i % 64] ^
                           (uint8_t)(0xE0 + i * 2));
}

void sec_element_rng(SecureElement *se, uint8_t *random_bytes, int len) {
    uint32_t seed = (uint32_t)(se->serial_num[0] | (se->serial_num[1] << 8) |
                               (se->serial_num[2] << 16) | (se->serial_num[3] << 24));
    for (int i = 0; i < len; i++) {
        seed = seed * 1103515245 + 12345;
        random_bytes[i] = (uint8_t)((seed >> 16) & 0xFF);
    }
}

void sec_element_store_key(SecureElement *se, int slot, const uint8_t *key, int len) {
    if (slot < 0 || slot >= 16) return;
    int offset = slot * 4;
    int cplen = len < 4 ? len : 4;
    memcpy(se->data_zone + offset, key, cplen);
}
