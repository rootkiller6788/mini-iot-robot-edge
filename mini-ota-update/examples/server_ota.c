#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ota_server.h"

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("=== OTA Server Demo ===\n\n");

    ota_server_config_t config;
    memset(&config, 0, sizeof(config));
    config.base_url = "https://ota.example.com/api/v1";
    config.api_key = "sk-ota-server-key-2024";
    config.http_get = NULL;
    config.http_free = NULL;
    config.ctx = NULL;

    ota_server_result_t res = ota_server_init(&config);
    printf("1. Server init: %s\n", ota_server_result_str(res));

    ota_device_info_t device;
    memset(&device, 0, sizeof(device));
    strncpy(device.device_id, "ESP32-001", OTA_SERVER_DEVICE_ID_MAX - 1);
    strncpy(device.product, "SmartSensor", OTA_SERVER_PRODUCT_MAX - 1);
    strncpy(device.current_version, "1.0.0", OTA_SERVER_VERSION_MAX - 1);
    device.channel = OTA_CHANNEL_STABLE;
    device.last_seen = (uint32_t)time(NULL);
    device.enrolled = true;

    char token[OTA_SERVER_AUTH_TOKEN_MAX];
    res = ota_server_register_device(&device, token, sizeof(token));
    printf("2. Register device: %s (token: %s)\n",
           ota_server_result_str(res), token);

    ota_firmware_release_t release_v2;
    memset(&release_v2, 0, sizeof(release_v2));
    strncpy(release_v2.product, "SmartSensor", OTA_SERVER_PRODUCT_MAX - 1);
    strncpy(release_v2.version, "2.0.0", OTA_SERVER_VERSION_MAX - 1);
    release_v2.channel = OTA_CHANNEL_DEV;
    release_v2.size = 512 * 1024;
    release_v2.rollout_percentage = 10;
    strncpy(release_v2.changelog,
            "Added: MQTT support, WiFi reconnect, OTA delta updates",
            sizeof(release_v2.changelog) - 1);
    strncpy(release_v2.download_url,
            "https://ota.example.com/firmware/smartsensor-2.0.0.bin",
            OTA_SERVER_URL_MAX - 1);

    res = ota_server_create_release(&release_v2);
    printf("3. Create release v2.0.0: %s\n", ota_server_result_str(res));

    ota_firmware_release_t release_v21;
    memset(&release_v21, 0, sizeof(release_v21));
    strncpy(release_v21.product, "SmartSensor", OTA_SERVER_PRODUCT_MAX - 1);
    strncpy(release_v21.version, "2.1.0", OTA_SERVER_VERSION_MAX - 1);
    release_v21.channel = OTA_CHANNEL_BETA;
    release_v21.size = 524 * 1024;
    release_v21.rollout_percentage = 50;
    strncpy(release_v21.changelog,
            "Added: TLS 1.3, deep-sleep optimization, battery gauge",
            sizeof(release_v21.changelog) - 1);
    strncpy(release_v21.download_url,
            "https://ota.example.com/firmware/smartsensor-2.1.0.bin",
            OTA_SERVER_URL_MAX - 1);

    res = ota_server_create_release(&release_v21);
    printf("4. Create release v2.1.0: %s\n", ota_server_result_str(res));

    res = ota_server_set_channel("2.0.0", OTA_CHANNEL_BETA);
    printf("5. Promote v2.0.0 to BETA: %s\n", ota_server_result_str(res));

    ota_firmware_release_t found;
    res = ota_server_check_update("ESP32-001", "1.0.0",
                                   OTA_CHANNEL_BETA, &found);
    printf("6. Check update (BETA channel): %s\n",
           ota_server_result_str(res));
    if (res == OTA_SERVER_OK) {
        printf("   Found: %s v%s (%u bytes)\n",
               found.product, found.version, found.size);
    }

    res = ota_server_check_update("ESP32-001", "1.0.0",
                                   OTA_CHANNEL_STABLE, &found);
    printf("7. Check update (STABLE channel): %s\n",
           ota_server_result_str(res));

    ota_rollout_plan_t plan;
    memset(&plan, 0, sizeof(plan));
    strncpy(plan.product, "SmartSensor", OTA_SERVER_PRODUCT_MAX - 1);
    plan.rollout_step = 10;
    plan.target_percentage = 100;
    plan.step_duration_hours = 24;
    plan.success_rate_threshold = 95;

    res = ota_server_create_rollout_plan(&plan);
    printf("8. Create rollout plan: %s\n", ota_server_result_str(res));

    printf("9. Advancing rollout...\n");
    for (int step = 0; step < 5; step++) {
        res = ota_server_advance_rollout("SmartSensor");
        printf("   Step %d: %s\n", step + 1, ota_server_result_str(res));
    }

    ota_download_session_t session;
    res = ota_server_begin_download("ESP32-001", &session);
    printf("10. Begin download: %s (session: %s)\n",
           ota_server_result_str(res), session.session_id);
    printf("    Content-Length: %llu\n",
           (unsigned long long)session.content_length);
    printf("    Resume: %s\n",
           session.resume_supported ? "supported" : "not supported");

    printf("11. Downloading firmware chunks...\n");
    uint64_t offset = 0;
    uint32_t total_downloaded = 0;
    for (int ch = 0; ch < 4; ch++) {
        uint8_t chunk[OTA_SERVER_RANGE_SIZE];
        uint32_t received = 0;
        res = ota_server_download_chunk(session.session_id, offset,
                                         OTA_SERVER_RANGE_SIZE,
                                         chunk, &received);
        printf("    Chunk %d: offset=%llu, received=%u, result=%s\n",
               ch + 1, (unsigned long long)offset, received,
               ota_server_result_str(res));
        offset += received;
        total_downloaded += received;
    }
    printf("    Total downloaded: %u bytes\n", total_downloaded);

    res = ota_server_resume_download(session.session_id, offset, &session);
    printf("12. Resume download at offset %llu: %s\n",
           (unsigned long long)offset, ota_server_result_str(res));

    res = ota_server_report_update_status("ESP32-001", "2.1.0", true, NULL);
    printf("13. Report success: %s\n", ota_server_result_str(res));

    ota_server_unregister_device("ESP32-001");
    printf("14. Device unregistered\n");

    ota_server_deinit();
    printf("\n=== Server demo complete ===\n");
    return 0;
}
