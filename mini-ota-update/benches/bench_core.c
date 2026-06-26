#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>

static double now_ms(void) {
    return ((double)clock()) / CLOCKS_PER_SEC * 1000.0;
}

int main(int argc, char *argv[]) {
    int N = (argc >= 2) ? atoi(argv[1]) : 5000;
    if (N <= 0) N = 5000;
    int i, k;
    double t0, t1;
    volatile int dummy = 0;

    printf("=== mini-ota-update Benchmarks (N=%d) ===\n\n", N);

    /* ---- AB Update ---- */
    t0 = now_ms();
    for (i = 0; i < N; i++) {
        dummy += ab_update_init();
        dummy += ab_update_partition_init();
        dummy += ab_update_get_active_slot();
        dummy += ab_update_get_standby_slot();
        dummy += ab_update_select_boot_slot();
        dummy += ab_update_mark_boot_successful();
        dummy += ab_update_mark_boot_failed();
        dummy += ab_update_result_str(0);
    }
    t1 = now_ms();
    printf("  ab_update suite:            %d ops in %.1f ms  (%.1f µs/op)\n",
           N * 8, t1 - t0, (t1 - t0) / (N * 8) * 1000.0);

    t0 = now_ms();
    for (i = 0; i < N; i++) {
        dummy += ab_update_switch_and_reboot();
        dummy += ab_update_rollback();
    }
    t1 = now_ms();
    printf("  ab_update_switch+rollback:  %d ops in %.1f ms  (%.1f µs/op)\n",
           N * 2, t1 - t0, (t1 - t0) / (N * 2) * 1000.0);

    t0 = now_ms();
    for (i = 0; i < N; i++) {
        ab_update_deinit();
    }
    t1 = now_ms();
    printf("  ab_update_deinit:           %d ops in %.1f ms  (%.1f µs/op)\n",
           N, t1 - t0, (t1 - t0) / N * 1000.0);

    /* ---- Delta Update ---- */
    t0 = now_ms();
    for (i = 0; i < N; i++) {
        dummy += delta_sa_build("test.bin", 1024);
        dummy += delta_sa_search("test.bin", 256);
        dummy += delta_result_str(0);
    }
    t1 = now_ms();
    printf("  delta_sa_* + result_str:    %d ops in %.1f ms  (%.1f µs/op)\n",
           N * 3, t1 - t0, (t1 - t0) / (N * 3) * 1000.0);

    int delta_patch_N = N / 20;
    if (delta_patch_N < 1) delta_patch_N = 1;
    t0 = now_ms();
    for (i = 0; i < delta_patch_N; i++) {
        unsigned char old_data[4096] = {0};
        unsigned char new_data[4096] = {0};
        unsigned char patch[4096] = {0};
        unsigned char result[4096] = {0};
        for (k = 0; k < 4096; k++) {
            old_data[k] = (unsigned char)(k & 0xFF);
            new_data[k] = (unsigned char)((k * 7) & 0xFF);
        }
        dummy += delta_create_patch(old_data, 4096, new_data, 4096, patch, 4096);
        dummy += delta_apply_patch(old_data, 4096, patch, 4096, result, 4096);
        dummy += delta_verify_patch(old_data, 4096, result, 4096);
        dummy += delta_compress(old_data, 4096, result, 4096);
        dummy += delta_decompress(result, 4096, result, 4096);
    }
    t1 = now_ms();
    int total_delta = delta_patch_N * 5;
    printf("  delta_create/apply/verify:  %d ops in %.1f ms  (%.1f µs/op)\n",
           total_delta, t1 - t0, (t1 - t0) / total_delta * 1000.0);

    /* ---- OTA Client ---- */
    t0 = now_ms();
    for (i = 0; i < N; i++) {
        dummy += ota_client_init();
        dummy += ota_client_set_policy(1);
        dummy += ota_client_set_update_url("https://ota.example.com/firmware.bin");
        dummy += ota_client_set_device_id("device-001");
        dummy += ota_client_get_state();
    }
    t1 = now_ms();
    printf("  ota_client_init/set_*:      %d ops in %.1f ms  (%.1f µs/op)\n",
           N * 5, t1 - t0, (t1 - t0) / (N * 5) * 1000.0);

    int ota_dl_N = N / 10;
    if (ota_dl_N < 1) ota_dl_N = 1;
    t0 = now_ms();
    for (i = 0; i < ota_dl_N; i++) {
        dummy += ota_client_check_for_update();
        dummy += ota_client_download_firmware();
        int prog = ota_client_download_get_progress();
        dummy += prog;
        dummy += ota_client_verify_firmware();
        dummy += ota_client_install_firmware();
    }
    t1 = now_ms();
    int total_ota = ota_dl_N * 5;
    printf("  ota_client_check/download:  %d ops in %.1f ms  (%.1f µs/op)\n",
           total_ota, t1 - t0, (t1 - t0) / total_ota * 1000.0);

    /* ---- OTA Server ---- */
    t0 = now_ms();
    for (i = 0; i < N; i++) {
        dummy += ota_server_init();
        dummy += ota_server_register_device("device-001", "v1.0");
        dummy += ota_server_check_update("device-001", "v1.0");
        dummy += ota_server_get_release("device-001");
        dummy += ota_server_begin_download("device-001");
        dummy += ota_server_report_update_status("device-001", 0);
    }
    t1 = now_ms();
    printf("  ota_server_init/register:   %d ops in %.1f ms  (%.1f µs/op)\n",
           N * 6, t1 - t0, (t1 - t0) / (N * 6) * 1000.0);

    int svr_N = N / 10;
    if (svr_N < 1) svr_N = 1;
    t0 = now_ms();
    for (i = 0; i < svr_N; i++) {
        dummy += ota_server_create_release("v2.0", "firmware_v2.bin");
        dummy += ota_server_create_rollout_plan("beta", 100);
        dummy += ota_server_advance_rollout(1);
    }
    t1 = now_ms();
    int total_svr = svr_N * 3;
    printf("  ota_server_create/rollout:  %d ops in %.1f ms  (%.1f µs/op)\n",
           total_svr, t1 - t0, (t1 - t0) / total_svr * 1000.0);

    /* ---- Signed Image ---- */
    t0 = now_ms();
    for (i = 0; i < N; i++) {
        dummy += sig_img_header_parse(NULL, 0);
        dummy += sig_img_header_validate(NULL);
        dummy += sig_img_check_anti_rollback(1);
        dummy += sig_img_key_load("key.pem");
    }
    t1 = now_ms();
    printf("  sig_img_header/key_load:    %d ops in %.1f ms  (%.1f µs/op)\n",
           N * 4, t1 - t0, (t1 - t0) / (N * 4) * 1000.0);

    int sig_N = N / 20;
    if (sig_N < 1) sig_N = 1;
    t0 = now_ms();
    for (i = 0; i < sig_N; i++) {
        unsigned char payload[2048] = {0};
        unsigned char sig[256] = {0};
        unsigned char encrypted[2048] = {0};
        unsigned char decrypted[2048] = {0};
        unsigned char header_buf[512] = {0};
        for (k = 0; k < 2048; k++) {
            payload[k] = (unsigned char)(k & 0xFF);
        }
        dummy += sig_img_verify_signature(payload, 2048, sig, 256);
        dummy += sig_img_verify_hash(payload, 2048, NULL, 0);
        dummy += sig_img_verify_full(payload, 2048, sig, 256);
        dummy += sig_img_encrypt_payload(payload, 2048, encrypted, 2048);
        dummy += sig_img_decrypt_payload(encrypted, 2048, decrypted, 2048);
        dummy += sig_img_build_header(header_buf, 512, 1);
    }
    t1 = now_ms();
    int total_sig = sig_N * 6;
    printf("  sig_img_verify/encrypt:     %d ops in %.1f ms  (%.1f µs/op)\n",
           total_sig, t1 - t0, (t1 - t0) / total_sig * 1000.0);

    printf("\n=== done ===\n\n");
    return dummy ? 0 : 0;
}
