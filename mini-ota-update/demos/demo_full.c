#include "ab_update.h"
#include "delta_update.h"
#include "ota_client.h"
#include "ota_server.h"
#include "signed_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

/* ================================================================
 *  Stub HAL / Callback Implementations
 * ================================================================ */

/* --- ab_update HAL stubs --- */
static void *flash_read(uint32_t addr, size_t len, void *ctx) {
    (void)addr; (void)ctx;
    static uint8_t buf[4096];
    memset(buf, 0xAB, len < sizeof(buf) ? len : sizeof(buf));
    return buf;
}
static int flash_write(uint32_t addr, const void *data, size_t len, void *ctx) {
    (void)addr; (void)data; (void)len; (void)ctx;
    return 0;
}
static int flash_erase(uint32_t addr, size_t len, void *ctx) {
    (void)addr; (void)len; (void)ctx;
    return 0;
}
static uint32_t get_boot_count(void *ctx) { (void)ctx; return 0; }
static int set_boot_count(uint32_t count, void *ctx) { (void)count; (void)ctx; return 0; }
static int reset_boot_count(void *ctx) { (void)ctx; return 0; }
static void reboot(int slot, void *ctx) { (void)slot; (void)ctx; }
static uint32_t get_time(void *ctx) { (void)ctx; return (uint32_t)time(NULL); }

/* --- signed_image HAL stubs --- */
static uint32_t sig_hal_read_counter(void *ctx) { (void)ctx; return 0; }
static int sig_hal_write_counter(uint32_t value, void *ctx) { (void)value; (void)ctx; return 0; }
static bool sig_hal_is_efuse(void *ctx) { (void)ctx; return true; }
static int sig_hal_burn_efuse(uint32_t bit_index, void *ctx) { (void)bit_index; (void)ctx; return 0; }

/* --- delta_update alloc / io stubs --- */
static void *d_alloc(size_t size, void *ctx) { (void)ctx; return malloc(size); }
static void d_free(void *ptr, void *ctx) { (void)ctx; free(ptr); }
static int d_read_buf(uint8_t *buf, size_t size, void *ctx) {
    (void)ctx; memset(buf, 0xAB, size); return (int)size;
}
static int d_write_buf(const uint8_t *buf, size_t size, void *ctx) {
    (void)buf; (void)ctx; return (int)size;
}
static int d_seek(int64_t offset, int whence, void *ctx) {
    (void)offset; (void)whence; (void)ctx; return 0;
}

/* --- ota_client callback stubs --- */
static int cli_download(const uint8_t *url, uint64_t offset,
                         uint32_t size, uint8_t *buffer,
                         uint32_t *received, void *user_data) {
    (void)url; (void)offset; (void)user_data;
    memset(buffer, 0xCC, size);
    *received = size;
    return 0;
}
static int cli_upload(const uint8_t *url, const uint8_t *data,
                       size_t len, void *user_data) {
    (void)url; (void)data; (void)len; (void)user_data;
    return 0;
}
static void cli_progress(const ota_progress_info_t *info, void *user_data) {
    (void)info; (void)user_data;
}
static void cli_event(ota_client_state_t state, int32_t error, void *user_data) {
    (void)state; (void)error; (void)user_data;
}
static uint32_t cli_time(void) { return (uint32_t)time(NULL); }
static uint32_t cli_battery(void) { return 85; }

/* --- ota_server http stubs --- */
static void *srv_http_get(const char *url, const char *auth,
                           uint8_t **response, size_t *response_len,
                           uint32_t range_start, uint32_t range_end,
                           void *ctx) {
    (void)url; (void)auth; (void)range_start; (void)range_end; (void)ctx;
    static uint8_t rbuf[512];
    memset(rbuf, 0xBB, sizeof(rbuf));
    *response = rbuf;
    *response_len = sizeof(rbuf);
    return (void *)1;
}
static void srv_http_free(void *handle, void *ctx) {
    (void)handle; (void)ctx;
}

int main(void) {
    printf("\n");
    printf("  +----------------------------------------------------------+\n");
    printf("  |          mini-ota-update -- Full Demo                    |\n");
    printf("  |  A/B Update / Delta Update / OTA Client/Server / Signed  |\n");
    printf("  +----------------------------------------------------------+\n");
    printf("\n");

    /* ================================================================
     *  1. A/B Update Partition Management
     * ================================================================ */
    printf("--- 1. A/B Partition Management -------------------------------\n\n");

    ab_update_hal_t hal;
    memset(&hal, 0, sizeof(hal));
    hal.flash_read = flash_read;
    hal.flash_write = flash_write;
    hal.flash_erase = flash_erase;
    hal.get_boot_count = get_boot_count;
    hal.set_boot_count = set_boot_count;
    hal.reset_boot_count = reset_boot_count;
    hal.reboot = reboot;
    hal.get_current_time = get_time;
    hal.ctx = NULL;

    printf("  [init] Initializing A/B update subsystem ...\n");
    ab_update_ctx_t *ab_ctx = ab_update_init(&hal);
    if (!ab_ctx) {
        printf("  [FAIL] ab_update_init failed\n");
        return 1;
    }
    printf("  [OK]   A/B update initialized\n\n");

    printf("  [part] Initializing partition table (1 MiB total) ...\n");
    ab_result_t ab_ret = ab_update_partition_init(ab_ctx, 1024 * 1024);
    printf("  [OK]   Partition init: %s\n\n", ab_update_result_str(ab_ret));

    uint8_t active_slot = 0xFF, standby_slot = 0xFF;
    ab_update_get_active_slot(ab_ctx, &active_slot);
    ab_update_get_standby_slot(ab_ctx, &standby_slot);
    printf("  [slot] Active slot:  %u\n", active_slot);
    printf("  [slot] Standby slot: %u\n\n", standby_slot);

    printf("  [boot] Selecting boot slot ...\n");
    uint8_t boot_slot = 0xFF;
    ab_update_select_boot_slot(ab_ctx, &boot_slot);
    printf("  [OK]   Selected boot slot: %u\n\n", boot_slot);

    printf("  [mark] Marking boot successful ...\n");
    ab_ret = ab_update_mark_boot_successful(ab_ctx);
    printf("  [OK]   mark_boot_successful: %s\n\n", ab_update_result_str(ab_ret));

    printf("  [fail] Simulating boot failure on next cycle ...\n");
    ab_ret = ab_update_mark_boot_failed(ab_ctx);
    printf("  [OK]   mark_boot_failed: %s\n\n", ab_update_result_str(ab_ret));

    printf("  [check] Checking boot count limit ...\n");
    bool exceeded = false;
    ab_update_check_boot_limit(ab_ctx, &exceeded);
    printf("  [OK]   Boot limit exceeded: %s\n\n", exceeded ? "YES" : "NO");

    /* ================================================================
     *  2. Delta Firmware Update
     * ================================================================ */
    printf("--- 2. Delta Firmware Update ---------------------------------\n\n");

    const size_t FW_SIZE = 2048;
    uint8_t *old_fw = (uint8_t *)malloc(FW_SIZE);
    uint8_t *new_fw = (uint8_t *)malloc(FW_SIZE);
    size_t k;
    for (k = 0; k < FW_SIZE; k++) {
        old_fw[k] = (uint8_t)(k & 0xFF);
        new_fw[k] = (uint8_t)((k + 7) & 0xFF);
    }

    printf("  [sa] Building suffix array for old firmware (%zu bytes) ...\n",
           FW_SIZE);
    delta_suffix_array_t *sa = delta_sa_build(old_fw, FW_SIZE, d_alloc, NULL);
    if (sa) {
        printf("  [OK]   Suffix array built: length=%d\n\n", sa->length);

        printf("  [search] Searching for pattern in suffix array ...\n");
        uint8_t pattern[8] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
        size_t match_len = 0;
        int32_t pos = delta_sa_search(sa, old_fw, pattern, 8, &match_len);
        printf("  [OK]   Search result: position=%d match_len=%zu\n\n",
               pos, match_len);

        delta_sa_free(sa, d_free, NULL);
        printf("  [OK]   Suffix array freed\n\n");
    } else {
        printf("  [INFO] suffix array build returned NULL (expected with stub)\n\n");
    }

    printf("  [compress] Compressing old firmware ...\n");
    uint8_t *compressed = (uint8_t *)malloc(FW_SIZE * 2);
    uint8_t *decompressed = (uint8_t *)malloc(FW_SIZE);
    size_t comp_len = FW_SIZE * 2;
    int comp_ret = delta_compress(old_fw, FW_SIZE, compressed, &comp_len);
    printf("  [OK]     Compressed: %zu -> %zu bytes (%.1f%%)\n",
           FW_SIZE, comp_len, (double)comp_len / (double)FW_SIZE * 100.0);

    printf("  [decompress] Decompressing ...\n");
    size_t dec_len = FW_SIZE;
    int dec_ret = delta_decompress(compressed, comp_len, decompressed, &dec_len);
    printf("  [OK]     Decompressed: %zu bytes, match=%s\n\n",
           dec_len,
           (dec_ret >= 0 && dec_len == FW_SIZE &&
            memcmp(old_fw, decompressed, FW_SIZE) == 0) ? "YES" : "NO");

    /* ================================================================
     *  3. OTA Client Workflow
     * ================================================================ */
    printf("--- 3. OTA Client Workflow -----------------------------------\n\n");

    ota_client_cb_t client_cb;
    memset(&client_cb, 0, sizeof(client_cb));
    client_cb.download   = cli_download;
    client_cb.upload     = cli_upload;
    client_cb.progress   = cli_progress;
    client_cb.event      = cli_event;
    client_cb.get_time   = cli_time;
    client_cb.get_battery = cli_battery;
    client_cb.user_data  = NULL;

    printf("  [init] Initializing OTA client ...\n");
    ota_client_ctx_t *client_ctx = ota_client_init(&client_cb);
    if (!client_ctx) {
        printf("  [FAIL] ota_client_init failed\n");
        free(old_fw); free(new_fw);
        free(compressed); free(decompressed);
        ab_update_deinit(ab_ctx);
        return 1;
    }
    printf("  [OK]   Client initialized\n\n");

    printf("  [cfg]  Setting device ID: EDGE-GW-001 ...\n");
    ota_client_set_device_id(client_ctx, "EDGE-GW-001");
    printf("  [OK]   Device ID set\n\n");

    printf("  [cfg]  Setting update URL ...\n");
    ota_client_set_update_url(client_ctx,
                               "https://ota.mycorp.com/firmware/edge/v2.0.bin");
    printf("  [OK]   URL set\n\n");

    printf("  [cfg]  Setting current version ...\n");
    ota_client_set_current_version(client_ctx, "1.5.0");
    printf("  [OK]   Version set to 1.5.0\n\n");

    printf("  [cfg]  Configuring update policy (anytime, allow cell) ...\n");
    ota_update_policy_config_t policy;
    memset(&policy, 0, sizeof(policy));
    policy.policy = OTA_POLICY_ANYTIME;
    policy.max_retries = 3;
    policy.allow_cellular = true;
    policy.auto_install = true;
    policy.install_delay_s = 30;
    policy.reboot_delay_s = 5;
    ota_client_set_policy(client_ctx, &policy);
    printf("  [OK]   Policy configured\n\n");

    printf("  [check] Checking for available update ...\n");
    char new_ver[OTA_CLIENT_VERSION_MAX];
    bool update_avail = false;
    ota_client_result_t cres = ota_client_check_for_update(client_ctx,
                                                            new_ver,
                                                            sizeof(new_ver),
                                                            &update_avail);
    printf("  [OK]   Check result: %s  Available: %s\n\n",
           ota_client_result_str(cres),
           update_avail ? "YES" : "NO");

    printf("  [state] Reading client state ...\n");
    ota_client_state_t cli_state;
    ota_client_get_state(client_ctx, &cli_state);
    printf("  [OK]   Current state: %s\n\n", ota_client_state_str(cli_state));

    printf("  [dl]    Downloading firmware ...\n");
    cres = ota_client_download_firmware(client_ctx);
    printf("  [OK]   Download: %s\n\n", ota_client_result_str(cres));

    printf("  [prog]  Checking download progress ...\n");
    ota_progress_info_t prog_info;
    memset(&prog_info, 0, sizeof(prog_info));
    cres = ota_client_download_get_progress(client_ctx, &prog_info);
    printf("  [OK]   Progress: %u%% (%u/%u bytes)  state=%s\n\n",
           prog_info.percent, prog_info.bytes_downloaded,
           prog_info.bytes_total,
           ota_client_state_str(prog_info.state));

    printf("  [verify] Verifying downloaded firmware ...\n");
    cres = ota_client_verify_firmware(client_ctx);
    printf("  [OK]   Verify: %s\n\n", ota_client_result_str(cres));

    printf("  [install] Installing firmware (reboot after=true) ...\n");
    cres = ota_client_install_firmware(client_ctx, true);
    printf("  [OK]   Install: %s\n\n", ota_client_result_str(cres));

    printf("  [policy] Checking battery and time window ...\n");
    bool in_window = false, batt_ok = false;
    ota_client_check_policy_window(client_ctx, &in_window);
    ota_client_check_battery(client_ctx, &batt_ok);
    printf("  [OK]   In time window: %s  Battery OK: %s\n\n",
           in_window ? "YES" : "NO", batt_ok ? "YES" : "NO");

    /* ================================================================
     *  4. OTA Server Workflow
     * ================================================================ */
    printf("--- 4. OTA Server Workflow -----------------------------------\n\n");

    ota_server_config_t srv_cfg;
    memset(&srv_cfg, 0, sizeof(srv_cfg));
    srv_cfg.base_url = "https://ota.mycorp.com/api/v2";
    srv_cfg.api_key  = "sk-ota-server-key-2024";
    srv_cfg.http_get = srv_http_get;
    srv_cfg.http_free = srv_http_free;
    srv_cfg.ctx = NULL;

    printf("  [init] Initializing OTA server ...\n");
    ota_server_result_t sres = ota_server_init(&srv_cfg);
    printf("  [OK]   Server init: %s\n\n", ota_server_result_str(sres));

    printf("  [reg]  Registering device EDGE-GW-001 ...\n");
    ota_device_info_t dev_info;
    memset(&dev_info, 0, sizeof(dev_info));
    strncpy(dev_info.device_id, "EDGE-GW-001", OTA_SERVER_DEVICE_ID_MAX - 1);
    strncpy(dev_info.product, "EdgeGateway-Pro", OTA_SERVER_PRODUCT_MAX - 1);
    strncpy(dev_info.current_version, "1.5.0", OTA_SERVER_VERSION_MAX - 1);
    dev_info.channel = OTA_CHANNEL_STABLE;
    dev_info.enrolled = true;
    char token[OTA_SERVER_AUTH_TOKEN_MAX];
    sres = ota_server_register_device(&dev_info, token, sizeof(token));
    printf("  [OK]   Register: %s\n\n", ota_server_result_str(sres));

    printf("  [check] Checking if update is available ...\n");
    ota_firmware_release_t release;
    memset(&release, 0, sizeof(release));
    sres = ota_server_check_update("EDGE-GW-001", "1.5.0",
                                    OTA_CHANNEL_STABLE, &release);
    printf("  [OK]   Check update: %s\n\n", ota_server_result_str(sres));

    printf("  [rel]   Getting release info ...\n");
    sres = ota_server_get_release("EDGE-GW-001", &release);
    printf("  [OK]   Get release: %s\n\n", ota_server_result_str(sres));

    printf("  [dl]    Beginning download session ...\n");
    ota_download_session_t session;
    memset(&session, 0, sizeof(session));
    sres = ota_server_begin_download("EDGE-GW-001", &session);
    printf("  [OK]   Begin download: %s\n\n", ota_server_result_str(sres));

    printf("  [dl]    Downloading chunk (offset=0, size=4096) ...\n");
    uint8_t chunk_buf[4096];
    uint32_t received = 0;
    sres = ota_server_download_chunk(session.session_id, 0, 4096,
                                      chunk_buf, &received);
    printf("  [OK]   Chunk download: %s  received=%u bytes\n\n",
           ota_server_result_str(sres), received);

    printf("  [status] Reporting update status: SUCCESS ...\n");
    sres = ota_server_report_update_status("EDGE-GW-001", "2.0.0",
                                            true, "");
    printf("  [OK]   Report status: %s\n\n", ota_server_result_str(sres));

    printf("  [create] Creating new firmware release v2.0.0 ...\n");
    ota_firmware_release_t new_release;
    memset(&new_release, 0, sizeof(new_release));
    strncpy(new_release.product, "EdgeGateway-Pro", OTA_SERVER_PRODUCT_MAX - 1);
    strncpy(new_release.version, "2.0.0", OTA_SERVER_VERSION_MAX - 1);
    new_release.channel = OTA_CHANNEL_BETA;
    new_release.size = 512 * 1024;
    new_release.rollout_percentage = 10;
    new_release.mandatory = false;
    sres = ota_server_create_release(&new_release);
    printf("  [OK]   Create release: %s\n\n", ota_server_result_str(sres));

    printf("  [rollout] Creating rollout plan ...\n");
    ota_rollout_plan_t plan;
    memset(&plan, 0, sizeof(plan));
    strncpy(plan.product, "EdgeGateway-Pro", OTA_SERVER_PRODUCT_MAX - 1);
    plan.rollout_step = 1;
    plan.target_percentage = 100;
    plan.step_duration_hours = 24;
    plan.success_rate_threshold = 95;
    sres = ota_server_create_rollout_plan(&plan);
    printf("  [OK]   Create rollout: %s\n\n", ota_server_result_str(sres));

    printf("  [advance] Advancing rollout ...\n");
    sres = ota_server_advance_rollout("plan-edge-gw-001");
    printf("  [OK]   Advance rollout: %s\n\n", ota_server_result_str(sres));

    /* ================================================================
     *  5. Signed Firmware Image
     * ================================================================ */
    printf("--- 5. Signed Firmware Image ---------------------------------\n\n");

    size_t img_size = 1024;
    uint8_t *fw_image = (uint8_t *)malloc(img_size);
    for (k = 0; k < img_size; k++) fw_image[k] = (uint8_t)((k * 13) & 0xFF);

    printf("  [key]   Loading signing key (32 bytes mock key) ...\n");
    uint8_t key_material[32];
    for (k = 0; k < 32; k++) key_material[k] = (uint8_t)k;
    sig_img_public_key_t pub_key;
    sig_img_result_t sig_ret = sig_img_key_load(key_material, 32, &pub_key);
    printf("  [OK]    Key loaded: %s  algo=%s\n\n",
           sig_img_result_str(sig_ret), sig_img_algo_name(pub_key.algo));

    printf("  [hdr]   Building signed image header ...\n");
    sig_img_key_store_t key_store;
    memset(&key_store, 0, sizeof(key_store));
    key_store.key_data = key_material;
    key_store.key_len = 32;
    key_store.algo = SIG_IMG_ALGO_ECDSA_P256;
    uint8_t *signed_output = (uint8_t *)malloc(img_size + SIGNED_IMAGE_HEADER_SIZE);
    size_t signed_len = img_size + SIGNED_IMAGE_HEADER_SIZE;
    signed_image_header_t sig_header;
    sig_ret = sig_img_build_header(fw_image, img_size, &key_store,
                                    &sig_header, signed_output, &signed_len);
    printf("  [OK]    Header built: %s  output=%zu bytes\n\n",
           sig_img_result_str(sig_ret), signed_len);

    printf("  [parse] Parsing image header ...\n");
    signed_image_header_t parsed_header;
    sig_ret = sig_img_header_parse(signed_output, signed_len, &parsed_header);
    printf("  [OK]    Parse: %s\n", sig_img_result_str(sig_ret));
    printf("          magic=0x%08X  version=%u  image_size=%u\n\n",
           parsed_header.magic, parsed_header.header_version,
           parsed_header.image_size);

    printf("  [validate] Validating header ...\n");
    sig_ret = sig_img_header_validate(&parsed_header);
    printf("  [OK]    Validate: %s\n\n", sig_img_result_str(sig_ret));

    printf("  [sign]  Verifying signature ...\n");
    sig_ret = sig_img_verify_signature(signed_output, signed_len, &pub_key);
    printf("  [OK]    Signature verify: %s\n\n", sig_img_result_str(sig_ret));

    printf("  [hash]  Verifying hash ...\n");
    sig_ret = sig_img_verify_hash(signed_output, signed_len,
                                   parsed_header.image_hash);
    printf("  [OK]    Hash verify: %s\n\n", sig_img_result_str(sig_ret));

    printf("  [full]  Full verification (with verify context) ...\n");
    sig_img_verify_ctx_t verify_ctx;
    memset(&verify_ctx, 0, sizeof(verify_ctx));
    verify_ctx.keys = &pub_key;
    verify_ctx.num_keys = 1;
    verify_ctx.hal.read_counter = sig_hal_read_counter;
    verify_ctx.hal.write_counter = sig_hal_write_counter;
    verify_ctx.hal.ctx = NULL;
    sig_ret = sig_img_verify_full(signed_output, signed_len, &verify_ctx);
    printf("  [OK]    Full verify: %s\n\n", sig_img_result_str(sig_ret));

    printf("  [arb]   Anti-rollback check ...\n");
    sig_img_hal_t rollback_hal;
    memset(&rollback_hal, 0, sizeof(rollback_hal));
    rollback_hal.read_counter = sig_hal_read_counter;
    rollback_hal.write_counter = sig_hal_write_counter;
    rollback_hal.ctx = NULL;
    bool rollback_ok = true;
    sig_ret = sig_img_check_anti_rollback(&parsed_header, &rollback_hal,
                                           &rollback_ok);
    printf("  [OK]    Anti-rollback: %s  allowed=%s\n\n",
           sig_img_result_str(sig_ret), rollback_ok ? "YES" : "NO");

    printf("  [enc]   Encrypting payload (AES-128-CTR) ...\n");
    uint8_t aes_key[16];
    uint8_t iv[SIGNED_IMAGE_IV_SIZE];
    for (k = 0; k < 16; k++) aes_key[k] = (uint8_t)((k * 5 + 1) & 0xFF);
    for (k = 0; k < SIGNED_IMAGE_IV_SIZE; k++) iv[k] = (uint8_t)((k * 3) & 0xFF);
    uint8_t *encrypted = (uint8_t *)malloc(img_size + 64);
    uint8_t *decrypted = (uint8_t *)malloc(img_size);
    size_t enc_len = img_size + 64;
    sig_ret = sig_img_encrypt_payload(fw_image, img_size,
                                       aes_key, 16, iv, SIGNED_IMAGE_IV_SIZE,
                                       encrypted, &enc_len);
    printf("  [OK]    Encrypt: %s  output=%zu bytes\n\n",
           sig_img_result_str(sig_ret), enc_len);

    printf("  [dec]   Decrypting payload ...\n");
    size_t dec_img_len = img_size;
    sig_ret = sig_img_decrypt_payload(encrypted, enc_len,
                                       aes_key, 16, iv, SIGNED_IMAGE_IV_SIZE,
                                       decrypted, &dec_img_len);
    printf("  [OK]    Decrypt: %s  output=%zu bytes\n",
           sig_img_result_str(sig_ret), dec_img_len);
    printf("          Roundtrip: %s\n\n",
           (dec_img_len == img_size &&
            memcmp(fw_image, decrypted, img_size) == 0)
           ? "PASS (data intact)" : "FAIL");

    printf("  [fp]    Computing key fingerprint ...\n");
    uint8_t fingerprint[SIGNED_IMAGE_HASH_SIZE];
    sig_ret = sig_img_key_fingerprint(&pub_key, fingerprint);
    printf("  [OK]    Fingerprint: %s  first_byte=0x%02X\n\n",
           sig_img_result_str(sig_ret), fingerprint[0]);

    /* ================================================================
     *  Cleanup
     * ================================================================ */
    printf("--- 6. Cleanup ------------------------------------------------\n\n");

    printf("  [close] Shutting down OTA client ...\n");
    ota_client_deinit(client_ctx);
    printf("  [OK]   Client shut down\n\n");

    printf("  [close] Shutting down OTA server ...\n");
    ota_server_deinit();
    printf("  [OK]   Server shut down\n\n");

    printf("  [close] Shutting down A/B update subsystem ...\n");
    ab_update_deinit(ab_ctx);
    printf("  [OK]   A/B update shut down\n\n");

    free(old_fw);
    free(new_fw);
    free(compressed);
    free(decompressed);
    free(fw_image);
    free(signed_output);
    free(encrypted);
    free(decrypted);

    printf("  +----------------------------------------------------------+\n");
    printf("  |     mini-ota-update Demo Completed Successfully          |\n");
    printf("  +----------------------------------------------------------+\n");
    printf("\n");

    return 0;
}
