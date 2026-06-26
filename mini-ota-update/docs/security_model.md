# Security Model — mini-ota-update

## 1. Threat Model

### 1.1 Assets to Protect

| Asset | Value | Attack Surface |
|-------|-------|----------------|
| Firmware image | Critical — controls device behavior | Network, flash, RAM |
| Signing private key | Critical — can sign malicious firmware | Build server, CI/CD |
| AES encryption key | High — protects firmware confidentiality | Device flash, provisioning |
| Device identity token | High — allows impersonation | RAM, persistent storage |
| Anti-rollback counter | Medium — prevents downgrade attacks | eFuse, TPM |
| Bootloader code | Critical — root of trust | Flash (protected by RDP/secure boot) |

### 1.2 Adversary Capabilities

| Level | Capabilities | Mitigation |
|-------|-------------|------------|
| **Remote (L1)** | Network interception (MITM), replay attacks | TLS 1.2+, certificate pinning, signed images |
| **Physical (L2)** | JTAG/SWD access, flash dumping, glitching | RDP Level 2, secure boot, tamper detection |
| **Provisioned (L3)** | Has valid device token, attempts rollback | Anti-rollback counter, server-side device auth |
| **Compromised Server (L4)** | OTA server fully compromised | Firmware signing offline, public key in bootloader |
| **Insider (L5)** | Access to CI/CD, signing infrastructure | HSM for keys, dual approval, audit logging |

## 2. Cryptographic Primitives

### 2.1 Signature

```
Algorithm:  ECDSA over NIST P-256 (secp256r1)
Hash:       SHA-256
Key size:   256-bit private, 512-bit public (compressed: 264-bit)
Sig size:   64-72 bytes (DER-encoded)
Standard:   FIPS 186-4
```

Alternative: Ed25519 (RFC 8032) — preferred for new designs due to:
- Deterministic signatures (no RNG needed on device)
- Constant-time verification
- Smaller signatures (64 bytes fixed)

### 2.2 Encryption

```
Algorithm:  AES-256-CTR
Key size:   256 bits (32 bytes)
IV/Nonce:   128 bits, derived from header + image hash
Mode:       Counter mode (seekable)
```

Why CTR:
- Stream cipher — no padding oracle attacks
- Parallelizable encryption/decryption
- Hardware acceleration on ARM Cortex-M (AES instructions)

### 2.3 Hashing

```
Algorithm:  SHA-256
Output:     256 bits (32 bytes)
Usage:      Firmware integrity, key fingerprinting
```

## 3. Key Lifecycle

### 3.1 Key Generation

```
+-------------------+     +-------------------+
| HSM / Secure Env  |     | Build Server      |
| (offline)         |     | (online)          |
+-------------------+     +-------------------+
        |                         |
  Generate ECDSA           Request signing
  key pair                 (image + hash)
        |                         |
  Export public key -----> Embed in bootloader
        |
  Store private key
  (never leaves HSM)
```

### 3.2 Key Storage

| Location | Key | Protection |
|----------|-----|------------|
| Bootloader flash | Public key (ECDSA) | Write-protected via MPU / RDP |
| eFuse / OTP | Public key hash | Physical one-time program |
| HSM | Private key (ECDSA) | Hardware security module |
| Device key slot | AES device key | Encrypted at rest |
| RAM (during boot) | Session keys | Zeroed after verification |

### 3.3 Key Rotation Procedure

```
Phase 1: Preparation (Week 1-2)
  - Generate new key pair (K2)
  - Build firmware with both K1_pub and K2_pub embedded
  - Deploy to all devices

Phase 2: Transition (Week 3-4)
  - Sign new firmware with K2_priv
  - Devices verify with K2_pub from bootloader
  - K1_pub remains as fallback

Phase 3: Retirement (Week 5+)
  - All devices running firmware signed with K2
  - Remove K1_pub from next bootloader build
  - Revoke K1_priv in HSM

Phase 4: Emergency Rotation (Any time)
  - Key compromised? Generate K3 immediately
  - Build emergency firmware with K3_pub
  - Force-update all devices (security override)
  - Revoke K1_priv and K2_priv
```

## 4. Anti-Rollback Design

### 4.1 Monotonic Counter (eFuse)

ESP32-style eFuse counter:
```
Block 0: [ VERSION_COUNTER_0 ] [ VERSION_COUNTER_1 ] ...
         bit0  bit1  bit2  ...  (one bit = one version increment)

Counter Value = count of burned bits
```

On each OTA:
1. Read current counter from eFuse
2. Check `firmware.min_counter_value <= current_counter`
3. If OK, and `firmware.min_counter_value > current_counter`:
   - Burn eFuse bits to match `firmware.min_counter_value`
4. If rejected: abort, report to server

### 4.2 Software Counter (Flash)

For devices without eFuse:
```
Dedicated flash page (e.g., 0x1000):
  [magic: 0x434F554E] [counter: uint32_t] [crc32]

Limitations:
  - Flash can be erased and re-written
  - Mitigation: combine with secure boot
  - Acceptable for non-safety-critical devices
```

### 4.3 TPM/SE-Based Counter

```
ATECC608A Secure Element:
  - Monotonic counter (0..2097151)
  - Stored in EEPROM inside chip
  - Increment requires authentication
  - Counter and key on same silicon
```

## 5. Encryption Key Management

### 5.1 Per-Device Key Derivation

```
Master Key (HSM) + Device Serial Number
         |
         v
    HKDF-SHA256
         |
         v
  Per-Device AES Key (256-bit)
```

### 5.2 Key Provisioning

```
Factory provisioning:
  1. HSM generates master key
  2. For each device:
     a. Read chip serial number (unique per MCU)
     b. Derive device AES key = HKDF(master_key, serial_number)
     c. Write device AES key to secure key slot
     d. Delete plaintext key from RAM
  3. Erase master key from provisioning station
```

## 6. Secure Boot Chain

### 6.1 Boot Sequence

```
ROM (mask ROM, immutable)
  |
  v
Secure Bootloader (signed, verified by ROM)
  |
  |--- Verify application signature
  |--- Check anti-rollback counter
  |--- Decrypt application (if encrypted)
  |
  v
Application Firmware (signed, verified by bootloader)
  |
  |--- Verify OTA image signature
  |--- Check anti-rollback counter
  |
  v
OTA Update Module (updates standby slot)
```

### 6.2 Fuse/OTP Configuration

```
Required security fuses:
  - SECURE_BOOT_ENABLE        = 1 (enable secure boot)
  - FLASH_CRYPT_CNT           = 0xFF (max, encryption enabled)
  - DISABLE_JTAG              = 1 (disable debug interface)
  - DISABLE_DOWNLOAD_MODE     = 1 (disable ROM download mode)
  - WDT_DISABLE_DURING_SLEEP  = 0 (keep WDT active)
  - CONSOLE_DEBUG_DISABLE     = 1 (disable ROM console output)
```

## 7. Transport Security

### 7.1 TLS Configuration

```
Minimum: TLS 1.2
Preferred: TLS 1.3

Cipher suites (embedded-optimized):
  - TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256  (ARM M4+)
  - TLS_ECDHE_ECDSA_WITH_AES_128_CCM         (ARM M0+)
  - TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305   (no AES HW)

Certificate pinning:
  - SHA-256 fingerprint of server cert embedded in firmware
  - Device verifies server cert matches pinned hash
  - Rotation requires firmware update
```

### 7.2 Mutual TLS (mTLS) Option

For high-security environments, devices present a client certificate:
```
Device Certificate (factory-provisioned):
  - Signed by factory CA
  - Contains device serial number
  - Validity: 10 years

Server validates:
  - Certificate chain to factory CA
  - Certificate not revoked (OCSP stapling)
  - Device serial matches request
```

## 8. Attack Vectors and Mitigations

### 8.1 Firmware Substitution

**Attack:** Attacker replaces firmware on OTA server or CDN.
**Mitigation:** Firmware signed offline; bootloader only trusts embedded public key.
Even a fully compromised server cannot produce valid signatures.

### 8.2 Rollback/Downgrade

**Attack:** Attacker serves old (signed) firmware with known vulnerabilities.
**Mitigation:** Anti-rollback counter in eFuse/TPM. Server tracks minimum version
per device.

### 8.3 Replay Attack

**Attack:** Attacker captures valid OTA download and replays to another device.
**Mitigation:** Each download session has unique token. Firmware is device-key
encrypted (optional).

### 8.4 Man-in-the-Middle

**Attack:** Attacker intercepts OTA download traffic.
**Mitigation:** TLS 1.2+ with certificate pinning. Firmware signed regardless of
transport security.

### 8.5 Physical Extraction

**Attack:** Attacker reads flash contents via JTAG/SWD.
**Mitigation:** RDP Level 2 (readout protection), encrypted firmware images,
secure boot prevents unsigned code execution even if flash read.

### 8.6 Power Loss During Update

**Attack:** Power fails during flash write, corrupting standby slot.
**Mitigation:** A/B scheme — active slot untouched. Standby slot verified before
switching. Resume support in download.

## 9. Compliance

| Standard | Requirement | Implementation |
|----------|-------------|----------------|
| IEC 62443-4-2 | Secure firmware update | Signed + encrypted images |
| NIST SP 800-53 | SI-7 Software Integrity | SHA-256 verification chain |
| ETSI EN 303 645 | Consumer IoT security | Anti-rollback, secure updates |
| ISO 27001 | Key management | HSM-based signing, key rotation |
| PSA Certified Level 2 | Secure boot + update | A/B slots, signed images |

## 10. Operational Security

### 10.1 Incident Response

```
Suspected key compromise:
  1. Revoke compromised key in HSM
  2. Generate new key pair
  3. Build emergency firmware update
  4. Push to all devices (override channel = EMERGENCY)
  5. Monitor rollout for 72 hours
  6. Post-mortem analysis

Suspected malicious firmware:
  1. Pause all rollouts immediately
  2. Identify affected versions and devices
  3. Block affected devices from OTA server
  4. Push rollback update to affected devices
  5. Investigate build pipeline integrity
```

### 10.2 Audit Requirements

Log the following events:
- Key generation and rotation
- Firmware signing (hash, timestamp, operator)
- Rollout creation and advancement
- Device update success/failure rates
- Anomalous update patterns (rapid re-requests)
- Failed signature verifications (potential attack indicator)
