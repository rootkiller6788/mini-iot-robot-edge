#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "yocto_buildroot.h"
#include "device_tree_overlay.h"
#include "kernel_module.h"
#include "initramfs_rootfs.h"
#include "busybox_app.h"

static char g_log_buffer[4096];
static int  g_log_len = 0;

static void demo_log(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(g_log_buffer + g_log_len, sizeof(g_log_buffer) - (size_t)g_log_len, fmt, args);
    va_end(args);
    if (n > 0) g_log_len += n;
}

static void demo_section(const char *title)
{
    printf("\n============================================================\n");
    printf("  %s\n", title);
    printf("============================================================\n");
}

static void demo_subsection(const char *title)
{
    printf("----- %s -----\n", title);
}

static int demo_ash_main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    printf("ash: built-in shell prompt $ ");
    return 0;
}

static int demo_init_main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    printf("init: starting system services...\n");
    return 0;
}

static int demo_ifconfig_main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    printf("eth0: flags=4163<UP,BROADCAST,RUNNING> mtu 1500\n");
    printf("      inet 192.168.1.100 netmask 255.255.255.0\n");
    return 0;
}

static int demo_mount_main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    printf("mount: filesystem mounted\n");
    return 0;
}

int main(void)
{
    demo_section("Mini Embedded Linux — Full System Demo");

    /* ================ Part 1: Yocto / Buildroot ================ */
    demo_section("Part 1: Build System (Yocto & Buildroot)");

    yocto_project_t yp;
    yocto_project_init(&yp, "/work/yocto/build");

    yocto_layer_add(&yp, "meta", "/work/yocto/poky/meta", 5);
    yocto_layer_add(&yp, "meta-poky", "/work/yocto/poky/meta-poky", 5);
    yocto_layer_add(&yp, "meta-yocto-bsp", "/work/yocto/poky/meta-yocto-bsp", 5);
    yocto_layer_add(&yp, "meta-oe", "/work/yocto/meta-openembedded/meta-oe", 10);
    yocto_layer_add(&yp, "meta-python", "/work/yocto/meta-openembedded/meta-python", 10);
    yocto_layer_add(&yp, "meta-networking", "/work/yocto/meta-openembedded/meta-networking", 10);
    yocto_layer_add(&yp, "meta-raspberrypi", "/work/yocto/meta-raspberrypi", 20);
    yocto_layer_add(&yp, "meta-iot", "/work/yocto/meta-iot", 30);

    const char *recipe_names[] = {
        "linux-raspberrypi", "u-boot", "busybox", "openssh", "wpa-supplicant",
        "bluez5", "alsa-utils", "gstreamer", "python3", "nodejs"
    };
    for (int i = 0; i < 10; i++) {
        yocto_recipe_t r;
        char bb_name[64];
        snprintf(bb_name, sizeof(bb_name), "%s_1.0.bb", recipe_names[i]);
        yocto_recipe_parse(&r, bb_name);
        yocto_recipe_add(&yp, &r);
    }

    yocto_project_dump(&yp);

    demo_subsection("Cross-compilation toolchain");
    cross_toolchain_t tc;
    cross_toolchain_setup(&tc, ARCH_ARM64, LIBC_MUSL);
    cross_toolchain_print(&tc);

    demo_subsection("Image Generation");
    const char *image_types[] = {"tar", "ext4", "wic"};
    image_format_t formats[] = {IMAGE_FMT_TAR, IMAGE_FMT_EXT4, IMAGE_FMT_WIC};
    for (int i = 0; i < 3; i++) {
        char out_path[256];
        snprintf(out_path, sizeof(out_path), "deploy/core-image-minimal.rootfs.%s", image_types[i]);
        image_config_t cfg = {formats[i], "", "", 512, 0};
        strncpy(cfg.rootfs_dir, yp.deploy_dir, YOCTO_PATH_MAX - 1);
        strncpy(cfg.output_name, out_path, 255);
        image_generate(&cfg, out_path);
    }

    demo_subsection("Buildroot");
    buildroot_project_t bp;
    buildroot_project_init(&bp, "/work/buildroot/output");
    buildroot_config_load(&bp, "configs/raspberrypi3_defconfig");

    const char *br_packages[] = {
        "busybox", "openssh", "dropbear", "wpa_supplicant", "hostapd",
        "bluez-utils", "alsa-lib", "gstreamer1", "ffmpeg", "opencv3",
        "python3", "mosquitto", "libgpiod", "can-utils", "i2c-tools"
    };
    for (int i = 0; i < 15; i++) {
        buildroot_package_add(&bp, br_packages[i], "latest");
    }
    buildroot_project_dump(&bp);

    /* ================ Part 2: Device Tree ================ */
    demo_section("Part 2: Device Tree & Overlay System");

    device_tree_t dt;
    dt_init(&dt);

    int soc = dt_node_add(&dt, dt.root_index, "soc");
    dt_prop_add_string(&dt, soc, "compatible", "simple-bus");
    dt_prop_add_u32(&dt, soc, "#address-cells", 2);
    dt_prop_add_u32(&dt, soc, "#size-cells", 1);
    dt_prop_add_string(&dt, soc, "ranges", "");

    const char *periph[] = {
        "gpio@7e200000", "uart@7e201000", "spi@7e204000",
        "i2c@7e205000", "i2c@7e804000", "pwm@7e20c000",
        "dma@7e007000", "mmc@7e300000", "usb@7e980000",
        "hdmi@7e902000", "dpi@7e208000", "vchiq@7e00b840",
        "thermal@7e212000", "watchdog@7e100000", "rng@7e104000"
    };
    const char *compat[] = {
        "brcm,bcm2835-gpio", "brcm,bcm2835-pl011", "brcm,bcm2835-spi",
        "brcm,bcm2835-i2c", "brcm,bcm2835-i2c", "brcm,bcm2835-pwm",
        "brcm,bcm2835-dma", "brcm,bcm2835-mmc", "brcm,bcm2835-usb",
        "brcm,bcm2835-hdmi", "brcm,bcm2835-dpi", "brcm,bcm2835-vchiq",
        "brcm,bcm2835-thermal", "brcm,bcm2835-pm", "brcm,bcm2835-rng"
    };
    for (int i = 0; i < 15; i++) {
        int n = dt_node_add(&dt, soc, periph[i]);
        dt_prop_add_string(&dt, n, "compatible", compat[i]);
        if (i < 3) dt_phandle_assign(&dt, n);
    }

    printf("\nDevice tree has %d nodes, %d phandles\n", dt.node_count, dt.phandle_count);

    demo_subsection("Device Tree Overlay (HAT EEPROM)");
    dt_overlay_t hat_ov;
    dt_overlay_init(&hat_ov);
    dt_overlay_add_fragment(&hat_ov, "__overlay__");
    dt_overlay_add_node(&hat_ov, 0, "hat");
    dt_overlay_add_node(&hat_ov, 0, "hat_eeprom");

    dt_overlay_t sensor_ov;
    dt_overlay_init(&sensor_ov);
    dt_overlay_add_fragment(&sensor_ov, "__overlay__");
    dt_overlay_add_node(&sensor_ov, 0, "bme280@76");
    dt_overlay_add_node(&sensor_ov, 0, "imu@68");
    dt_overlay_add_node(&sensor_ov, 0, "light@29");

    int merged = dt_apply_overlays(&dt, (const dt_overlay_t[]){hat_ov, sensor_ov}, 2);
    printf("Applied %d overlay nodes\n", merged);

    demo_subsection("GPIO Pinmux Configuration");
    dt_pinmux_table_t pm;
    dt_pinmux_setup(&pm, "raspberrypi,3-model-b");

    for (int chip = 0; chip < 3; chip++) {
        for (int pin = 0; pin < 8; pin++) {
            dt_pinmux_add(&pm, chip, pin,
                          (uint32_t)((pin % 4) + 1),
                          (uint32_t)(pin % 3),
                          (uint32_t)((pin % 2) + 1));
        }
    }
    dt_pinmux_dump(&pm);
    dt_pinmux_apply_to_dt(&dt, &pm);

    /* ================ Part 3: Kernel Modules ================ */
    demo_section("Part 3: Kernel Module Subsystem");

    km_subsystem_t ks;
    km_subsystem_init(&ks);

    km_module_t mods[8];
    const char *mod_names[] = {
        "usb_common", "usbcore", "xhci_hcd", "usb_storage",
        "sdhci", "mmc_block", "bcm2835_mmc", "brcmfmac"
    };
    const char *mod_vers[] = {
        "1.0", "3.0", "1.2", "1.0", "2.0", "1.5", "1.0", "7.45"
    };
    km_license_t mod_lics[] = {
        KM_LICENSE_GPL, KM_LICENSE_GPL_V2, KM_LICENSE_GPL, KM_LICENSE_GPL_V2,
        KM_LICENSE_GPL, KM_LICENSE_GPL, KM_LICENSE_GPL, KM_LICENSE_DUAL_BSD_GPL
    };

    for (int i = 0; i < 8; i++) {
        km_module_init(&mods[i], mod_names[i], mod_vers[i]);
        km_module_set_license(&mods[i], mod_lics[i]);
        if (i > 0) km_module_add_dependency(&mods[i], mod_names[i - 1]);
    }

    for (int i = 0; i < 8; i++) {
        int ret = km_module_load(&ks, &mods[i]);
        if (ret != 0) printf("Failed to load %s\n", mod_names[i]);
    }

    km_module_list(&ks);

    demo_subsection("/sys/module visibility");
    for (int i = 0; i < ks.module_count && i < 5; i++) {
        printf("  /sys/module/%s/   parameters/\n", ks.modules[i].name);
        printf("  /sys/module/%s/   refcnt=%d\n", ks.modules[i].name, ks.modules[i].ref_count);
    }

    demo_subsection("Proc filesystem");
    km_proc_create(&ks, "modules", "", 0444, NULL,
                   (int(*)(char*,int,void*))NULL, NULL);
    km_proc_create(&ks, "version", "", 0444, NULL,
                   (int(*)(char*,int,void*))NULL, NULL);
    km_proc_create(&ks, "devices", "", 0444, NULL,
                   (int(*)(char*,int,void*))NULL, NULL);

    demo_subsection("Device nodes");
    struct { const char *name; unsigned maj; unsigned min; } devs[] = {
        {"ttyS0", 204, 64}, {"ttyAMA0", 204, 66}, {"mmcblk0", 179, 0},
        {"mmcblk0p1", 179, 1}, {"mmcblk0p2", 179, 2}, {"fb0", 29, 0},
        {"i2c-1", 89, 1}, {"spidev0.0", 153, 0}, {"gpiochip0", 254, 0}
    };
    for (int i = 0; i < 9; i++) {
        km_device_create(&ks, devs[i].name, KM_DEVTYPE_CHAR,
                         devs[i].maj, devs[i].min, 0660, NULL, NULL, NULL, NULL, NULL);
    }

    demo_subsection("Tasklet & Workqueue");
    for (int i = 0; i < 4; i++) {
        km_tasklet_create(&ks, (void(*)(unsigned long))NULL, (unsigned long)(i * 100));
    }
    for (int i = 0; i < 4; i++) {
        km_workqueue_create(&ks, (void(*)(void*))NULL, (void*)(uintptr_t)i);
    }
    km_tasklet_schedule(&ks, 0);
    km_workqueue_queue(&ks, 0);

    km_subsystem_dump(&ks);

    /* ================ Part 4: Boot & Rootfs ================ */
    demo_section("Part 4: Boot Sequence & Root Filesystem");

    boot_context_t bctx;
    boot_context_init(&bctx);
    boot_context_set_cmdline(&bctx,
        "console=ttyAMA0,115200 root=/dev/mmcblk0p2 rootfstype=ext4 rw rootwait "
        "coherent_pool=1M 8250.nr_uarts=1 cma=64M smsc95xx.macaddr=B8:27:EB:12:34:56");
    boot_context_set_root(&bctx, "/dev/mmcblk0p2", ROOTFS_TYPE_EXT4);

    demo_subsection("Initramfs early userspace");
    initramfs_config_init(&bctx.initramfs_cfg, "/initramfs");
    const char *early_mods[] = {"usb-common", "usbcore", "sdhci", "mmc-block",
                                "ext4", "squashfs", "overlay"};
    for (int i = 0; i < 7; i++) {
        initramfs_config_add_module(&bctx.initramfs_cfg, early_mods[i]);
    }
    char init_script[2048];
    initramfs_config_generate_script(&bctx.initramfs_cfg, init_script, sizeof(init_script));
    printf("Generated init script (%d bytes):\n%s\n", (int)strlen(init_script), init_script);

    demo_subsection("CPIO archive");
    cpio_archive_t arch;
    cpio_archive_init(&arch);
    cpio_archive_add_dir(&arch, "/", 0755);
    cpio_archive_add_dir(&arch, "/bin", 0755);
    cpio_archive_add_dir(&arch, "/dev", 0755);
    cpio_archive_add_dir(&arch, "/etc", 0755);
    cpio_archive_add_dir(&arch, "/lib", 0755);
    cpio_archive_add_dir(&arch, "/proc", 0555);
    cpio_archive_add_dir(&arch, "/sys", 0555);
    cpio_archive_add_dir(&arch, "/mnt", 0755);
    cpio_archive_add_dir(&arch, "/mnt/rootfs", 0755);
    cpio_archive_add_dir(&arch, "/tmp", 01777);
    cpio_archive_add_file(&arch, "init", "/init",
                          (const uint8_t *)init_script,
                          (uint32_t)strlen(init_script), 0755);
    cpio_archive_add_symlink(&arch, "/bin/sh", "busybox");
    cpio_archive_list(&arch);

    demo_subsection("Root filesystem types");
    printf("  initramfs: cpio archive in kernel memory (ramfs)\n");
    printf("  squashfs : read-only compressed filesystem (flash)\n");
    printf("  ubifs   : UBI filesystem for raw NAND flash\n");
    printf("  ext4    : journaling filesystem (eMMC/SD card)\n");
    printf("  overlayfs: writable overlay on read-only root\n\n");

    demo_subsection("OverlayFS setup");
    overlayfs_setup(&bctx, "/rom/root.squashfs", "/data/overlay/upper",
                    "/data/overlay/work", "/");
    overlayfs_merge(&bctx);

    demo_subsection("Full boot sequence simulation");
    boot_sequence_simulate(&bctx);
    switch_root_perform(&bctx);

    boot_context_dump(&bctx);

    /* ================ Part 5: Busybox ================ */
    demo_section("Part 5: Busybox — The Swiss Army Knife");

    busybox_ctx_t bb;
    busybox_init(&bb, "/bin/busybox");

    struct {
        bb_applet_id_t id;
        const char *name;
        bb_applet_main_t func;
        const char *usage;
    } applets_to_register[] = {
        {BB_APPLET_INIT,    "init",    demo_init_main,    "init [options]"},
        {BB_APPLET_ASH,     "ash",     demo_ash_main,     "ash [script]"},
        {BB_APPLET_HUSH,    "hush",    demo_ash_main,     "hush [script]"},
        {BB_APPLET_SH,      "sh",      demo_ash_main,     "sh [script]"},
        {BB_APPLET_MOUNT,   "mount",   demo_mount_main,   "mount [-t fstype] dev dir"},
        {BB_APPLET_UMOUNT,  "umount",  demo_mount_main,   "umount dir|dev"},
        {BB_APPLET_IFCONFIG,"ifconfig",demo_ifconfig_main,"ifconfig [iface] [addr]"},
        {BB_APPLET_ROUTE,   "route",   demo_mount_main,   "route add/del"},
        {BB_APPLET_UDHCPC,  "udhcpc",  demo_mount_main,   "udhcpc [-i iface]"},
        {BB_APPLET_MDEV,    "mdev",    demo_mount_main,   "mdev [-s]"},
        {BB_APPLET_SYSLOGD, "syslogd", demo_mount_main,   "syslogd [options]"},
        {BB_APPLET_GETTY,   "getty",   demo_mount_main,   "getty [baud] tty"},
        {BB_APPLET_REBOOT,  "reboot",  demo_mount_main,   "reboot [-f]"},
        {BB_APPLET_POWEROFF,"poweroff",demo_mount_main,   "poweroff [-f]"},
        {BB_APPLET_INSMOD,  "insmod",  demo_mount_main,   "insmod module.ko"},
        {BB_APPLET_RMMOD,   "rmmod",   demo_mount_main,   "rmmod module"},
        {BB_APPLET_LSMOD,   "lsmod",   demo_mount_main,   "lsmod"},
        {BB_APPLET_DMESG,   "dmesg",   demo_mount_main,   "dmesg"},
        {BB_APPLET_PING,    "ping",    demo_mount_main,   "ping host"},
        {BB_APPLET_HTTPD,   "httpd",   demo_mount_main,   "httpd [-p port]"},
    };

    for (int i = 0; i < 20; i++) {
        bb_applet_register(&bb, applets_to_register[i].id,
                           applets_to_register[i].name,
                           applets_to_register[i].func,
                           applets_to_register[i].usage);
    }

    printf("Registered %d busybox applets\n", bb.applet_count);

    demo_subsection("Symlink installation");
    bb_applet_install_symlinks(&bb, "/bin");

    demo_subsection("/etc/inittab configuration");
    bb_inittab_load(&bb, "/etc/inittab");
    bb_inittab_dump(&bb);

    demo_subsection("System initialization (::sysinit)");
    bb_sysinit_run(&bb);
    bb_inittab_execute(&bb, BB_INITTAB_SYSINIT);
    bb_inittab_execute(&bb, BB_INITTAB_RESPAWN);

    demo_subsection("Networking");
    bb_net_iface_add(&bb, "lo");
    bb_net_iface_config(&bb, "lo", "127.0.0.1", "255.0.0.0", NULL);
    bb_net_iface_up(&bb, "lo");

    bb_net_iface_add(&bb, "eth0");
    bb_net_iface_config(&bb, "eth0", "192.168.1.100", "255.255.255.0", "192.168.1.255");
    bb_net_iface_up(&bb, "eth0");
    bb_net_set_gateway(&bb, "192.168.1.1");
    bb_net_set_dns(&bb, "8.8.8.8");

    bb_net_iface_add(&bb, "wlan0");
    bb_net_dhcp_start(&bb, "wlan0");

    bb_route_add(&bb, "default", "192.168.1.1", "0.0.0.0", "eth0", 0);
    bb_route_add(&bb, "192.168.1.0", "0.0.0.0", "255.255.255.0", "eth0", 0);
    bb_route_add(&bb, "10.0.0.0", "192.168.1.254", "255.0.0.0", "eth0", 10);

    demo_subsection("Service daemons");
    bb_syslogd_init();
    bb_klogd_init();
    bb_syslogd_log(3, "demo", "System initialized successfully");

    busybox_ctx_dump(&bb);

    /* ================ Summary ================ */
    demo_section("System Summary");
    printf("\n"
           "  Embedded Linux stack fully simulated:\n"
           "  ┌─────────────────────────────────────┐\n"
           "  │  1. Yocto/Buildroot (Build)         │\n"
           "  │  2. Device Tree (Hardware desc)     │\n"
           "  │  3. Kernel Modules (Drivers)        │\n"
           "  │  4. Initramfs/Rootfs (Boot)         │\n"
           "  │  5. Busybox (Userspace)             │\n"
           "  └─────────────────────────────────────┘\n"
           "\n");

    cpio_archive_free(&arch);

    printf("All demos complete.\n");
    return 0;
}
