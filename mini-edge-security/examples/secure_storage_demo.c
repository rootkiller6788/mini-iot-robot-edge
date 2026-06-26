#include "secure_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("================================================================\n");
    printf("  Secure Storage Demo — Encrypted Flash + Secure Element\n");
    printf("================================================================\n\n");

    printf("[TEST 1] Initialize Secure Storage with device secret\n");
    SecureStorage ss;
    uint8_t device_secret[32];
    for (int i = 0; i < 32; i++) device_secret[i] = (uint8_t)(0xAB + i * 3);
    sec_storage_init(&ss, device_secret);
    printf("  Device secret : ");
    for (int i = 0; i < 8; i++) printf("%02X", device_secret[i]);
    printf("...\n");
    printf("  Locked        : %s\n\n", ss.locked ? "YES" : "NO");

    printf("[TEST 2] Key Derivation (HMAC-based KDF)\n");
    sec_storage_kdf_derive(&ss);
    printf("  KDF salt      : ");
    for (int i = 0; i < 8; i++) printf("%02X", ss.kdf.salt[i]);
    printf("...\n");
    printf("  Derived key   : ");
    for (int i = 0; i < 8; i++) printf("%02X", ss.kdf.derived_key[i]);
    printf("...\n");
    printf("  Algorithm     : AES_CTR_256\n\n");

    printf("[TEST 3] Configure Access Regions\n");
    sec_storage_add_region(&ss, 0x0000, 0x0800, SEC_ACCESS_READ | SEC_ACCESS_WRITE, true);
    sec_storage_add_region(&ss, 0x0800, 0x0400, SEC_ACCESS_READ, false);
    sec_storage_add_region(&ss, 0x0C00, 0x0400, SEC_ACCESS_WRITE | SEC_ACCESS_DELETE, true);
    printf("  Region 0      : [0x0000-0x0800] RW, encrypted\n");
    printf("  Region 1      : [0x0800-0x0C00] R, plain\n");
    printf("  Region 2      : [0x0C00-0x1000] W+DELETE, encrypted\n");
    printf("  Total regions : %d\n\n", ss.region_count);

    printf("[TEST 4] Access Control Check\n");
    printf("  Write @ 0x0100: %s\n",
           sec_storage_check_access(&ss, 0x0100, SEC_ACCESS_WRITE) ? "ALLOWED" : "DENIED");
    printf("  Read  @ 0x0100: %s\n",
           sec_storage_check_access(&ss, 0x0100, SEC_ACCESS_READ) ? "ALLOWED" : "DENIED");
    printf("  Write @ 0x0800: %s\n",
           sec_storage_check_access(&ss, 0x0800, SEC_ACCESS_WRITE) ? "ALLOWED" : "DENIED");
    printf("  Read  @ 0x0800: %s\n\n",
           sec_storage_check_access(&ss, 0x0800, SEC_ACCESS_READ) ? "ALLOWED" : "DENIED");

    printf("[TEST 5] Write & Read Encrypted Data\n");
    uint8_t plaintext[64];
    for (int i = 0; i < 64; i++) plaintext[i] = (uint8_t)(i + 0x30);
    printf("  Plaintext     : ");
    for (int i = 0; i < 16; i++) printf("%c", (char)plaintext[i]);
    printf("\n");
    sec_storage_write(&ss, 0x0100, plaintext, 64);
    uint8_t decrypted[64];
    sec_storage_read(&ss, 0x0100, decrypted, 64);
    bool match = (memcmp(plaintext, decrypted, 64) == 0);
    printf("  Decrypted     : ");
    for (int i = 0; i < 16; i++) printf("%c", (char)decrypted[i]);
    printf("\n");
    printf("  Round-trip OK : %s\n\n", match ? "TRUE" : "FALSE");

    printf("[TEST 6] CRC32 Integrity Check\n");
    uint32_t crc = sec_storage_crc32(plaintext, 64);
    printf("  CRC32 of data : 0x%08X\n", (unsigned)crc);
    printf("  CRC32 of empty: 0x%08X\n", (unsigned)sec_storage_crc32((const uint8_t*)"", 0));
    printf("  CRC32 of test : 0x%08X\n\n", (unsigned)sec_storage_crc32((const uint8_t*)"123456789", 9));

    printf("[TEST 7] Tamper Detection\n");
    bool tamper_ok = sec_storage_tamper_check(&ss);
    printf("  Tamper check  : %s\n", tamper_ok ? "PASSED" : "TAMPERED");
    printf("  Tamper flag   : %s\n\n", ss.tamper_detected ? "SET" : "CLEAR");

    printf("[TEST 8] File System on Encrypted Partition\n");
    sec_storage_format_fs(&ss);
    int fd1 = sec_storage_file_create(&ss, "wifi_psk");
    int fd2 = sec_storage_file_create(&ss, "dev_key");
    int fd3 = sec_storage_file_create(&ss, "azure_cert");
    printf("  Created files : %d\n", ss.file_count);
    printf("  File[%d] name  : %s\n", fd1, ss.files[fd1].name);
    printf("  File[%d] name  : %s\n", fd2, ss.files[fd2].name);
    printf("  File[%d] name  : %s\n", fd3, ss.files[fd3].name);

    uint8_t wifi_data[] = "MySecretWiFiPass123";
    sec_storage_file_write(&ss, fd1, wifi_data, (int)sizeof(wifi_data));
    uint8_t read_buf[64];
    int rd = sec_storage_file_read(&ss, fd1, read_buf, 64);
    printf("  Write wifi    : %s\n", wifi_data);
    printf("  Read  wifi    : %.*s\n", rd, read_buf);
    printf("  Read OK       : %s\n\n", memcmp(wifi_data, read_buf, rd) == 0 ? "TRUE" : "FALSE");

    printf("[TEST 9] Monotonic Counters\n");
    sec_storage_monotonic_inc(&ss, 0);
    sec_storage_monotonic_inc(&ss, 0);
    sec_storage_monotonic_inc(&ss, 0);
    printf("  Counter[0]    : %llu\n", (unsigned long long)sec_storage_monotonic_get(&ss, 0));
    sec_storage_monotonic_inc(&ss, 1);
    printf("  Counter[1]    : %llu\n\n", (unsigned long long)sec_storage_monotonic_get(&ss, 1));

    printf("[TEST 10] Secure Element (ATECC608 Simulated)\n");
    SecureElement se;
    sec_element_init(&se, SEC_EL_ATECC608);
    printf("  Type          : ATECC608\n");
    printf("  Serial        : ");
    for (int i = 0; i < 9; i++) printf("%02X", se.serial_num[i]);
    printf("\n");
    printf("  Pubkey        : ");
    for (int i = 0; i < 8; i++) printf("%02X", se.pubkey[i]);
    printf("...\n");

    uint8_t peer_pub[64], shared[32];
    for (int i = 0; i < 64; i++) peer_pub[i] = (uint8_t)(0xBB - i);
    sec_element_ecdh(&se, peer_pub, shared);
    printf("  ECDH shared   : ");
    for (int i = 0; i < 8; i++) printf("%02X", shared[i]);
    printf("...\n");

    uint8_t digest[32], sig[64]; int slen;
    for (int i = 0; i < 32; i++) digest[i] = (uint8_t)(i * 3 + 0x10);
    sec_element_sign(&se, digest, sig, &slen);
    printf("  Sign digest   : ");
    for (int i = 0; i < 8; i++) printf("%02X", digest[i]);
    printf("...\n");
    printf("  Signature len : %d\n", slen);

    uint8_t rng_out[16];
    sec_element_rng(&se, rng_out, 16);
    printf("  RNG output    : ");
    for (int i = 0; i < 8; i++) printf("%02X", rng_out[i]);
    printf("...\n");

    sec_element_store_key(&se, 0, device_secret, 4);
    printf("  Key slot 0    : 0x%08X\n\n",
           (unsigned)(se.data_zone[0] | (se.data_zone[1] << 8) |
                      (se.data_zone[2] << 16) | (se.data_zone[3] << 24)));

    printf("================================================================\n");
    printf("  SECURE STORAGE DEMO COMPLETE\n");
    printf("================================================================\n");
    return 0;
}
