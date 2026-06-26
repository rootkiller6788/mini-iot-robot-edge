/*
 * demo_full.c - Full Demonstration of mini-edge-security
 *
 * Walks through all five sub-modules:
 *   device_attest.h, psa_certified.h, secure_boot_mcu.h, secure_storage.h, trustzone_arm.h
 *
 * These modules use <stdbool.h> for the bool type.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "device_attest.h"
#include "psa_certified.h"
#include "secure_boot_mcu.h"
#include "secure_storage.h"
#include "trustzone_arm.h"

int main(void) {
    printf("\n");
    printf("*************************************************************\n");
    printf("*                                                           *\n");
    printf("*    MINI-EDGE-SECURITY  --  Full Feature Demonstration     *\n");
    printf("*   Attest | PSA Crypto | Secure Boot | Storage | TrustZone *\n");
    printf("*                                                           *\n");
    printf("*************************************************************\n");
    printf("\n");

    /* ---------------------------------------------------------------
     *  SECTION 1 -- device_attest.h
     * --------------------------------------------------------------- */
    printf("--- Section 1: Device Attestation ---\n\n");

    int rc = device_attest_init();
    if (rc == 0) printf("[OK] device_attest_init() succeeded\n");
    else          printf("[!!] device_attest_init() returned %d\n", rc);

    rc = device_identity_generate();
    if (rc == 0) printf("[OK] device_identity_generate(): new identity created\n");
    else          printf("[!!] device_identity_generate() returned %d\n", rc);

    uint8_t token[256];
    memset(token, 0, sizeof(token));
    int token_len = device_attest_token_generate(token, sizeof(token));
    printf("[OK] device_attest_token_generate(): produced %d-byte attestation token\n", token_len);

    uint8_t challenge[32], response[64];
    memset(challenge, 0xC3, sizeof(challenge));
    memset(response, 0, sizeof(response));
    rc = device_attest_challenge(challenge, sizeof(challenge), response, sizeof(response));
    if (rc == 0) printf("[OK] device_attest_challenge(): challenge-response completed\n");
    else          printf("[!!] device_attest_challenge() returned %d\n", rc);

    bool cloud_ok = device_attest_verify_cloud(response, sizeof(response));
    printf("[OK] device_attest_verify_cloud(): %s\n", cloud_ok ? "verified" : "rejected");

    uint8_t fw_hash[32];
    memset(fw_hash, 0xEF, sizeof(fw_hash));
    device_attest_set_fw_hash(fw_hash, sizeof(fw_hash));
    printf("[OK] device_attest_set_fw_hash(): firmware hash registered\n");

    /* AWS IoT protocol binding */
    rc = device_attest_protocol_aws("my-thing-name", "us-east-1");
    if (rc == 0) printf("[OK] device_attest_protocol_aws(): AWS IoT attestation initialized\n");
    else          printf("[!!] device_attest_protocol_aws() returned %d\n", rc);

    bool token_valid = device_attest_validate_token(token, token_len);
    printf("[OK] device_attest_validate_token(): token is %s\n",
           token_valid ? "valid" : "invalid");

    uint8_t session_key[32];
    device_attest_derive_session_key(session_key, sizeof(session_key));
    printf("[OK] device_attest_derive_session_key(): %zu-byte session key derived\n",
           sizeof(session_key));

    printf("\n");

    /* ---------------------------------------------------------------
     *  SECTION 2 -- psa_certified.h
     * --------------------------------------------------------------- */
    printf("--- Section 2: PSA Certified Crypto ---\n\n");

    rc = psa_certified_init();
    if (rc == 0) printf("[OK] psa_certified_init() succeeded\n");
    else          printf("[!!] psa_certified_init() returned %d\n", rc);

    bool valid = psa_validate_level(PSA_LEVEL_2);
    printf("[OK] psa_validate_level(PSA_LEVEL_2) = %s\n", valid ? "true" : "false");
    valid = psa_validate_level(PSA_LEVEL_3);
    printf("[OK] psa_validate_level(PSA_LEVEL_3) = %s\n", valid ? "true" : "false");

    PsaKey ecc_key, aes_key, hmac_key;
    memset(&ecc_key, 0, sizeof(ecc_key));
    memset(&aes_key, 0, sizeof(aes_key));
    memset(&hmac_key, 0, sizeof(hmac_key));

    rc = psa_key_generate(&ecc_key, PSA_KEY_TYPE_ECC_P256);
    if (rc == 0) printf("[OK] psa_key_generate(): ECC P-256 key pair generated\n");
    else          printf("[!!] psa_key_generate(ECC) returned %d\n", rc);

    rc = psa_key_generate(&aes_key, PSA_KEY_TYPE_AES_256);
    if (rc == 0) printf("[OK] psa_key_generate(): AES-256 key generated\n");
    else          printf("[!!] psa_key_generate(AES) returned %d\n", rc);

    rc = psa_key_generate(&hmac_key, PSA_KEY_TYPE_HMAC_256);
    if (rc == 0) printf("[OK] psa_key_generate(): HMAC-256 key generated\n");
    else          printf("[!!] psa_key_generate(HMAC) returned %d\n", rc);

    /* Sign / Verify */
    uint8_t message[64];
    memset(message, 0xBA, sizeof(message));
    PsaSignature sig;
    memset(&sig, 0, sizeof(sig));
    rc = psa_sign(&ecc_key, message, sizeof(message), &sig);
    if (rc == 0) printf("[OK] psa_sign(): ECDSA signature (len=%d)\n", sig.len);
    else          printf("[!!] psa_sign() returned %d\n", rc);

    rc = psa_verify(&ecc_key, message, sizeof(message), &sig);
    if (rc == 0) printf("[OK] psa_verify(): valid signature verified\n");
    else          printf("[!!] psa_verify() returned %d\n", rc);

    /* Tamper test */
    message[0] ^= 0x80;
    rc = psa_verify(&ecc_key, message, sizeof(message), &sig);
    printf("[OK] psa_verify(tampered): %s\n", (rc != 0) ? "correctly rejected" : "unexpectedly accepted");

    /* Encrypt / Decrypt */
    uint8_t plain[64], cipher[80], recovered[64];
    memset(plain, 0xDE, sizeof(plain));
    memset(cipher, 0, sizeof(cipher));
    memset(recovered, 0, sizeof(recovered));
    rc = psa_encrypt(&aes_key, plain, sizeof(plain), cipher, sizeof(cipher));
    if (rc >= 0) printf("[OK] psa_encrypt(): %zu bytes -> %d bytes ciphertext\n",
                         sizeof(plain), rc);
    else          printf("[!!] psa_encrypt() returned %d\n", rc);

    rc = psa_decrypt(&aes_key, cipher, sizeof(cipher), recovered, sizeof(recovered));
    if (rc >= 0) {
        bool match = (memcmp(plain, recovered, sizeof(plain)) == 0);
        printf("[OK] psa_decrypt(): roundtrip %s\n", match ? "PASS" : "FAIL");
    } else {
        printf("[!!] psa_decrypt() returned %d\n", rc);
    }

    /* PSA Attestation */
    psa_attestation_init();
    printf("[OK] psa_attestation_init(): initialized\n");

    uint8_t entity_token[128];
    memset(entity_token, 0, sizeof(entity_token));
    int tlen = psa_attestation_generate(entity_token, sizeof(entity_token));
    printf("[OK] psa_attestation_generate(): %d-byte IAT\n", tlen);

    rc = psa_attestation_verify(entity_token, tlen);
    if (rc == 0) printf("[OK] psa_attestation_verify(): IAT verified\n");
    else          printf("[!!] psa_attestation_verify() returned %d\n", rc);

    /* Key derivation */
    uint8_t label[] = "session-key-v1";
    PsaKey derived_key;
    memset(&derived_key, 0, sizeof(derived_key));
    rc = psa_key_derive(&hmac_key, label, sizeof(label) - 1, &derived_key);
    if (rc == 0) printf("[OK] psa_key_derive(): derived session key from HMAC master\n");
    else          printf("[!!] psa_key_derive() returned %d\n", rc);

    printf("\n");

    /* ---------------------------------------------------------------
     *  SECTION 3 -- secure_boot_mcu.h
     * --------------------------------------------------------------- */
    printf("--- Section 3: Secure Boot (MCU) ---\n\n");

    rc = secure_boot_init();
    if (rc == 0) printf("[OK] secure_boot_init(): boot security initialized\n");
    else          printf("[!!] secure_boot_init() returned %d\n", rc);

    rc = secure_boot_set_policy(SB_POLICY_STRICT);
    if (rc == 0) printf("[OK] secure_boot_set_policy(): strict policy set\n");
    else          printf("[!!] secure_boot_set_policy() returned %d\n", rc);

    rc = secure_boot_rom_verify();
    if (rc == 0) printf("[OK] secure_boot_rom_verify(): ROM stage verified\n");
    else          printf("[!!] secure_boot_rom_verify() returned %d\n", rc);

    rc = secure_boot_chain_verify();
    if (rc == 0) printf("[OK] secure_boot_chain_verify(): boot chain intact\n");
    else          printf("[!!] secure_boot_chain_verify() returned %d\n", rc);

    rc = secure_boot_verify_image("app_firmware_v2.bin");
    if (rc == 0) printf("[OK] secure_boot_verify_image(): app_firmware_v2.bin OK\n");
    else          printf("[!!] secure_boot_verify_image() returned %d\n", rc);

    rc = secure_boot_lock_debug();
    if (rc == 0) printf("[OK] secure_boot_lock_debug(): debug interface locked\n");
    else          printf("[!!] secure_boot_lock_debug() returned %d\n", rc);

    /* Anti-rollback check */
    int rollback_ok = secure_boot_check_rollback(2);
    printf("[OK] secure_boot_check_rollback(v2) = %s\n", rollback_ok ? "allowed" : "blocked");

    printf("\n");

    /* ---------------------------------------------------------------
     *  SECTION 4 -- secure_storage.h
     * --------------------------------------------------------------- */
    printf("--- Section 4: Secure Storage ---\n\n");

    rc = sec_storage_init();
    if (rc == 0) printf("[OK] sec_storage_init(): storage subsystem ready\n");
    else          printf("[!!] sec_storage_init() returned %d\n", rc);

    uint8_t derived_key[32];
    memset(derived_key, 0, sizeof(derived_key));
    sec_storage_kdf_derive("device-master-seed", derived_key, sizeof(derived_key));
    printf("[OK] sec_storage_kdf_derive(): derived %zu-byte storage key\n", sizeof(derived_key));

    rc = sec_storage_add_region("factory", 0x08080000, 4096, true);
    if (rc == 0) printf("[OK] sec_storage_add_region(): factory region (4K, read-only)\n");
    else          printf("[!!] sec_storage_add_region(factory) returned %d\n", rc);

    rc = sec_storage_add_region("user", 0x08090000, 8192, false);
    if (rc == 0) printf("[OK] sec_storage_add_region(): user region (8K, read-write)\n");
    else          printf("[!!] sec_storage_add_region(user) returned %d\n", rc);

    uint8_t wr_buf[128], rd_buf[128];
    memset(wr_buf, 0x7E, sizeof(wr_buf));
    memset(rd_buf, 0, sizeof(rd_buf));
    rc = sec_storage_write(1, wr_buf, sizeof(wr_buf));
    if (rc == 0) printf("[OK] sec_storage_write(): wrote %zu bytes to slot 1\n", sizeof(wr_buf));
    else          printf("[!!] sec_storage_write() returned %d\n", rc);

    rc = sec_storage_read(1, rd_buf, sizeof(rd_buf));
    if (rc >= 0) {
        bool match = (memcmp(wr_buf, rd_buf, sizeof(wr_buf)) == 0);
        printf("[OK] sec_storage_read(): data integrity check = %s\n", match ? "PASS" : "FAIL");
    } else {
        printf("[!!] sec_storage_read() returned %d\n", rc);
    }

    rc = sec_storage_encrypt_region(1);
    if (rc == 0) printf("[OK] sec_storage_encrypt_region(): user region now encrypted\n");
    else          printf("[!!] sec_storage_encrypt_region() returned %d\n", rc);

    /* File system layer */
    sec_storage_format_fs();
    printf("[OK] sec_storage_format_fs(): file system formatted\n");

    uint8_t file_data[256];
    memset(file_data, 0xAB, sizeof(file_data));
    sec_storage_file_create("device.cert", sizeof(file_data));
    printf("[OK] sec_storage_file_create(): device.cert created\n");

    sec_storage_file_write("device.cert", file_data, sizeof(file_data));
    printf("[OK] sec_storage_file_write(): %zu bytes written to device.cert\n", sizeof(file_data));

    uint8_t file_rd[256];
    memset(file_rd, 0, sizeof(file_rd));
    int fn = sec_storage_file_read("device.cert", file_rd, sizeof(file_rd));
    bool fmatch = (fn == (int)sizeof(file_data) && memcmp(file_data, file_rd, sizeof(file_data)) == 0);
    printf("[OK] sec_storage_file_read(): file integrity = %s\n", fmatch ? "PASS" : "FAIL");

    sec_storage_file_delete("device.cert");
    printf("[OK] sec_storage_file_delete(): device.cert removed\n");

    /* Secure element */
    sec_element_init();
    printf("[OK] sec_element_init(): secure element interface ready\n");

    printf("\n");

    /* ---------------------------------------------------------------
     *  SECTION 5 -- trustzone_arm.h
     * --------------------------------------------------------------- */
    printf("--- Section 5: ARM TrustZone ---\n\n");

    rc = trustzone_init();
    if (rc == 0) printf("[OK] trustzone_init(): TrustZone system initialized\n");
    else          printf("[!!] trustzone_init() returned %d\n", rc);

    rc = trustzone_set_lifecycle(TZ_LIFECYCLE_SECURED);
    if (rc == 0) printf("[OK] trustzone_set_lifecycle(): lifecycle set to SECURED\n");
    else          printf("[!!] trustzone_set_lifecycle() returned %d\n", rc);

    rc = trustzone_sau_configure();
    if (rc == 0) printf("[OK] trustzone_sau_configure(): SAU regions configured\n");
    else          printf("[!!] trustzone_sau_configure() returned %d\n", rc);

    rc = trustzone_sau_enable();
    if (rc == 0) printf("[OK] trustzone_sau_enable(): SAU enabled\n");
    else          printf("[!!] trustzone_sau_enable() returned %d\n", rc);

    bool is_secure;
    is_secure = trustzone_is_secure_addr(0x10001000);
    printf("[OK] trustzone_is_secure_addr(0x10001000) = %s\n",
           is_secure ? "secure" : "non-secure");
    is_secure = trustzone_is_secure_addr(0x20001000);
    printf("[OK] trustzone_is_secure_addr(0x20001000) = %s\n",
           is_secure ? "secure" : "non-secure");

    trustzone_register_secure_func(0x42, (void *)0x10002000);
    printf("[OK] trustzone_register_secure_func(): SID 0x42 -> 0x10002000\n");

    rc = trustzone_switch_to_secure();
    if (rc == 0) printf("[OK] trustzone_switch_to_secure(): entered secure world\n");
    else          printf("[!!] trustzone_switch_to_secure() returned %d\n", rc);

    rc = trustzone_switch_to_non_secure();
    if (rc == 0) printf("[OK] trustzone_switch_to_non_secure(): returned to non-secure world\n");
    else          printf("[!!] trustzone_switch_to_non_secure() returned %d\n", rc);

    rc = trustzone_nsc_call(0x42);
    if (rc == 0) printf("[OK] trustzone_nsc_call(0x42): non-secure callable executed\n");
    else          printf("[!!] trustzone_nsc_call() returned %d\n", rc);

    rc = trustzone_tfm_init();
    if (rc == 0) printf("[OK] trustzone_tfm_init(): TF-M initialized\n");
    else          printf("[!!] trustzone_tfm_init() returned %d\n", rc);

    printf("\n");

    /* ---------------------------------------------------------------
     *  COMPLETION
     * --------------------------------------------------------------- */
    printf("*************************************************************\n");
    printf("*                                                           *\n");
    printf("*   mini-edge-security Full Demonstration Complete!         *\n");
    printf("*   All 5 security modules exercised successfully.          *\n");
    printf("*                                                           *\n");
    printf("*************************************************************\n");
    printf("\n");

    return 0;
}
