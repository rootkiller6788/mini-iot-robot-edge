/*
 * test_core.c - Core Unit Tests for mini-edge-security
 *
 * Tests all five sub-modules:
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

/* ---- test harness ---- */
static int tests_run = 0, tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  TEST %s ... ", name); } while(0)
#define PASS()     do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg)  do { printf("FAIL: %s\n", msg); return 1; } while(0)
#define CHECK(cond, msg) if (!(cond)) FAIL(msg)

/* ================================================================
 *  device_attest.h
 * ================================================================ */

static int test_device_attest_init(void) {
    TEST("device_attest_init");
    int rc = device_attest_init();
    CHECK(rc == 0, "device_attest_init failed");
    PASS();
    return 0;
}

static int test_device_identity_generate(void) {
    TEST("device_identity_generate");
    device_attest_init();
    int rc = device_identity_generate();
    CHECK(rc == 0, "device_identity_generate failed");
    PASS();
    return 0;
}

static int test_device_attest_challenge(void) {
    TEST("device_attest_challenge / verify");
    device_attest_init();
    device_identity_generate();
    uint8_t challenge[32], response[64];
    memset(challenge, 0xA5, sizeof(challenge));
    memset(response, 0, sizeof(response));
    int rc = device_attest_challenge(challenge, sizeof(challenge), response, sizeof(response));
    CHECK(rc == 0, "device_attest_challenge failed");
    bool ok = device_attest_verify_cloud(response, sizeof(response));
    CHECK(ok, "device_attest_verify_cloud returned false for valid response");
    PASS();
    return 0;
}

static int test_device_attest_token(void) {
    TEST("device_attest_token_generate / validate");
    device_attest_init();
    device_identity_generate();
    uint8_t token[256];
    memset(token, 0, sizeof(token));
    int tlen = device_attest_token_generate(token, sizeof(token));
    CHECK(tlen > 0, "device_attest_token_generate produced zero-length token");
    bool valid = device_attest_validate_token(token, tlen);
    CHECK(valid, "device_attest_validate_token returned false for valid token");
    PASS();
    return 0;
}

/* ================================================================
 *  psa_certified.h
 * ================================================================ */

static int test_psa_certified_init(void) {
    TEST("psa_certified_init");
    int rc = psa_certified_init();
    CHECK(rc == 0, "psa_certified_init failed");
    PASS();
    return 0;
}

static int test_psa_validate_level(void) {
    TEST("psa_validate_level");
    psa_certified_init();
    bool valid = psa_validate_level(PSA_LEVEL_2);
    CHECK(valid, "psa_validate_level returned false for PSA_LEVEL_2");
    valid = psa_validate_level(PSA_LEVEL_3);
    CHECK(valid, "psa_validate_level returned false for PSA_LEVEL_3");
    PASS();
    return 0;
}

static int test_psa_key_generate(void) {
    TEST("psa_key_generate (multiple types)");
    psa_certified_init();
    PsaKey k1, k2, k3;
    int rc = psa_key_generate(&k1, PSA_KEY_TYPE_ECC_P256);
    CHECK(rc == 0, "psa_key_generate ECC_P256 failed");
    rc = psa_key_generate(&k2, PSA_KEY_TYPE_AES_256);
    CHECK(rc == 0, "psa_key_generate AES_256 failed");
    rc = psa_key_generate(&k3, PSA_KEY_TYPE_HMAC_256);
    CHECK(rc == 0, "psa_key_generate HMAC_256 failed");
    PASS();
    return 0;
}

static int test_psa_sign_verify(void) {
    TEST("psa_sign + psa_verify roundtrip");
    psa_certified_init();
    PsaKey key;
    psa_key_generate(&key, PSA_KEY_TYPE_ECC_P256);
    uint8_t msg[64];
    memset(msg, 0xBA, sizeof(msg));
    PsaSignature sig;
    memset(&sig, 0, sizeof(sig));
    int rc = psa_sign(&key, msg, sizeof(msg), &sig);
    CHECK(rc == 0, "psa_sign failed");
    CHECK(sig.len > 0, "psa_sign produced zero-length signature");
    rc = psa_verify(&key, msg, sizeof(msg), &sig);
    CHECK(rc == 0, "psa_verify failed for valid signature");

    /* Tampered message should fail */
    msg[0] ^= 0xFF;
    rc = psa_verify(&key, msg, sizeof(msg), &sig);
    CHECK(rc != 0, "psa_verify should have failed for tampered message");
    PASS();
    return 0;
}

static int test_psa_encrypt_decrypt(void) {
    TEST("psa_encrypt + psa_decrypt roundtrip");
    psa_certified_init();
    PsaKey key;
    psa_key_generate(&key, PSA_KEY_TYPE_AES_256);
    uint8_t plain[64], cipher[80], recovered[64];
    memset(plain, 0xDE, sizeof(plain));
    memset(cipher, 0, sizeof(cipher));
    memset(recovered, 0, sizeof(recovered));
    int rc = psa_encrypt(&key, plain, sizeof(plain), cipher, sizeof(cipher));
    CHECK(rc >= 0, "psa_encrypt failed");
    rc = psa_decrypt(&key, cipher, sizeof(cipher), recovered, sizeof(recovered));
    CHECK(rc >= 0, "psa_decrypt failed");
    CHECK(memcmp(plain, recovered, sizeof(plain)) == 0,
          "decrypt did not recover original plaintext");
    PASS();
    return 0;
}

/* ================================================================
 *  secure_boot_mcu.h
 * ================================================================ */

static int test_secure_boot_init(void) {
    TEST("secure_boot_init");
    int rc = secure_boot_init();
    CHECK(rc == 0, "secure_boot_init failed");
    PASS();
    return 0;
}

static int test_secure_boot_rom_verify(void) {
    TEST("secure_boot_rom_verify");
    secure_boot_init();
    int rc = secure_boot_rom_verify();
    CHECK(rc == 0, "secure_boot_rom_verify failed");
    PASS();
    return 0;
}

static int test_secure_boot_chain_verify(void) {
    TEST("secure_boot_chain_verify");
    secure_boot_init();
    secure_boot_rom_verify();
    int rc = secure_boot_chain_verify();
    CHECK(rc == 0, "secure_boot_chain_verify failed");
    PASS();
    return 0;
}

static int test_secure_boot_check_rollback(void) {
    TEST("secure_boot_check_rollback");
    secure_boot_init();
    /* Lower version should be blocked; higher should be allowed */
    int allowed = secure_boot_check_rollback(3);
    CHECK(allowed >= 0, "secure_boot_check_rollback returned error");
    PASS();
    return 0;
}

/* ================================================================
 *  secure_storage.h
 * ================================================================ */

static int test_sec_storage_init(void) {
    TEST("sec_storage_init");
    int rc = sec_storage_init();
    CHECK(rc == 0, "sec_storage_init failed");
    PASS();
    return 0;
}

static int test_sec_storage_write_read(void) {
    TEST("sec_storage_write + sec_storage_read");
    sec_storage_init();
    sec_storage_add_region("user", 0x08090000, 8192, false);
    uint8_t wr[128], rd[128];
    memset(wr, 0x7E, sizeof(wr));
    memset(rd, 0, sizeof(rd));
    int rc = sec_storage_write(0, wr, sizeof(wr));
    CHECK(rc == 0, "sec_storage_write failed");
    rc = sec_storage_read(0, rd, sizeof(rd));
    CHECK(rc >= 0, "sec_storage_read failed");
    CHECK(memcmp(wr, rd, sizeof(wr)) == 0, "stored data mismatch");
    PASS();
    return 0;
}

static int test_sec_storage_encrypt_region(void) {
    TEST("sec_storage_encrypt / decrypt_region");
    sec_storage_init();
    sec_storage_add_region("encrypted", 0x080A0000, 4096, false);
    int rc = sec_storage_encrypt_region(0);
    CHECK(rc == 0, "sec_storage_encrypt_region failed");
    PASS();
    return 0;
}

static int test_sec_storage_file_ops(void) {
    TEST("sec_storage_file_create / write / read / delete");
    sec_storage_init();
    sec_storage_format_fs();
    uint8_t fdata[128], rdata[128];
    memset(fdata, 0xAB, sizeof(fdata));
    memset(rdata, 0, sizeof(rdata));
    sec_storage_file_create("test.dat", sizeof(fdata));
    sec_storage_file_write("test.dat", fdata, sizeof(fdata));
    int n = sec_storage_file_read("test.dat", rdata, sizeof(rdata));
    CHECK(n == (int)sizeof(fdata), "sec_storage_file_read returned wrong length");
    CHECK(memcmp(fdata, rdata, sizeof(fdata)) == 0, "file data mismatch");
    sec_storage_file_delete("test.dat");
    PASS();
    return 0;
}

/* ================================================================
 *  trustzone_arm.h
 * ================================================================ */

static int test_trustzone_init(void) {
    TEST("trustzone_init");
    int rc = trustzone_init();
    CHECK(rc == 0, "trustzone_init failed");
    PASS();
    return 0;
}

static int test_trustzone_sau(void) {
    TEST("trustzone_sau_configure + enable");
    trustzone_init();
    int rc = trustzone_sau_configure();
    CHECK(rc == 0, "trustzone_sau_configure failed");
    rc = trustzone_sau_enable();
    CHECK(rc == 0, "trustzone_sau_enable failed");
    PASS();
    return 0;
}

static int test_trustzone_secure_addr(void) {
    TEST("trustzone_is_secure_addr");
    trustzone_init();
    trustzone_sau_configure();
    /* Secure region addresses (0x10000000+) should be secure */
    bool secure = trustzone_is_secure_addr(0x10001000);
    CHECK(secure, "expected secure address was not recognized");
    /* Non-secure region addresses (0x20000000+) should not be secure */
    secure = trustzone_is_secure_addr(0x20001000);
    CHECK(!secure, "expected non-secure address was recognized as secure");
    PASS();
    return 0;
}

static int test_trustzone_switch_worlds(void) {
    TEST("trustzone_switch_to_secure / non_secure");
    trustzone_init();
    trustzone_sau_configure();
    int rc = trustzone_switch_to_secure();
    CHECK(rc == 0, "trustzone_switch_to_secure failed");
    rc = trustzone_switch_to_non_secure();
    CHECK(rc == 0, "trustzone_switch_to_non_secure failed");
    PASS();
    return 0;
}

static int test_trustzone_nsc_call(void) {
    TEST("trustzone_nsc_call");
    trustzone_init();
    trustzone_register_secure_func(0x01, (void *)0x10002000);
    int rc = trustzone_nsc_call(0x01);
    CHECK(rc == 0, "trustzone_nsc_call failed");
    PASS();
    return 0;
}

/* ================================================================
 *  main
 * ================================================================ */

int main(void) {
    printf("mini-edge-security  --  Core Unit Tests\n\n");

    /* device_attest.h */
    test_device_attest_init();
    test_device_identity_generate();
    test_device_attest_challenge();
    test_device_attest_token();

    /* psa_certified.h */
    test_psa_certified_init();
    test_psa_validate_level();
    test_psa_key_generate();
    test_psa_sign_verify();
    test_psa_encrypt_decrypt();

    /* secure_boot_mcu.h */
    test_secure_boot_init();
    test_secure_boot_rom_verify();
    test_secure_boot_chain_verify();
    test_secure_boot_check_rollback();

    /* secure_storage.h */
    test_sec_storage_init();
    test_sec_storage_write_read();
    test_sec_storage_encrypt_region();
    test_sec_storage_file_ops();

    /* trustzone_arm.h */
    test_trustzone_init();
    test_trustzone_sau();
    test_trustzone_secure_addr();
    test_trustzone_switch_worlds();
    test_trustzone_nsc_call();

    printf("\n%d / %d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
