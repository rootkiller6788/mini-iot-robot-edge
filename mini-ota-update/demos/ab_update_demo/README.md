# A/B Update Demo

## Overview (概述)

This demo shows a complete A/B (dual-slot) firmware update system using the `ab_update` module.
The A/B update scheme is the foundation of reliable OTA for embedded devices: two identical
firmware slots coexist in flash, one active and one standby. New firmware is written to the
standby slot, verified, and only then swapped in — the previous version remains as a fallback.

## Architecture

```
+--------------------------------------------------+
|                  FLASH / NOR / NAND               |
|  +---------------------------------------------+ |
|  |  Partition Header (metadata, slot table)    | |
|  +---------------------------------------------+ |
|  |  SLOT A (offset: HEADER_SIZE)               | |
|  |  - Firmware image                            | |
|  |  - Active or Standby                         | |
|  |  - Version, CRC, bootable flag               | |
|  +---------------------------------------------+ |
|  |  SLOT B (offset: HEADER_SIZE + SLOT_SIZE)   | |
|  |  - Firmware image                            | |
|  |  - Active or Standby                         | |
|  |  - Version, CRC, bootable flag               | |
|  +---------------------------------------------+ |
+--------------------------------------------------+
```

## Data Flow

```
+----------+     +----------+     +----------+     +----------+
| Factory  |     | Download |     | Verify   |     | Reboot   |
| Image    | --> | to       | --> | Standby  | --> | into     |
| (Slot A) |     | Standby  |     | Slot     |     | Standby  |
+----------+     | (Slot B) |     +----------+     +----------+
                 +----------+
                      ^
                      |
                 +----------+
                 | HTTP/    |
                 | CoAP/    |
                 | MQTT     |
                 +----------+
```

## Boot Sequence

1. **Power-on / Reset** — Bootloader starts
2. **Load partition header** — Read metadata from flash offset 0
3. **Validate CRC** — Check partition header integrity
4. **Evaluate slots**:
   - If only one slot is bootable, select it
   - If both slots are bootable, select by priority, then by version
   - If no slot is bootable, fall back to slot A
5. **Increment boot counter** — Track boot attempts for rollback detection
6. **Jump to firmware entry point** — Execute the selected image

## Update Sequence

```
1. Device checks for update (HTTP GET /api/check?version=1.0.0)
2. Server responds with new firmware URL
3. Client downloads firmware in chunks (with resume)
4. Each chunk written to STANDBY slot at incremental offsets
5. Full image CRC verified against expected hash
6. Slot metadata updated: bootable=1, version=2.0.0
7. Partition header saved to flash
8. Device reboots
9. Bootloader sees STANDBY now has bootable flag
10. Bootloader selects STANDBY as new active slot
```

## Rollback Mechanism

If the new firmware fails to boot (crashes, hangs, or reports failure):

```
Boot Count > Limit?
     |
  NO |---> Normal boot
     |
  YES |---> Mark slot INVALID
         |
         +---> Select previous slot
              |
              +---> Reset boot counter
                   |
                   +---> Boot previous firmware
```

### Boot Count Tracking

- Every boot increments a persistent counter (eFuse, RTC backup RAM, or flash region)
- If boot mark succeeds (`ab_update_mark_boot_successful`), counter resets to 0
- If counter exceeds `AB_UPDATE_BOOT_COUNT_MAX` (default 7), rollback triggers
- This protects against boot loops from a bad firmware update

## Slot Metadata

| Field | Description |
|-------|-------------|
| `slot_id` | 0 for slot A, 1 for slot B |
| `slot_name` | Human-readable name ("slot_A", "slot_B") |
| `slot_offset` | Flash byte offset where slot begins |
| `slot_size` | Total bytes available in this slot |
| `state` | UNKNOWN / INACTIVE / BOOTING / ACTIVE / INVALID |
| `priority` | Higher number = preferred boot target |
| `bootable` | 0=not bootable, 1=bootable |
| `boot_successful` | 0=boot in progress/pending, 1=confirmed good |
| `boot_attempts` | How many times this slot has been booted |
| `boot_count_limit` | Max attempts before rollback (default 7) |
| `version_major` | Firmware major version |
| `version_minor` | Firmware minor version |
| `version_patch` | Firmware patch version |
| `image_hash` | SHA-256 of firmware image |
| `image_size` | Size of firmware in bytes |
| `image_crc` | CRC-32 of firmware image |
| `last_boot_time` | Unix timestamp of last boot |
| `update_timestamp` | Unix timestamp of last update |

## State Machine

```
     +----------+
     | UNKNOWN  |  (initial state, never used)
     +----------+
          |
          v
     +----------+     write firmware    +----------+
     | INACTIVE | -------------------> | INACTIVE |
     +----------+     (bootable=1)     +----------+
          |                                   |
   boot selected                         boot selected
          |                                   |
          v                                   v
     +----------+                       +----------+
     | BOOTING  |                       | BOOTING  |
     +----------+                       +----------+
          |                                   |
   success?     fail?                  success?     fail?
      |            |                      |            |
      v            v                      v            v
  +--------+  +---------+           +--------+  +---------+
  | ACTIVE |  | INVALID |           | ACTIVE |  | INVALID |
  +--------+  +---------+           +--------+  +---------+
      |            |                      |            |
      +-- can be --+                      +-- can be --+
         downgraded                          downgraded
```

## Flash Layout Options

### Option 1: Single Flash Chip
```
0x000000 +------------------+
         | Partition Header  |  512 bytes
0x000200 +------------------+
         | Slot A           |  N/2 bytes
         +------------------+
         | Slot B           |  N/2 bytes
         +------------------+
```

### Option 2: Dual Flash Chip
```
Chip 0:  [Partition Header | Slot A | ...]
Chip 1:  [Slot B | ...]
```

### Option 3: External Storage (SD/MMC)
```
Fileystem:
  /ota/partition.bin    (header)
  /ota/slot_a.bin       (firmware A)
  /ota/slot_b.bin       (firmware B)
```

## API Usage Example

```c
#include "ab_update.h"

// Define HAL (Hardware Abstraction Layer)
ab_update_hal_t hal = {
    .flash_read  = my_flash_read,
    .flash_write = my_flash_write,
    .flash_erase = my_flash_erase,
    .get_boot_count = my_boot_count_read,
    .reset_boot_count = my_boot_count_reset,
    .reboot = my_reboot,
    .get_current_time = my_get_time,
};

// Initialize and load partition
ab_update_ctx_t *ab = ab_update_init(&hal);
ab_update_partition_load(ab);

// Select which slot to boot
uint8_t boot_slot;
ab_update_select_boot_slot(ab, &boot_slot);
jump_to_firmware(boot_slot);

// ... firmware runs ...

// Mark boot as successful (call early in main)
ab_update_mark_boot_successful(ab);

// Write new firmware to standby
uint8_t standby;
ab_update_get_standby_slot(ab, &standby);
for (int off = 0; off < fw_size; off += 256) {
    ab_update_write_to_standby(ab, fw_data + off, 256, off);
}

// Verify and switch
ab_update_verify_standby(ab, expected_hash);
ab_update_set_standby_bootable(ab, 2, 0, 0);
ab_update_switch_and_reboot(ab);
```

## Integration Checklist

- [ ] Implement `flash_read`, `flash_write`, `flash_erase` for your MCU
- [ ] Set partition offset/size based on your flash layout
- [ ] Implement boot counter persistence (eFuse, RTC RAM, or dedicated flash page)
- [ ] Call `ab_update_select_boot_slot` from bootloader
- [ ] Call `ab_update_mark_boot_successful` early in application main()
- [ ] Wire up `reboot` callback to your MCU reset function
- [ ] Ensure flash erase is aligned to your MCU's sector/page size

## Build & Run

```bash
cd demos/ab_update_demo
make clean && make
./ab_update_demo
```

Expected output:

```
=== Basic OTA Update Demo ===
1. Initializing A/B partition...
   Result: OK
2. Loading partition table: OK
3. Active: 0, Standby: 1
4. Boot slot selected: 0
5. Boot marked successful
6. Written v1.0.0 to standby slot
7. Verify standby: OK
8. Standby set bootable
9. Boot limit exceeded: no
10. Switching to new firmware and rebooting...
[BOOT] Rebooting into slot 1...
=== Demo complete ===
```
