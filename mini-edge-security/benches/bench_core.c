/*
 * bench_core.c - Core Benchmarks for mini-edge-security
 *
 * Measures performance of the major API functions across all five sub-modules:
 *   device_attest.h, psa_certified.h, secure_boot_mcu.h, secure_storage.h, trustzone_arm.h
 *
 * These modules use <stdbool.h> for the bool type.
 *
 * Usage: bench_core [N]
 *   N = iteration scale factor (default 5000)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#include "device_attest.h"
#include "psa_certified.h"
#include "secure_boot_mcu.h"
#include "secure_storage.h"
#include "trustzone_arm.h"

/* ---- helper: high-resolution timer ---- */
static double now_ms(void) {
    return (double)clock() * 1000.0 / (double)CLOCKS_PER_SEC;
}

/* ---- benchmark runner ---- */
static void bench_run(const char *name, void (*fn)(int), int n) {
    double t0 = now_ms();
    fn(n);
    double t1 = now_ms();
    double elapsed = t1 - t0;
    printf("  %-40s %d ops in %9.1f ms  (%8.1f µs/op)\n",
           name, n, elapsed, (elapsed * 1000.0) / (double)n);
}

/* ================================================================
 *  BENCHMARKS – device_attest.h
 * ================================================================ */

static void bm_device_attest_init(int n) {
    int scaled = n / 10;
    for (int i = 0; i < scaled; i++) {
        device_attest_init();
    }
}

static void bm_device_identity_generate(int n) {
    device_attest_init();
    int scaled = n / 10;
    for (int i = 0; i < scaled; i++) {
        device_identity_generate();
    }
}

/* ================================================================
 *  BENCHMARKS – psa_certified.h
 * ================================================================ */

static void bm_psa_sign_verify(int n) {
    psa_certified_init();
    PsaKey key;
    psa_key_generate(&key, PSA_KEY_TYPE_ECC_P256);
    uint8_t hash[32];
    memset(hash, 0xAB, sizeof(hash));
    int scaled = n / 5;
    for (int i = 0; i < scaled; i++) {
        PsaSignature sig;
        psa_sign(&key, hash, sizeof(hash), &sig);
        psa_verify(&key, hash, sizeof(hash), &sig);
    }
}

static void bm_psa_encrypt_decrypt(int n) {
    psa_certified_init();
    PsaKey key;
    psa_key_generate(&key, PSA_KEY_TYPE_AES_256);
    uint8_t plain[64], cipher[80];
    memset(plain, 0xCD, sizeof(plain));
    int scaled = n / 5;
    for (int i = 0; i < scaled; i++) {
        psa_encrypt(&key, plain, sizeof(plain), cipher, sizeof(cipher));
        psa_decrypt(&key, cipher, sizeof(cipher), plain, sizeof(plain));
    }
}

/* ================================================================
 *  BENCHMARKS – secure_boot_mcu.h
 * ================================================================ */

static void bm_secure_boot_verify(int n) {
    secure_boot_init();
    int scaled = n / 20;
    for (int i = 0; i < scaled; i++) {
        secure_boot_rom_verify();
    }
}

/* ================================================================
 *  BENCHMARKS – secure_storage.h
 * ================================================================ */

static void bm_sec_storage_read_write(int n) {
    sec_storage_init();
    sec_storage_add_region("factory", 0x08080000, 4096, true);
    uint8_t buf[128];
    memset(buf, 0x42, sizeof(buf));
    int scaled = n / 10;
    for (int i = 0; i < scaled; i++) {
        sec_storage_write(0, buf, sizeof(buf));
        sec_storage_read(0, buf, sizeof(buf));
    }
}

static void bm_sec_storage_file_ops(int n) {
    sec_storage_init();
    sec_storage_format_fs();
    uint8_t data[256];
    memset(data, 0xEF, sizeof(data));
    int scaled = n / 10;
    for (int i = 0; i < scaled; i++) {
        sec_storage_file_create("testfile.bin", sizeof(data));
        sec_storage_file_write("testfile.bin", data, sizeof(data));
    }
}

/* ================================================================
 *  BENCHMARKS – trustzone_arm.h
 * ================================================================ */

static void bm_trustzone_switch(int n) {
    trustzone_init();
    trustzone_sau_configure();
    int scaled = n / 20;
    for (int i = 0; i < scaled; i++) {
        trustzone_switch_to_secure();
    }
}

/* ================================================================
 *  main
 * ================================================================ */

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 5000;
    if (N < 1) N = 5000;

    printf("mini-edge-security  --  Core Benchmarks  (N=%d)\n\n", N);

    bench_run("device_attest_init",               bm_device_attest_init, N);
    bench_run("device_identity_generate",         bm_device_identity_generate, N);
    bench_run("psa_sign + psa_verify (roundtrip)", bm_psa_sign_verify, N);
    bench_run("psa_encrypt + psa_decrypt (rt)",   bm_psa_encrypt_decrypt, N);
    bench_run("secure_boot_rom_verify",           bm_secure_boot_verify, N);
    bench_run("sec_storage_write + read (rt)",    bm_sec_storage_read_write, N);
    bench_run("sec_storage file create + write",  bm_sec_storage_file_ops, N);
    bench_run("trustzone_switch_to_secure",       bm_trustzone_switch, N);

    printf("\nDone.\n");
    return 0;
}
