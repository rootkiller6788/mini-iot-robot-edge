#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "initramfs_rootfs.h"
#include "busybox_app.h"
#include "kernel_module.h"
#include "device_tree_overlay.h"

static void section(const char *title)
{
    printf("\n============================================================\n");
    printf("  %s\n", title);
    printf("============================================================\n");
}

static void sub(const char *title)
{
    printf("----- %s -----\n", title);
}

static void delay_step(const char *msg)
{
    printf("  ... %s\n", msg);
}

int main(void)
{
    section("Embedded Linux Boot & Root Filesystem Demo");

    /* ==================== Phase 1: Bootloader ==================== */
    section("Phase 1: Bootloader → Kernel Handoff");

    printf("┌─────────────────────────────────────────────────────┐\n");
    printf("│  Boot ROM (on-chip)                                 │\n");
    printf("│    → SPL (Secondary Program Loader)                 │\n");
    printf("│      → ATF / ARM Trusted Firmware                   │\n");
    printf("│        → U-Boot / Barebox                           │\n");
    printf("│          → Linux Kernel (zImage/uImage/FIT)         │\n");
    printf("└─────────────────────────────────────────────────────┘\n\n");

    boot_context_t ctx;
    boot_context_init(&ctx);

    printf("Kernel command line:\n  %s\n\n", ctx.kernel_cmdline);

    delay_step("ROM loads SPL from SD card / eMMC boot partition");
    delay_step("SPL initializes DRAM controller");
    delay_step("SPL loads U-Boot proper");
    delay_step("U-Boot initializes console, network, storage");
    delay_step("U-Boot loads kernel FIT image (kernel + DTB + initramfs)");

    boot_context_advance_stage(&ctx, BOOT_STAGE_BOOTLOADER);
    printf("  U-Boot: bootm 0x10000000#conf-1 (FIT image)\n");

    boot_context_advance_stage(&ctx, BOOT_STAGE_KERNEL);
    printf("  Kernel: Decompressing... Booting Linux...\n\n");

    /* ==================== Phase 2: Initramfs ==================== */
    section("Phase 2: initramfs — Early Userspace");

    initramfs_config_t *icfg = &ctx.initramfs_cfg;

    sub("Loading essential kernel modules from initramfs");
    const char *boot_modules[] = {
        "usb-common", "usbcore", "xhci-hcd", "xhci-plat-hcd",
        "dw-mci-core", "dw-mci-pltfm", "mmc-block", "sdhci",
        "ext4", "squashfs", "overlay", "ubifs",
        "dm-crypt", "dm-mod", "dm-verity"
    };
    for (int i = 0; i < 15; i++) {
        initramfs_config_add_module(icfg, boot_modules[i]);
        printf("  [%2d] insmod %s/%s.ko\n", i + 1, icfg->module_dir, boot_modules[i]);
    }

    sub("Generated /init script");
    char init_script[2048];
    int slen = initramfs_config_generate_script(icfg, init_script, sizeof(init_script));
    printf("Script size: %d bytes\n", slen);

    boot_context_advance_stage(&ctx, BOOT_STAGE_INITRAMFS);
    printf("  Kernel executes /init in initramfs...\n");
    printf("  /init: mounting /proc, /sys, /dev...\n");
    printf("  /init: loading %d modules...\n", icfg->module_count);

    sub("Building CPIO archive");
    cpio_archive_t cpio;
    cpio_archive_init(&cpio);

    cpio_archive_add_dir(&cpio, "/", 0755);
    cpio_archive_add_dir(&cpio, "/bin", 0755);
    cpio_archive_add_dir(&cpio, "/sbin", 0755);
    cpio_archive_add_dir(&cpio, "/dev", 0755);
    cpio_archive_add_dir(&cpio, "/etc", 0755);
    cpio_archive_add_dir(&cpio, "/lib", 0755);
    cpio_archive_add_dir(&cpio, "/lib/modules", 0755);
    cpio_archive_add_dir(&cpio, "/proc", 0550);
    cpio_archive_add_dir(&cpio, "/sys", 0550);
    cpio_archive_add_dir(&cpio, "/mnt", 0755);
    cpio_archive_add_dir(&cpio, "/mnt/rootfs", 0755);
    cpio_archive_add_dir(&cpio, "/mnt/overlay", 0755);
    cpio_archive_add_dir(&cpio, "/tmp", 01777);
    cpio_archive_add_dir(&cpio, "/var", 0755);
    cpio_archive_add_dir(&cpio, "/var/log", 0755);

    cpio_archive_add_file(&cpio, "init", "/init",
                          (const uint8_t *)init_script,
                          (uint32_t)slen, 0755);

    const char *busybox_dummy = "BusyBox v1.36.1 multi-call binary";
    cpio_archive_add_file(&cpio, "busybox", "/bin/busybox",
                          (const uint8_t *)busybox_dummy,
                          (uint32_t)strlen(busybox_dummy), 0755);

    cpio_archive_add_symlink(&cpio, "/bin/sh", "busybox");
    cpio_archive_add_symlink(&cpio, "/sbin/init", "../bin/busybox");
    cpio_archive_add_symlink(&cpio, "/bin/mount", "busybox");
    cpio_archive_add_symlink(&cpio, "/bin/umount", "busybox");

    cpio_archive_dump(&cpio);
    cpio_archive_pack(&cpio, "/boot/initramfs.cpio.gz");

    /* ==================== Phase 3: Root Filesystems ==================== */
    section("Phase 3: Root Filesystem Types & Setup");

    sub("Mounting virtual filesystems");
    fs_mount_add(&ctx, "proc", "/proc", ROOTFS_TYPE_TMPFS, "rw,nosuid,nodev");
    fs_mount_add(&ctx, "sysfs", "/sys", ROOTFS_TYPE_TMPFS, "rw,nosuid,nodev");
    fs_mount_add(&ctx, "devtmpfs", "/dev", ROOTFS_TYPE_TMPFS, "rw,nosuid");
    fs_mount_add(&ctx, "devpts", "/dev/pts", ROOTFS_TYPE_TMPFS, "rw,nosuid,noexec,mode=620");
    fs_mount_add(&ctx, "tmpfs", "/tmp", ROOTFS_TYPE_TMPFS, "rw,nosuid,nodev");
    fs_mount_add(&ctx, "tmpfs", "/run", ROOTFS_TYPE_TMPFS, "rw,nosuid,nodev,mode=755");
    fs_mount_all(&ctx);

    sub("Root filesystem options");
    printf("\n"
           "  Type          | ReadOnly | Compression | Best For\n"
           "  ───────────────┼──────────┼─────────────┼────────────────────\n"
           "  initramfs     | No       | cpio.gz     | Small boot images\n"
           "  squashfs      | Yes      | xz/lzo/zstd | Read-only rootfs\n"
           "  ext4          | No       | None        | SD card / eMMC\n"
           "  UBIFS         | No       | None        | Raw NAND flash\n"
           "  overlayfs     | Layers   | N/A         | RW on RO root\n"
           "  NFS           | Network  | N/A         | Development\n"
           "\n");

    sub("Squashfs — read-only compressed rootfs");
    rootfs_create_squashfs("/work/rootfs", "/images/rootfs.squashfs", 6);
    printf("  Characteristics:\n");
    printf("  - Block size: 128KB, compression: xz\n");
    printf("  - Read-only, immutable root\n");
    printf("  - Ideal for: firmware, appliances, embedded\n");

    sub("UBIFS — raw flash filesystem");
    rootfs_create_ubifs("/work/rootfs", "/images/rootfs.ubi", 253952);
    printf("  Characteristics:\n");
    printf("  - LEB size: 248KB (hardware-dependent)\n");
    printf("  - Wear leveling built-in\n");
    printf("  - Ideal for: raw NAND flash on embedded boards\n");

    sub("OverlayFS — writable layer on read-only root");
    overlayfs_setup(&ctx,
        "/rom/rootfs.squashfs",       /* lower: read-only squashfs */
        "/data/overlay/upper",        /* upper: writable persistent */
        "/data/overlay/work",         /* work: overlay workdir */
        "/");                          /* merge: final merged view */
    overlayfs_merge(&ctx);
    printf("  Result: seamless RW filesystem from RO base + RW overlay\n");
    printf("  All writes go to /data/overlay/upper/\n");
    printf("  Original squashfs remains clean / factory-resettable\n");

    /* ==================== Phase 4: Switch Root ==================== */
    section("Phase 4: switch_root — Transition to Real Rootfs");

    boot_context_advance_stage(&ctx, BOOT_STAGE_SWITCH_ROOT);

    printf("  Sequence:\n");
    printf("    1. Mount new root on /mnt/rootfs\n");
    printf("    2. Move /proc, /sys, /dev mounts to /mnt/rootfs\n");
    printf("    3. pivot_root(.=mnt/rootfs, mnt/rootfs/initramfs)\n");
    printf("    4. chroot to new root\n");
    printf("    5. exec /sbin/init in new root\n\n");

    switch_root_perform(&ctx);

    boot_context_advance_stage(&ctx, BOOT_STAGE_ROOTFS);
    boot_context_advance_stage(&ctx, BOOT_STAGE_USERSPACE);

    /* ==================== Phase 5: Init System ==================== */
    section("Phase 5: Init System & Service Startup");

    busybox_ctx_t bb;
    busybox_init(&bb, "/sbin/init");

    sub("/etc/inittab processing");
    bb_inittab_load(&bb, "/etc/inittab");

    printf("  Processing inittab entries by action:\n\n");
    printf("  ::sysinit entries (run first, sequentially):\n");
    bb_inittab_execute(&bb, BB_INITTAB_SYSINIT);

    printf("\n  ::wait entries (run sequentially, wait for completion):\n");
    bb_inittab_execute(&bb, BB_INITTAB_WAIT);

    printf("\n  ::once entries (run once, don't wait):\n");
    bb_inittab_execute(&bb, BB_INITTAB_ONCE);

    printf("\n  ::respawn entries (restart if process dies):\n");
    bb_inittab_execute(&bb, BB_INITTAB_RESPAWN);

    printf("\n  ::ctrlaltdel entry (Ctrl-Alt-Del handler):\n");
    bb_inittab_execute(&bb, BB_INITTAB_CTRLALTDEL);

    sub("Busybox services");
    bb_syslogd_init();
    bb_klogd_init();
    bb_syslogd_log(6, "init", "Entering runlevel 3: multi-user");

    sub("Network initialization");
    bb_net_iface_add(&bb, "lo");
    bb_net_iface_config(&bb, "lo", "127.0.0.1", "255.0.0.0", NULL);
    bb_net_iface_up(&bb, "lo");

    bb_net_iface_add(&bb, "eth0");
    bb_net_iface_config(&bb, "eth0", "192.168.1.50", "255.255.255.0", "192.168.1.255");
    bb_net_iface_up(&bb, "eth0");

    bb_net_set_gateway(&bb, "192.168.1.1");
    bb_route_add(&bb, "default", "192.168.1.1", "0.0.0.0", "eth0", 100);

    bb_net_iface_add(&bb, "wlan0");
    bb_net_dhcp_start(&bb, "wlan0");
    bb_net_set_dns(&bb, "8.8.8.8");

    sub("Kernel module management at runtime");
    bb_module_load("industrialio");
    bb_module_load("bme280");
    bb_module_load("inv-mpu6050");

    char lsmod_buf[256];
    bb_module_list(lsmod_buf, sizeof(lsmod_buf));
    printf("%s", lsmod_buf);

    /* ==================== Phase 6: Device Management ==================== */
    section("Phase 6: Device Management (mdev / devtmpfs)");

    km_subsystem_t ks;
    km_subsystem_init(&ks);

    printf("  devtmpfs: kernel auto-creates /dev nodes\n");
    printf("  mdev -s : coldplug — creates nodes for existing devices\n");
    printf("  mdev    : hotplug — handles new device events\n\n");

    struct { const char *name; unsigned maj; unsigned min; } devices[] = {
        {"mem",       1, 1},  {"null",       1, 3},
        {"zero",      1, 5},  {"random",     1, 8},
        {"urandom",   1, 9},  {"console",    5, 1},
        {"tty",       5, 0},  {"ttyS0",      4, 64},
        {"ttyUSB0", 188, 0},  {"i2c-0",     89, 0},
        {"i2c-1",    89, 1},  {"spidev0.0",153, 0},
        {"gpiochip0",254, 0}, {"gpiochip1", 254, 1},
        {"mmcblk0",  179, 0}, {"mmcblk0p1", 179, 1},
        {"mmcblk0p2",179, 2}, {"watchdog",   10,130},
    };
    int ndev = (int)(sizeof(devices) / sizeof(devices[0]));
    for (int i = 0; i < ndev; i++) {
        km_device_create(&ks, devices[i].name, KM_DEVTYPE_CHAR,
                        devices[i].maj, devices[i].min, 0660,
                        NULL, NULL, NULL, NULL, NULL);
    }

    /* ==================== Phase 7: Complete Boot Visualization ==================== */
    section("Phase 7: Complete Boot Sequence Visualization");

    boot_context_t viz;
    boot_context_init(&viz);
    boot_context_set_cmdline(&viz,
        "console=ttyS0,115200 earlycon root=/dev/mmcblk0p2 rw "
        "quiet loglevel=4 systemd.show_status=yes");
    boot_context_set_root(&viz, "/dev/mmcblk0p2", ROOTFS_TYPE_OVERLAYFS);

    overlayfs_setup(&viz,
        "/rom/rootfs.squashfs",
        "/data/rw/upper", "/data/rw/work", "/");

    for (int stage = BOOT_STAGE_BOOTLOADER; stage <= BOOT_STAGE_RUNNING; stage++) {
        printf("  [%02d] ", stage);
        boot_context_advance_stage(&viz, (boot_stage_t)stage);
        switch (stage) {
            case BOOT_STAGE_BOOTLOADER:
                printf("    U-Boot 2024.01 (Jan 15 2024)\n");
                printf("    Loading kernel from mmc 0:2...\n");
                break;
            case BOOT_STAGE_KERNEL:
                printf("    [0.000000] Booting Linux on CPU0\n");
                printf("    [0.500000] Calibrating delay loop...\n");
                printf("    [1.200000] Mounting root...\n");
                break;
            case BOOT_STAGE_INITRAMFS:
                printf("    Running /init (initramfs)\n");
                printf("    Loading modules: usb, mmc, filesystem drivers\n");
                break;
            case BOOT_STAGE_SWITCH_ROOT:
                printf("    switch_root: pivot_root to /dev/mmcblk0p2\n");
                printf("    Executing /sbin/init...\n");
                break;
            case BOOT_STAGE_ROOTFS:
                printf("    Mounting overlayfs...\n");
                printf("    Running fsck...\n");
                break;
            case BOOT_STAGE_USERSPACE:
                printf("    Starting init system...\n");
                printf("    ::sysinit /etc/init.d/rcS\n");
                printf("    Starting services: syslogd, klogd, mdev\n");
                break;
            case BOOT_STAGE_RUNNING:
                printf("    [OK] Reached target Multi-User.\n");
                printf("    [OK] Started getty on ttyS0.\n");
                printf("    Login: _\n");
                break;
            default: break;
        }
    }

    /* ==================== Final Dump ==================== */
    section("Final System State");

    boot_context_dump(&viz);
    fs_mount_dump(&ctx);

    printf("\n┌──────────────────────────────────────────────────────────────┐\n");
    printf("│  Boot Summary                                                │\n");
    printf("├──────────────────────────────────────────────────────────────┤\n");
    printf("│  Bootloader → Kernel → initramfs → switch_root → Rootfs     │\n");
    printf("│  Rootfs type: OverlayFS (squashfs + writable overlay)       │\n");
    printf("│  Init: Busybox init (/sbin/init → /etc/inittab)             │\n");
    printf("│  Shell: Busybox ash/hush                                     │\n");
    printf("│  Services: syslogd, klogd, mdev, udhcpc, getty              │\n");
    printf("│  Boot time: ~2.5 seconds (simulated)                        │\n");
    printf("└──────────────────────────────────────────────────────────────┘\n\n");

    overlayfs_dissolve(&viz);
    fs_umount_all(&ctx);
    cpio_archive_free(&cpio);

    printf("Boot & rootfs demo complete.\n");
    return 0;
}
