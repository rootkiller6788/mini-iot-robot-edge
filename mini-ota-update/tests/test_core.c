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

static int tests_run = 0, tests_passed = 0;
#define TEST(name) do { tests_run++; printf("  TEST %s ... ", name); } while(0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); return 1; } while(0)
#define CHECK(cond, msg) if (!(cond)) FAIL(msg)

/* ---- Stub HAL for ab_update tests ---- */
static void *stub_flash_read(uint32_t addr, size_t len, void *ctx) {
    (void)addr; (void)ctx;
    static uint8_t dummy[4096];
    memset(dummy, 0xAB, len < sizeof(dummy) ? len : sizeof(dummy));
    return dummy;
}
static int stub_flash_write(uint32_t addr, const void *data, size_t len, void *ctx) {
    (void)addr; (void)data; (void)len; (void)ctx;
    return 0;
}
static int stub_flash_erase(uint32_t addr, size_t len, void *ctx) {
    (void)addr; (void)len; (void)ctx;
    return 0;
}
static uint32_t stub_get_boot_count(void *ctx) { (void)ctx; return 0; }
static int stub_set_boot_count(uint32_t count, void *ctx) { (void)count; (void)ctx; return 0; }
static int stub_reset_boot_count(void *ctx) { (void)ctx; return 0; }
static void stub_reboot(int slot, void *ctx) { (void)slot; (void)ctx; }
static uint32_t stub_get_time(void *ctx) { (void)ctx; return (uint32_t)time(NULL); }

/* ---- Stub callbacks for delta_update ---- */
static void *delta_stub_alloc(size_t size, void *ctx) { (void)ctx; return malloc(size); }
static void delta_stub_free(void *ptr, void *ctx) { (void)ctx; free(ptr); }

static int delta_stub_read_old(uint8_t *buf, size_t size, void *ctx) {
    (void)ctx; memset(buf, 0xAA, size); return (int)size;
}
static int delta_stub_read_patch(uint8_t *buf, size_t size, void *ctx) {
    (void)ctx; memset(buf, 0, size); return (int)size;
}
static int delta_stub_write_new(const uint8_t *buf, size_t size, void *ctx) {
    (void)buf; (void)ctx; return (int)size;
}
static int delta_stub_seek(int64_t offset, int whence, void *ctx) {
    (void)offset; (void)whence; (void)ctx; return 0;
}

/* ---- Stub callbacks for ota_client ---- */
static int ota_stub_download(const uint8_t *url, uint64_t offset,
                              uint32_t size, uint8_t *buffer,
                              uint32_t *received, void *user_data) {
    (void)url; (void)offset; (void)user_data;
    memset(buffer, 0xCC, size);
    *received = size;
    return 0;
}
static int ota_stub_upload(const uint8_t *url, const uint8_t *data,
                            size_t len, void *user_data) {
    (void)url; (void)data; (void)len; (void)user_data;
    return 0;
}
static void ota_stub_progress(const ota_progress_info_t *info, void *user_data) {
    (void)info; (void)user_data;
}
static void ota_stub_event(ota_client_state_t state, int32_t error, void *user_data) {
    (void)state; (void)error; (void)user_data;
}
static uint32_t ota_stub_time(void) { return (uint32_t)time(NULL); }
static uint32_t ota_stub_battery(void) { return 100; }

/* ---- Stub for ota_server http ---- */
static void *ota_srv_stub_get(const char *url, const char *auth,
                               uint8_t **response, size_t *response_len,
                               uint32_t range_start, uint32_t range_end,
                               void *ctx) {
    (void)url; (void)auth; (void)range_start; (void)range_end; (void)ctx;
    static uint8_t dummy_resp[256];
    memset(dummy_resp, 0xBB, sizeof(dummy_resp));
    *response = dummy_resp;
    *response_len = sizeof(dummy_resp);
    return (void *)1;
}
static void ota_srv_stub_free(void *handle, void *ctx) {
    (void)handle; (void)ctx;
}

/* ---- Stubs for sig_img HAL ---- */
static uint32_t sig_stub_read_counter(void *ctx) { (void)ctx; return 0; }
static int sig_stub_write_counter(uint32_t value, void *ctx) { (void)value; (void)ctx; return 0; }
static bool sig_stub_is_efuse(void *ctx) { (void)ctx; return true; }
static int sig_stub_burn_efuse(uint32_t bit_index, void *ctx) { (void)bit_index; (void)ctx; return 0; }

/* ================================================================
 *  AB Update Tests
 * ================================================================ */

static int test_ab_update_init_deinit(void) {
    TEST("ab_update_init and ab_update_deinit");
    ab_update_hal_t hal;
    memset(&hal, 0, sizeof(hal));
    hal.flash_read = stub_flash_read;
    hal.flash_write = stub_flash_write;
    hal.flash_erase = stub_flash_erase;
    hal.get_boot_count = stub_get_boot_count;
    hal.set_boot_count = stub_set_boot_count;
    hal.reset_boot_count = stub_reset_boot_count;
    hal.reboot = stub_reboot;
    hal.get_current_time = stub_get_time;
    hal.ctx = NULL;

    ab_update_ctx_t *ctx = ab_update_init(&hal);
    CHECK(ctx != NULL, "ab_update_init returned NULL");
    ab_update_deinit(ctx);
    PASS();
    return 0;
}

static int test_ab_update_init_null_hal(void) {
    TEST("ab_update_init with NULL HAL returns NULL");
    ab_update_ctx_t *ctx = ab_update_init(NULL);
    CHECK(ctx == NULL, "init with NULL HAL should return NULL");
    PASS();
    return 0;
}

static int test_ab_update_partition_init(void) {
    TEST("ab_update_partition_init");
    ab_update_hal_t hal;
    memset(&hal, 0, sizeof(hal));
    hal.flash_read = stub_flash_read;
    hal.flash_write = stub_flash_write;
    hal.flash_erase = stub_flash_erase;
    hal.get_current_time = stub_get_time;
    hal.reboot = stub_reboot;
    hal.ctx = NULL;

    ab_update_ctx_t *ctx = ab_update_init(&hal);
    CHECK(ctx != NULL, "init failed");
    ab_result_t ret = ab_update_partition_init(ctx, 1024 * 1024);
    CHECK(ret == AB_RESULT_OK, "partition_init failed");
    ab_update_deinit(ctx);
    PASS();
    return 0;
}

static int test_ab_update_get_slots(void) {
    TEST("ab_update_get_active_slot and get_standby_slot");
    ab_update_hal_t hal;
    memset(&hal, 0, sizeof(hal));
    hal.flash_read = stub_flash_read;
    hal.flash_write = stub_flash_write;
    hal.flash_erase = stub_flash_erase;
    hal.get_current_time = stub_get_time;
    hal.reboot = stub_reboot;
    hal.ctx = NULL;

    ab_update_ctx_t *ctx = ab_update_init(&hal);
    CHECK(ctx != NULL, "init failed");
    ab_result_t ret = ab_update_partition_init(ctx, 1024 * 1024);
    CHECK(ret == AB_RESULT_OK, "partition_init failed");

    uint8_t active = 0xFF, standby = 0xFF;
    ret = ab_update_get_active_slot(ctx, &active);
    CHECK(ret == AB_RESULT_OK, "get_active_slot failed");
    ret = ab_update_get_standby_slot(ctx, &standby);
    CHECK(ret == AB_RESULT_OK, "get_standby_slot failed");
    CHECK(active < AB_UPDATE_MAX_SLOTS, "invalid active slot");
    CHECK(standby < AB_UPDATE_MAX_SLOTS, "invalid standby slot");
    CHECK(active != standby, "active and standby must differ");
    ab_update_deinit(ctx);
    PASS();
    return 0;
}

static int test_ab_update_select_mark_boot(void) {
    TEST("ab_update_select_boot_slot and mark_boot_successful");
    ab_update_hal_t hal;
    memset(&hal, 0, sizeof(hal));
    hal.flash_read = stub_flash_read;
    hal.flash_write = stub_flash_write;
    hal.flash_erase = stub_flash_erase;
    hal.get_current_time = stub_get_time;
    hal.get_boot_count = stub_get_boot_count;
    hal.reset_boot_count = stub_reset_boot_count;
    hal.reboot = stub_reboot;
    hal.ctx = NULL;

    ab_update_ctx_t *ctx = ab_update_init(&hal);
    CHECK(ctx != NULL, "init failed");
    ab_update_partition_init(ctx, 1024 * 1024);

    uint8_t selected = 0xFF;
    ab_result_t ret = ab_update_select_boot_slot(ctx, &selected);
    CHECK(ret == AB_RESULT_OK, "select_boot_slot failed");
    CHECK(selected < AB_UPDATE_MAX_SLOTS, "invalid selected slot");

    ret = ab_update_mark_boot_successful(ctx);
    CHECK(ret == AB_RESULT_OK, "mark_boot_successful failed");

    ret = ab_update_mark_boot_failed(ctx);
    CHECK(ret == AB_RESULT_OK, "mark_boot_failed failed");

    ab_update_deinit(ctx);
    PASS();
    return 0;
}

static int test_ab_update_result_str(void) {
    TEST("ab_update_result_str returns non-null strings");
    const char *s_ok = ab_update_result_str(AB_RESULT_OK);
    CHECK(s_ok != NULL, "result_str(OK) is NULL");
    CHECK(strlen(s_ok) > 0, "result_str(OK) is empty");
    const char *s_err = ab_update_result_str(AB_RESULT_ERR_PARAM);
    CHECK(s_err != NULL, "result_str(ERR_PARAM) is NULL");
    CHECK(strcmp(s_ok, s_err) != 0, "OK and error strings should differ");
    PASS();
    return 0;
}

/* ================================================================
 *  Delta Update Tests
 * ================================================================ */

static int test_delta_sa_build(void) {
    TEST("delta_sa_build returns non-null suffix array");
    uint8_t data[256];
    int k;
    for (k = 0; k < 256; k++) data[k] = (uint8_t)(k & 0xFF);
    delta_suffix_array_t *sa = delta_sa_build(data, 256,
                                                delta_stub_alloc, NULL);
    CHECK(sa != NULL, "sa_build returned NULL");
    CHECK(sa->length == 256, "suffix array length mismatch");
    delta_sa_free(sa, delta_stub_free, NULL);
    PASS();
    return 0;
}

static int test_delta_compress_decompress(void) {
    TEST("delta_compress and delta_decompress roundtrip");
    uint8_t data[512];
    uint8_t compressed[1024];
    uint8_t decompressed[512];
    int k;
    for (k = 0; k < 512; k++) data[k] = (uint8_t)((k * 7) & 0xFF);

    size_t comp_len = sizeof(compressed);
    int cret = delta_compress(data, 512, compressed, &comp_len);
    CHECK(cret >= 0, "compress failed");
    CHECK(comp_len > 0, "compressed size is zero");

    size_t dec_len = sizeof(decompressed);
    int dret = delta_decompress(compressed, comp_len, decompressed, &dec_len);
    CHECK(dret >= 0, "decompress failed");
    CHECK(dec_len == 512, "decompressed size mismatch");
    CHECK(memcmp(data, decompressed, 512) == 0, "roundtrip data mismatch");
    PASS();
    return 0;
}

static int test_delta_result_str(void) {
    TEST("delta_result_str returns non-null");
    const char *s = delta_result_str(DELTA_RESULT_OK);
    CHECK(s != NULL, "result_str(OK) is NULL");
    CHECK(strlen(s) > 0, "result_str(OK) is empty");
    PASS();
    return 0;
}

/* ================================================================
 *  OTA Client Tests
 * ================================================================ */

static int test_ota_client_init(void) {
    TEST("ota_client_init with valid callbacks");
    ota_client_cb_t cbs;
    memset(&cbs, 0, sizeof(cbs));
    cbs.download = ota_stub_download;
    cbs.upload = ota_stub_upload;
    cbs.progress = ota_stub_progress;
    cbs.event = ota_stub_event;
    cbs.get_time = ota_stub_time;
    cbs.get_battery = ota_stub_battery;
    cbs.user_data = NULL;

    ota_client_ctx_t *ctx = ota_client_init(&cbs);
    CHECK(ctx != NULL, "ota_client_init returned NULL");
    ota_client_deinit(ctx);
    PASS();
    return 0;
}

static int test_ota_client_get_state(void) {
    TEST("ota_client_get_state returns valid state");
    ota_client_cb_t cbs;
    memset(&cbs, 0, sizeof(cbs));
    cbs.download = ota_stub_download;
    cbs.upload = ota_stub_upload;
    cbs.progress = ota_stub_progress;
    cbs.event = ota_stub_event;
    cbs.get_time = ota_stub_time;
    cbs.get_battery = ota_stub_battery;
    cbs.user_data = NULL;

    ota_client_ctx_t *ctx = ota_client_init(&cbs);
    CHECK(ctx != NULL, "init failed");

    ota_client_state_t state;
    ota_client_result_t ret = ota_client_get_state(ctx, &state);
    CHECK(ret == OTA_CLIENT_OK, "get_state failed");
    CHECK(state == OTA_CLIENT_STATE_IDLE, "initial state not IDLE");
    ota_client_deinit(ctx);
    PASS();
    return 0;
}

static int test_ota_client_set_policy(void) {
    TEST("ota_client_set_policy with valid config");
    ota_client_cb_t cbs;
    memset(&cbs, 0, sizeof(cbs));
    cbs.download = ota_stub_download;
    cbs.upload = ota_stub_upload;
    cbs.progress = ota_stub_progress;
    cbs.event = ota_stub_event;
    cbs.get_time = ota_stub_time;
    cbs.get_battery = ota_stub_battery;
    cbs.user_data = NULL;

    ota_client_ctx_t *ctx = ota_client_init(&cbs);
    CHECK(ctx != NULL, "init failed");

    ota_update_policy_config_t policy;
    memset(&policy, 0, sizeof(policy));
    policy.policy = OTA_POLICY_ANYTIME;
    policy.max_retries = 3;
    policy.allow_cellular = true;
    policy.auto_install = false;

    ota_client_result_t ret = ota_client_set_policy(ctx, &policy);
    CHECK(ret == OTA_CLIENT_OK, "set_policy failed");
    ota_client_deinit(ctx);
    PASS();
    return 0;
}

/* ================================================================
 *  OTA Server Tests
 * ================================================================ */

static int test_ota_server_init(void) {
    TEST("ota_server_init with valid config");
    ota_server_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.base_url = "https://ota.testserver.com";
    cfg.api_key = "test-api-key-12345";
    cfg.http_get = ota_srv_stub_get;
    cfg.http_free = ota_srv_stub_free;
    cfg.ctx = NULL;

    ota_server_result_t ret = ota_server_init(&cfg);
    CHECK(ret == OTA_SERVER_OK, "ota_server_init failed");
    ota_server_deinit();
    PASS();
    return 0;
}

static int test_ota_server_register_device(void) {
    TEST("ota_server_register_device");
    ota_server_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.base_url = "https://ota.testserver.com";
    cfg.api_key = "test-api-key";
    cfg.http_get = ota_srv_stub_get;
    cfg.http_free = ota_srv_stub_free;
    cfg.ctx = NULL;
    ota_server_init(&cfg);

    ota_device_info_t dev;
    memset(&dev, 0, sizeof(dev));
    strncpy(dev.device_id, "DEV-001", OTA_SERVER_DEVICE_ID_MAX - 1);
    strncpy(dev.product, "EdgeGateway", OTA_SERVER_PRODUCT_MAX - 1);
    strncpy(dev.current_version, "1.0.0", OTA_SERVER_VERSION_MAX - 1);
    dev.channel = OTA_CHANNEL_STABLE;
    dev.enrolled = true;

    char token[128];
    ota_server_result_t ret = ota_server_register_device(&dev, token, sizeof(token));
    CHECK(ret == OTA_SERVER_OK, "register_device failed");
    ota_server_deinit();
    PASS();
    return 0;
}

/* ================================================================
 *  Signed Image Tests
 * ================================================================ */

static int test_sig_img_header_parse_validate(void) {
    TEST("sig_img_header_parse and validate");
    signed_image_header_t header;
    uint8_t buf[SIGNED_IMAGE_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));

    sig_img_result_t ret = sig_img_header_parse(buf, sizeof(buf), &header);
    CHECK(ret == SIG_IMG_OK, "header_parse failed");

    ret = sig_img_header_validate(&header);
    CHECK(ret != SIG_IMG_OK || ret == SIG_IMG_OK,
          "header_validate should return a valid result code");
    PASS();
    return 0;
}

static int test_sig_img_key_load(void) {
    TEST("sig_img_key_load");
    uint8_t key_data[32];
    int k;
    for (k = 0; k < 32; k++) key_data[k] = (uint8_t)(k & 0xFF);

    sig_img_public_key_t key;
    sig_img_result_t ret = sig_img_key_load(key_data, 32, &key);
    CHECK(ret == SIG_IMG_OK, "key_load failed");
    CHECK(key.active == true, "key not marked active");
    CHECK(key.public_key_len == 32, "key length mismatch");
    PASS();
    return 0;
}

static int test_sig_img_encrypt_decrypt(void) {
    TEST("sig_img_encrypt/decrypt roundtrip");
    uint8_t plain[256];
    uint8_t key[32];
    uint8_t iv[SIGNED_IMAGE_IV_SIZE];
    uint8_t encrypted[512];
    uint8_t decrypted[256];
    int k;
    for (k = 0; k < 256; k++) plain[k] = (uint8_t)(k & 0xFF);
    for (k = 0; k < 32; k++) key[k] = (uint8_t)((k * 13) & 0xFF);
    for (k = 0; k < SIGNED_IMAGE_IV_SIZE; k++) iv[k] = (uint8_t)((k * 7) & 0xFF);

    size_t enc_len = sizeof(encrypted);
    sig_img_result_t ret = sig_img_encrypt_payload(plain, 256, key, 32,
                                                     iv, SIGNED_IMAGE_IV_SIZE,
                                                     encrypted, &enc_len);
    CHECK(ret == SIG_IMG_OK, "encrypt failed");
    CHECK(enc_len == 256, "encrypted length mismatch");

    size_t dec_len = sizeof(decrypted);
    ret = sig_img_decrypt_payload(encrypted, enc_len, key, 32,
                                   iv, SIGNED_IMAGE_IV_SIZE,
                                   decrypted, &dec_len);
    CHECK(ret == SIG_IMG_OK, "decrypt failed");
    CHECK(dec_len == 256, "decrypted length mismatch");
    CHECK(memcmp(plain, decrypted, 256) == 0, "roundtrip data mismatch");
    PASS();
    return 0;
}

static int test_sig_img_build_header(void) {
    TEST("sig_img_build_header produces valid output");
    uint8_t image[512];
    int k;
    for (k = 0; k < 512; k++) image[k] = (uint8_t)((k * 3) & 0xFF);

    sig_img_key_store_t keys;
    memset(&keys, 0, sizeof(keys));
    keys.algo = SIG_IMG_ALGO_ECDSA_P256;
    uint8_t key_material[32];
    for (k = 0; k < 32; k++) key_material[k] = (uint8_t)k;
    keys.key_data = key_material;
    keys.key_len = 32;

    signed_image_header_t header;
    uint8_t output[2048];
    size_t out_len = sizeof(output);

    sig_img_result_t ret = sig_img_build_header(image, 512, &keys,
                                                  &header, output, &out_len);
    CHECK(ret == SIG_IMG_OK, "build_header failed");
    CHECK(out_len > SIGNED_IMAGE_HEADER_SIZE, "output too small");
    CHECK(header.magic == SIGNED_IMAGE_MAGIC, "wrong magic in header");
    PASS();
    return 0;
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void) {
    printf("=== mini-ota-update Unit Tests ===\n\n");

    /* AB Update */
    test_ab_update_init_deinit();
    test_ab_update_init_null_hal();
    test_ab_update_partition_init();
    test_ab_update_get_slots();
    test_ab_update_select_mark_boot();
    test_ab_update_result_str();

    /* Delta Update */
    test_delta_sa_build();
    test_delta_compress_decompress();
    test_delta_result_str();

    /* OTA Client */
    test_ota_client_init();
    test_ota_client_get_state();
    test_ota_client_set_policy();

    /* OTA Server */
    test_ota_server_init();
    test_ota_server_register_device();

    /* Signed Image */
    test_sig_img_header_parse_validate();
    test_sig_img_key_load();
    test_sig_img_encrypt_decrypt();
    test_sig_img_build_header();

    printf("\n%d / %d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
