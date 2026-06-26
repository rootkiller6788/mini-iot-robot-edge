# OTA Protocol Specification

## 1. Protocol Overview

This document defines the over-the-air (OTA) update protocol used between IoT edge
devices and the OTA update server. The protocol operates over HTTPS/TLS 1.2+ and uses
JSON payloads for metadata exchange and binary chunked transfer for firmware download.

### 1.1 Architecture

```
+----------+        +-----------+        +-----------+
|  Device  | <----> | OTA Server | <----> | Admin UI  |
| (Client) |  HTTPS | (Backend)  |  HTTPS | (Dashboard)|
+----------+        +-----------+        +-----------+
                           |
                           v
                    +------------+
                    | CDN / S3   |
                    | (Binaries) |
                    +------------+
```

### 1.2 Protocol Version

Current version: `v1`
Supported versions: `v1`

All API endpoints are prefixed with `/api/v1/`.

## 2. Device Authentication

### 2.1 Token-Based Authentication

Every API request includes an `Authorization` header:

```
Authorization: Bearer <device_token>
```

### 2.2 Device Registration (Initial Provisioning)

```
POST /api/v1/devices/register
Content-Type: application/json

Request:
{
    "device_id":     "ESP32-001",
    "product":       "SmartSensor",
    "hw_version":    "rev3",
    "fw_version":    "1.0.0",
    "chip_id":       "0xDEADBEEF00112233",
    "public_key":    "base64-encoded-device-pubkey"
}

Response 200:
{
    "device_token":  "eyJhbGciOiJIUzI1NiIs...",
    "token_expires": 1735689600,
    "check_interval": 3600
}

Response 409 (already registered):
{
    "error": "DEVICE_ALREADY_REGISTERED",
    "message": "Device ID already exists"
}
```

### 2.3 Token Refresh

```
POST /api/v1/devices/token/refresh
Authorization: Bearer <old_token>

Response 200:
{
    "device_token":  "eyJhbGciOiJIUzI1NiIs...",
    "token_expires": 1735689600
}
```

## 3. Update Check

### 3.1 Check for Available Update

```
GET /api/v1/update/check?device_id=<id>&current_version=<ver>&channel=<ch>

Query Parameters:
- device_id:       Device identifier (required)
- current_version: Current firmware version (required)
- channel:         dev | beta | stable (default: stable)

Response 200 (update available):
{
    "update_available": true,
    "version":          "2.1.0",
    "release_id":       "rel_20240521_001",
    "size":             524288,
    "sha256":           "a1b2c3d4e5f6...",
    "download_url":     "https://cdn.ota.example.com/fw/2.1.0.bin",
    "delta_url":        "https://cdn.ota.example.com/fw/1.0.0-2.1.0.patch",
    "delta_size":       12345,
    "mandatory":        false,
    "changelog":        "Added MQTT support, fixed WiFi reconnect",
    "encryption_key_url":"https://ota.example.com/keys/2.1.0.aes",
    "signature_url":    "https://ota.example.com/sig/2.1.0.sig",
    "min_battery":      30,
    "install_window": {
        "start_hour": 2,
        "end_hour":   4
    },
    "force_after":      1719792000
}

Response 200 (no update):
{
    "update_available": false
}
```

## 4. Firmware Download

### 4.1 Begin Download Session

```
POST /api/v1/download/begin
Authorization: Bearer <token>
Content-Type: application/json

Request:
{
    "release_id": "rel_20240521_001",
    "use_delta":  true
}

Response 200:
{
    "session_id":       "dl_a1b2c3d4",
    "download_url":     "https://cdn.ota.example.com/fw/2.1.0.bin",
    "content_length":   524288,
    "accept_ranges":    true,
    "chunk_size":       65536,
    "checksum_sha256":  "a1b2c3d4e5f6..."
}
```

### 4.2 Download Chunk (Range Request)

```
GET https://cdn.ota.example.com/fw/2.1.0.bin
Range: bytes=0-65535

Response 206 Partial Content:
Content-Range: bytes 0-65535/524288
Content-Length: 65536

[binary data...]
```

### 4.3 Resume Download

```
POST /api/v1/download/resume
Authorization: Bearer <token>
Content-Type: application/json

Request:
{
    "session_id": "dl_a1b2c3d4",
    "offset":     131072
}

Response 200:
{
    "session_id":   "dl_a1b2c3d4",
    "next_offset":  131072,
    "remaining":    393216
}
```

### 4.4 Cancel Download

```
POST /api/v1/download/cancel
Authorization: Bearer <token>
Content-Type: application/json

Request:
{
    "session_id": "dl_a1b2c3d4"
}

Response 200:
{
    "cancelled": true
}
```

## 5. Update Status Reporting

### 5.1 Report Installation Result

```
POST /api/v1/update/status
Authorization: Bearer <token>
Content-Type: application/json

Request:
{
    "release_id": "rel_20240521_001",
    "version":    "2.1.0",
    "status":     "success" | "failed" | "rolled_back",
    "error_code": 0,
    "error_msg":  "",
    "timestamp":  1711059200,
    "boot_count": 1
}

Response 200:
{
    "acknowledged": true
}
```

### 5.2 Status Values

| Status | Description |
|--------|-------------|
| `downloading` | Firmware download in progress |
| `downloaded` | Download complete, pending verification |
| `verifying` | Cryptographic verification in progress |
| `installing` | Writing to flash / applying delta |
| `success` | New firmware installed and booting |
| `failed` | Installation or verification failed |
| `rolled_back` | New firmware failed, reverted to previous |

## 6. Delta Updates (Optional)

### 6.1 Delta Patch Format

The delta patch is a binary file with the structure defined in `delta_update.h`:

```
Delta Patch File:
+-----------------------------+
| delta_patch_header_t        |  (128 bytes)
+-----------------------------+
| Operation sequence          |
|  [OP_ADD: len(2) + data]   |
|  [OP_COPY: len(2) + pos(4)]|
|  [OP_SEEK: end marker]     |
+-----------------------------+
```

### 6.2 Delta Endpoint

```
GET /api/v1/update/delta?from=<old_ver>&to=<new_ver>
```

The server computes the delta on-the-fly or serves a pre-generated patch.

## 7. Rollout Management (Admin)

### 7.1 Create Rollout Plan

```
POST /api/v1/admin/rollout/create
Authorization: Bearer <admin_token>
Content-Type: application/json

Request:
{
    "release_id":         "rel_20240521_001",
    "product":            "SmartSensor",
    "steps":              [10, 25, 50, 75, 100],
    "step_duration_hours": 24,
    "success_threshold":   95,
    "auto_rollback":       true,
    "target_groups":       ["beta-testers", "production-east"]
}
```

### 7.2 Advance Rollout

```
POST /api/v1/admin/rollout/advance
Authorization: Bearer <admin_token>
Content-Type: application/json

Request:
{
    "plan_id": "plan_a1b2c3d4"
}

Response 200:
{
    "current_step": 2,
    "percentage":   25,
    "state":        "active"
}
```

## 8. Error Codes

| HTTP Status | Error Code | Description |
|-------------|------------|-------------|
| 400 | `BAD_REQUEST` | Malformed request |
| 401 | `UNAUTHORIZED` | Invalid or expired token |
| 403 | `FORBIDDEN` | Device not authorized for this release |
| 404 | `NOT_FOUND` | Release or session not found |
| 409 | `CONFLICT` | Device already registered |
| 429 | `RATE_LIMITED` | Too many requests |
| 500 | `SERVER_ERROR` | Internal server error |
| 503 | `MAINTENANCE` | Server under maintenance |

## 9. Security Considerations

1. All communication over TLS 1.2 or 1.3
2. Server certificate pinned in device firmware
3. Device tokens expire (default 30 days)
4. Firmware integrity verified via SHA-256 checksum
5. Firmware authenticity verified via ECDSA/Ed25519 signature
6. Anti-rollback via monotonic counter
7. Download resume tokens are single-use
8. Rate limiting per device (max 1 check per hour)

## 10. Recommended Intervals

| Parameter | Value |
|-----------|-------|
| Update check interval | 1-24 hours (configurable) |
| Token refresh | 7 days before expiry |
| Download chunk size | 4-64 KB (based on RAM) |
| Max download retries | 3 per chunk |
| Reboot delay after install | 5-60 seconds |
| Status report retry | 3 times at 60s intervals |
