#include "secure_boot_mcu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("================================================================\n");
    printf("  Secure Boot MCU Demo — Chain of Trust Verification\n");
    printf("================================================================\n\n");

    SecureBootCtx ctx;
    secure_boot_init(&ctx);

    printf("[PHASE 1] ROM Boot: immutal ROM verifies first stage (SPL)\n");
    printf("  ROM hash : ");
    for (int i = 0; i < 8; i++) printf("%02X", ctx.rom_hash[i]);
    printf("...\n");
    printf("  OTP pubkey hash: ");
    for (int i = 0; i < 8; i++) printf("%02X", ctx.pubkey_hash[i]);
    printf("...\n");
    printf("  Secure boot: %s\n", ctx.secure_boot_enabled ? "ENABLED" : "DISABLED");
    printf("  Debug state: %s\n\n", ctx.debug_state == DEBUG_LOCKED ? "LOCKED" : "UNLOCKED");

    printf("[PHASE 2] Boot ROM verifies SPL (simulated)\n");
    uint8_t spl_fake[1024];
    memset(spl_fake, 0xAA, sizeof(spl_fake));
    BootError err = secure_boot_rom_verify(&ctx, spl_fake, sizeof(spl_fake));
    printf("  SPL size      : %d bytes\n", (int)sizeof(spl_fake));
    printf("  Verification  : %s\n", err == BOOT_OK ? "PASSED" : "FAILED");
    printf("  Current stage : %d (SPL)\n\n", (int)BOOT_STAGE_SPL);

    printf("[PHASE 3] Chain of trust: SPL -> U-Boot\n");
    uint8_t uboot_fake[2048];
    memset(uboot_fake, 0xCC, sizeof(uboot_fake));
    err = secure_boot_chain_verify(&ctx, BOOT_STAGE_SPL, BOOT_STAGE_UBOOT,
                                    uboot_fake, sizeof(uboot_fake));
    printf("  U-Boot size   : %d bytes\n", (int)sizeof(uboot_fake));
    printf("  Chain verify  : %s\n", err == BOOT_OK ? "PASSED" : "FAILED");
    printf("  Current stage : %d (U-Boot)\n\n", (int)BOOT_STAGE_UBOOT);

    printf("[PHASE 4] Chain of trust: U-Boot -> Kernel\n");
    uint8_t kernel_fake[4096];
    memset(kernel_fake, 0xCD, sizeof(kernel_fake));
    err = secure_boot_chain_verify(&ctx, BOOT_STAGE_UBOOT, BOOT_STAGE_KERNEL,
                                    kernel_fake, sizeof(kernel_fake));
    printf("  Kernel size   : %d bytes\n", (int)sizeof(kernel_fake));
    printf("  Chain verify  : %s\n", err == BOOT_OK ? "PASSED" : "FAILED");
    printf("  Current stage : %d (Kernel)\n\n", (int)BOOT_STAGE_KERNEL);

    printf("[PHASE 5] Signed Image Verification\n");
    SignedImage img;
    memset(&img, 0, sizeof(img));
    img.header.magic[0] = 'S'; img.header.magic[1] = 'B';
    img.header.magic[2] = 'O'; img.header.magic[3] = 'T';
    img.header.version = 1;
    img.header.image_size = 256;
    img.header.rollback_ctr = 1;
    for (int i = 0; i < 256; i++) img.payload[i] = (uint8_t)(i * 3);

    err = secure_boot_verify_image(&ctx, &img);
    printf("  Magic         : %c%c%c%c\n", img.header.magic[0], img.header.magic[1],
           img.header.magic[2], img.header.magic[3]);
    printf("  Version       : %u\n", img.header.version);
    printf("  Image size    : %u bytes\n", img.header.image_size);
    printf("  Rollback ctr  : %u\n", img.header.rollback_ctr);
    printf("  Verification  : %s\n\n", err == BOOT_OK ? "PASSED" : "FAILED");

    printf("[PHASE 6] Rollback Prevention\n");
    bool ok = secure_boot_check_rollback(&ctx, 0);
    printf("  Version 0 vs min=1 : %s\n", ok ? "ALLOWED" : "BLOCKED");
    ok = secure_boot_check_rollback(&ctx, 2);
    printf("  Version 2 vs min=1 : %s\n\n", ok ? "ALLOWED" : "BLOCKED");

    printf("[PHASE 7] Debug Authentication\n");
    uint8_t challenge[16], response[16];
    for (int i = 0; i < 16; i++) challenge[i] = (uint8_t)(0x10 + i);
    for (int i = 0; i < 16; i++)
        response[i] = challenge[i] ^ 0xA5 ^ ctx.pubkey_hash[i % PUBKEY_HASH_SIZE];
    DebugAuthState ds = secure_boot_debug_auth(&ctx, challenge, response, 16);
    printf("  Challenge     : ");
    for (int i = 0; i < 8; i++) printf("%02X", challenge[i]); printf("...\n");
    printf("  Response      : ");
    for (int i = 0; i < 8; i++) printf("%02X", response[i]); printf("...\n");
    printf("  Auth result   : %s\n\n", ds == DEBUG_UNLOCKED_TEMP ? "UNLOCKED (temp)" : "DENIED");

    printf("[PHASE 8] Boot State & OTP\n");
    uint8_t state[BOOT_STATE_SIZE];
    secure_boot_get_state(&ctx, state);
    printf("  Debug state   : %d\n", state[0]);
    printf("  Stage         : %d\n", state[1]);
    printf("  OTP fuse[0]   : 0x%08X\n", (unsigned)secure_boot_otp_read(&ctx, 0));
    printf("  OTP fuse[3]   : 0x%08X\n\n", (unsigned)secure_boot_otp_read(&ctx, 3));

    printf("================================================================\n");
    printf("  SECURE BOOT DEMO COMPLETE\n");
    printf("================================================================\n");
    return 0;
}
