# Signed Image OTA Demo

## Overview (概述)

This demo shows a complete signed and encrypted firmware update flow using the
`signed_image` module. Every firmware image is cryptographically signed before
deployment, and the bootloader verifies the signature before execution. Optional
AES-CTR encryption protects firmware confidentiality during transit and at rest.

## Security Model

```
+------------------+        +------------------+        +------------------+
|    Build Server  |        |    OTA Server    |        |    Device        |
|                  |        |                  |        |                  |
| 1. Compile FW    |        | 2. Host signed   |        | 3. Download      |
| 2. Hash (SHA256) | -----> |    firmware      | -----> |    signed image  |
| 3. Sign (ECDSA)  |        | 3. Device        |        | 4. Verify header |
| 4. Encrypt (AES) |        |    registration   |        | 5. Verify sig    |
| 5. Package       |        | 4. Rollout mgmt  |        | 6. Check counter |
+------------------+        +------------------+        | 7. Decrypt       |
                                                        | 8. Boot FW       |
                                                        +------------------+
```

## Firmware Image Format

```
+-- Signed Image Container --+
|                            |
|  [Header: 512 bytes]       |  <-- signed_image_header_t
|  +------------------------+|
|  | magic:    0x5349474E   ||
|  | version:  1            ||
|  | hdr_size: 512          ||
|  | img_size: N            ||
|  | img_off:  512          ||
|  | img_hash: SHA256(...)  ||  <-- hash of image[512:512+N]
|  | signature: ECDSA(...)  ||  <-- signature over hash
|  | sig_len:  64-72        ||
|  | sig_algo: ECDSA-P256   ||
|  | enc:      AES256-CTR   ||
|  | iv:       [16 bytes]   ||  <-- initialization vector
|  | key_fp:   SHA256(pub)  ||  <-- key fingerprint
|  | key_id:   1            ||
|  | min_counter: 42        ||  <-- anti-rollback
|  | hw_id:    [16 bytes]   ||  <-- hardware ID binding
|  | security_level: 2      ||
|  | flags:    0x01         ||  <-- bit0=encrypted, bit1=mandatory
|  | header_crc: CRC32(...) ||
|  +------------------------+|
|                            |
|  [Payload: N bytes]        |  <-- encrypted firmware
|  (AES-CTR encrypted)       |
|                            |
+----------------------------+
```

## Signature Algorithms Supported

| Algorithm   | Key Size | Signature Size | Security Level | MCU Suitability     |
|-------------|----------|----------------|----------------|---------------------|
| ECDSA P256  | 32 bytes | 64-72 bytes    | 128-bit        | ARM M0+ and above   |
| ECDSA P384  | 48 bytes | 96-104 bytes   | 192-bit        | ARM M4 and above    |
| Ed25519     | 32 bytes | 64 bytes       | 128-bit        | ARM M3 and above    |
| RSA 2048    | 256 bytes| 256 bytes      | 112-bit        | ARM M4 (HW accel)   |
| RSA 3072    | 384 bytes| 384 bytes      | 128-bit        | ARM M7 (HW accel)   |
| RSA 4096    | 512 bytes| 512 bytes      | 140-bit        | ARM M7 + CryptoCell |

## Key Management

### Key Hierarchy

```
Root CA Key (HSM, offline)
    |
    v
Signing Key #1 (active, 1 year validity)
    |                         
    v                         
Signing Key #2 (standby, pre-loaded for rotation)
    |
    v
Public Key embedded in bootloader (at build time)
```

### Key Rotation

1. Generate new signing key pair (signing_key_2)
2. Flash firmware with both keys: key_1 (current), key_2 (next)
3. Start signing new firmware with key_2
4. After all devices updated, remove key_1 from next build
5. Key fingerprints provide traceability

## Anti-Rollback Protection

### Monotonic Counter

```
                Device eFuse                 Firmware Image
               +-----------+                +----------------+
Version 1.0    | counter:0 |  <-- allows -- | min_counter: 0 |
               +-----------+                +----------------+
                      |
               OTA to v2.0
                      |
                      v
               +-----------+                +----------------+
Version 2.0    | counter:0 |                | min_counter: 0 |
               +-----------+                +----------------+
                      |
               Set counter to 1
                      |
                      v
               +-----------+                +----------------+
Version 2.0    | counter:1 |  <-- blocks -- | min_counter: 0 |  REJECTED
               +-----------+                +----------------+
                      |
               OTA to v2.1
                      |
                      v
               +-----------+                +----------------+
Version 2.1    | counter:1 |  <-- allows -- | min_counter: 1 |
               +-----------+                +----------------+
```

### Counter Storage Options

1. **eFuse bits** — Permanent one-time programmable. Each version increment burns a bit.
   - Pro: Physically irreversible
   - Con: Limited number of bits (32-256 typical)

2. **TPM (Trusted Platform Module)** — Dedicated security chip
   - Pro: Unlimited updates, attestation support
   - Con: Additional BOM cost, I2C/SPI bus dependency

3. **Secure Element** (ATECC608, SE050)
   - Pro: Hardware-backed counter, key storage
   - Con: May require NDAs, licensing

4. **On-chip Secure Flash** (ESP32-S3, nRF5340)
   - Pro: No external chip needed
   - Con: Platform lock-in

## Encryption

### AES-CTR Mode

```
Plaintext:  [P0] [P1] [P2] [P3] [P4] [P5] ...
                |    |    |    |    |    |
Counter:    [C0] [C1] [C2] [C3] [C4] [C5] ...
                |    |    |    |    |    |
           +----+----+----+----+----+----+
           |    AES Key (128/256-bit)    |
           +----+----+----+----+----+----+
                |    |    |    |    |    |
Keystream:  [K0] [K1] [K2] [K3] [K4] [K5] ...
                |    |    |    |    |    |
           XOR  |    |    |    |    |    |
                |    |    |    |    |    |
Ciphertext: [C0] [C1] [C2] [C3] [C4] [C5] ...
```

### Why CTR Mode for OTA

- Stream cipher: no padding needed, output size = input size
- Seekable: encrypt/decrypt chunk N without chunk N-1
- Hardware acceleration: AES-CTR available on most MCU crypto engines
- Deterministic nonce from header prevents key reuse

## Hardware Binding

Optional hardware ID matching prevents firmware from running on unauthorized devices:

```c
uint8_t hw_id[16];      // Target hardware identifier
uint8_t hw_id_mask[16]; // Bitmask: 0xFF = must match, 0x00 = don't care

// Example: bind to specific board revision
hw_id = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, ...};  // Board serial prefix
hw_id_mask = {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, ...}; // Match first 4 bytes
```

## Verification Flow

```
+-----------------+
| Load image from |     No
| flash/SD/network|------------> ERROR
+-----------------+
        |
        v
+-----------------+     No
| Parse header    |------------> ERR_MAGIC / ERR_VERSION
+-----------------+
        |
        v
+-----------------+     No
| Validate CRC    |------------> ERR_HASH
+-----------------+
        |
        v
+-----------------+     No
| Find public key |------------> ERR_KEY
+-----------------+
        |
        v
+-----------------+     No
| Verify ECDSA sig|------------> ERR_SIGNATURE
+-----------------+
        |
        v
+-----------------+     No
| Check hash      |------------> ERR_HASH
+-----------------+
        |
        v
+-----------------+     No
| Anti-rollback   |------------> ERR_ROLLBACK
+-----------------+
        |
        v
+-----------------+     No
| HW ID match     |------------> ERR_KEY (if enabled)
+-----------------+
        |
        v
+-----------------+     No
| Decrypt payload |------------> ERR_DECRYPT
+-----------------+
        |
        v
+-----------------+
| BOOT FIRMWARE   |
+-----------------+
```

## Build Instructions

### Generate Keys (Development)

```bash
# ECDSA P256 key pair
openssl ecparam -genkey -name prime256v1 -noout -out dev_private.pem
openssl ec -in dev_private.pem -pubout -out dev_public.pem

# AES key (256-bit)
openssl rand -hex 32 > dev_aes_key.txt

# Convert public key to C header
xxd -i dev_public.der > public_key.h
```

### Build the Demo

```bash
cd demos/signed_ota_demo
make clean && make
./signed_ota_demo
```

### Expected Output

```
=== Signed Image OTA Demo ===
1. Loading signing key... OK (ECDSA-P256)
2. Compiling firmware... 458752 bytes
3. Computing SHA-256... done
4. Signing with ECDSA... 64 bytes signature
5. Encrypting payload (AES-256-CTR)... done
6. Packaging image header... done
7. Verifying image:
   - Magic:       OK
   - Version:     OK
   - Header CRC:  OK
   - Signature:   OK
   - Hash:        OK
   - Rollback:    OK (counter=0, min=0)
   - Decrypt:     OK
8. Firmware validated successfully
9. Writing to OTA partition... done
10. Rebooting into new firmware...
=== Demo complete ===
```

## Production Checklist

- [ ] Generate real signing keys (not dev keys)
- [ ] Store private key in HSM or secure CI/CD vault
- [ ] Embed public key (not private) in bootloader
- [ ] Enable hardware readout protection (RDP level 2)
- [ ] Set JTAG/SWD lock bits after programming
- [ ] Implement secure key provisioning in factory
- [ ] Enable MPU to protect bootloader flash region
- [ ] Use hardware crypto accelerator when available
- [ ] Implement secure boot chain (ROM -> bootloader -> app)
- [ ] Test anti-rollback with actual eFuse programming
- [ ] Validate firmware size fits allocated slot with header overhead
- [ ] Configure watchdog timer for verification timeout

## Security Best Practices

1. **Never embed private keys** in firmware or source control
2. **Rotate signing keys** on a schedule (every 6-12 months)
3. **Use separate keys** for dev/beta/stable channels
4. **Pin TLS certificates** for OTA download URLs
5. **Validate all input sizes** before malloc/copy operations
6. **Use constant-time comparisons** for signature verification
7. **Zero out key material** from RAM after use
8. **Implement failure rate monitoring** to detect attacks
9. **Require dual approval** for production firmware promotion
10. **Log all OTA events** for audit trail
