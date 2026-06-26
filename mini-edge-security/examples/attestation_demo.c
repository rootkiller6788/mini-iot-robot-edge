#include "device_attest.h"
#include "psa_certified.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("================================================================\n");
    printf("  Device Attestation Demo — Cloud IoT Onboarding\n");
    printf("================================================================\n\n");

    DeviceAttestCtx ctx;
    device_attest_init(&ctx);

    printf("[STEP 1] Device Identity Generation (Factory Injection)\n");
    uint8_t uid[16];
    for (int i = 0; i < 16; i++) uid[i] = (uint8_t)(0xDE + i * 3);
    device_identity_generate(&ctx, uid, 16);
    printf("  Device UID    : ");
    for (int i = 0; i < 8; i++) printf("%02X", ctx.identity.device_id[i]);
    printf("...\n");
    printf("  Device key    : ");
    for (int i = 0; i < 8; i++) printf("%02X", ctx.identity.device_key[i]);
    printf("...\n");
    printf("  Key source    : FACTORY\n");
    printf("  Key protected : %s\n\n", ctx.identity.key_protected ? "YES" : "NO");

    printf("[STEP 2] Set Firmware Hash and Boot State\n");
    uint8_t fw_hash[FIRMWARE_HASH_SIZE];
    for (int i = 0; i < FIRMWARE_HASH_SIZE; i++) fw_hash[i] = (uint8_t)(i * 7 + 0x30);
    device_attest_set_fw_hash(&ctx, fw_hash);
    printf("  FW hash       : ");
    for (int i = 0; i < 8; i++) printf("%02X", fw_hash[i]);
    printf("...\n");
    device_attest_set_boot_state(&ctx, (const uint8_t*)"BOOTED_SECURE", true);
    printf("  Boot state    : SECURE\n\n");

    printf("[STEP 3] Cloud Challenge-Response (AWS IoT Core)\n");
    device_attest_protocol_aws(&ctx, "greenhouse-sensor-01");
    uint8_t challenge[CHALLENGE_SIZE];
    for (int i = 0; i < CHALLENGE_SIZE; i++) challenge[i] = (uint8_t)(i + 0x40);
    device_attest_challenge(&ctx, challenge, CHALLENGE_SIZE);
    printf("  Protocol      : AWS IOT\n");
    printf("  Thing name    : greenhouse-sensor-01\n");
    printf("  Challenge     : ");
    for (int i = 0; i < 8; i++) printf("%02X", challenge[i]);
    printf("...\n\n");

    printf("[STEP 4] Sign Challenge Response\n");
    uint8_t response[RESPONSE_SIZE];
    int rlen;
    device_attest_sign_challenge(&ctx, response, &rlen);
    printf("  Response len  : %d bytes\n", rlen);
    printf("  Response      : ");
    for (int i = 0; i < 8; i++) printf("%02X", response[i]);
    printf("...\n\n");

    printf("[STEP 5] Cloud Verifies Device\n");
    bool verified = device_attest_verify_cloud(&ctx, response);
    printf("  Verification  : %s\n", verified ? "PASSED" : "REJECTED");
    printf("  State         : %d (0=REJECTED, 3=VERIFIED)\n\n", (int)ctx.state);

    printf("[STEP 6] Generate Attestation Token\n");
    device_attest_token_generate(&ctx);
    printf("  Token len     : %d bytes\n", ctx.token_len);
    printf("  Token prefix  : ");
    for (int i = 0; i < 6; i++) printf("%c", ctx.token[i]);
    printf("\n  Device ID in token: ");
    for (int i = 6; i < 14; i++) printf("%02X", ctx.token[i]);
    printf("...\n\n");

    printf("[STEP 7] Azure DPS Enrollment\n");
    DeviceAttestCtx ctx2;
    device_attest_init(&ctx2);
    device_identity_generate(&ctx2, uid, 16);
    device_attest_protocol_azure(&ctx2, "iot-hub-device-42");
    device_attest_set_fw_hash(&ctx2, fw_hash);
    device_attest_set_boot_state(&ctx2, (const uint8_t*)"BOOTED_SECURE", true);
    device_attest_challenge(&ctx2, challenge, CHALLENGE_SIZE);
    device_attest_token_generate(&ctx2);
    printf("  Protocol      : AZURE DPS\n");
    printf("  Registration  : iot-hub-device-42\n");
    printf("  Token generated: %s\n\n", ctx2.token_len > 0 ? "YES" : "NO");

    printf("[STEP 8] JITP X.509 Certificate\n");
    X509Cert jitp;
    device_attest_jitp_cert(&ctx, &jitp);
    printf("  Certificate   : JITP_REGISTRATION\n");
    printf("  Serial        : ");
    for (int i = 0; i < 8; i++) printf("%02X", jitp.serial[i]); printf("\n");
    printf("  CA            : %s\n", jitp.is_ca ? "YES" : "NO");
    bool cert_ok = device_attest_verify_device_cert(&ctx, &jitp);
    printf("  Cert valid    : %s\n\n", cert_ok ? "TRUE" : "FALSE");

    printf("[STEP 9] Derive Session Key\n");
    uint8_t session_key[DEVICE_KEY_SIZE];
    device_attest_derive_session_key(&ctx, session_key);
    printf("  Session key   : ");
    for (int i = 0; i < 8; i++) printf("%02X", session_key[i]);
    printf("...\n\n");

    printf("[STEP 10] PSA Attestation (Level 3)\n");
    PSACertifiedCtx psa_ctx;
    psa_certified_init(&psa_ctx, PSA_LEVEL3);
    PSAAttestToken psa_token;
    psa_attestation_init(&psa_ctx, &psa_token);
    psa_attestation_generate(&psa_ctx, &psa_token, challenge, CHALLENGE_SIZE);
    bool psa_ok = psa_attestation_verify(&psa_ctx, &psa_token, fw_hash);
    printf("  PSA Level     : 3\n");
    printf("  IAT generated : %d bytes\n", psa_token.token_len);
    printf("  Token valid   : %s\n", psa_ok ? "YES" : "NO");

    printf("\n================================================================\n");
    printf("  DEVICE ATTESTATION DEMO COMPLETE\n");
    printf("================================================================\n");
    return 0;
}
