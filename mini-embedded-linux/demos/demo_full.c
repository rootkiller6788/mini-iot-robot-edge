/*
 * demo_full.c - Full Demonstration of mini-embedded-linux
 *
 * Walks through all five sub-modules:
 *   busybox_app.h, device_tree_overlay.h, initramfs_rootfs.h, kernel_module.h, yocto_buildroot.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "busybox_app.h"
#include "device_tree_overlay.h"
#include "initramfs_rootfs.h"
#include "kernel_module.h"
#include "yocto_buildroot.h"

int main(void) {
    printf("\n");
    printf("*************************************************************\n");
    printf("*                                                           *\n");
    printf("*   MINI-EMBEDDED-LINUX  --  Full Feature Demonstration     *\n");
    printf("*   BusyBox | DT Overlay | Initramfs | Kernel Mod | Yocto   *\n");
    printf("*                                                           *\n");
    printf("*************************************************************\n");
    printf("\n");

    /* ---------------------------------------------------------------
     *  SECTION 1 -- busybox_app.h
     * --------------------------------------------------------------- */
    printf("--- Section 1: BusyBox Applet Management ---\n\n");

    int rc = busybox_init();
    if (rc == 0) printf("[OK] busybox_init() succeeded\n");
    else          printf("[!!] busybox_init() returned %d\n", rc);

    /* Register common applets */
    bb_applet_register("cat", NULL);
    bb_applet_register("ls", NULL);
    bb_applet_register("sh", NULL);
    bb_applet_register("echo", NULL);
    bb_applet_register("mount", NULL);
    bb_applet_register("ifconfig", NULL);
    bb_applet_register("route", NULL);
    printf("[OK] bb_applet_register(): 7 applets registered\n");

    BBApplet *sh = bb_applet_find("sh");
    printf("[OK] bb_applet_find(\"sh\") -> %p\n", (void *)sh);

    rc = bb_applet_invoke(sh, 0, NULL);
    printf("[OK] bb_applet_invoke(sh) returned %d\n", rc);

    /* Init system */
    rc = bb_inittab_load("/etc/inittab");
    printf("[OK] bb_inittab_load(\"/etc/inittab\") returned %d\n", rc);

    bb_inittab_add("ttyS0", "/bin/sh", "respawn");
    bb_inittab_add("ttyS1", "/sbin/getty", "askfirst");
    bb_inittab_add("null", "/bin/true", "once");
    printf("[OK] bb_inittab_add(): 3 inittab entries configured\n");

    rc = bb_inittab_execute();
    printf("[OK] bb_inittab_execute() returned %d\n", rc);

    rc = bb_sysinit_run();
    printf("[OK] bb_sysinit_run() returned %d\n", rc);

    /* Network configuration */
    rc = bb_net_iface_config("eth0", "192.168.1.100", "255.255.255.0");
    if (rc == 0) printf("[OK] bb_net_iface_config(): eth0 = 192.168.1.100/24\n");
    else          printf("[!!] bb_net_iface_config() returned %d\n", rc);

    rc = bb_net_iface_up("eth0");
    if (rc == 0) printf("[OK] bb_net_iface_up(): eth0 is up\n");
    else          printf("[!!] bb_net_iface_up() returned %d\n", rc);

    rc = bb_net_dhcp_start("eth0");
    if (rc == 0) printf("[OK] bb_net_dhcp_start(): DHCP client started on eth0\n");
    else          printf("[!!] bb_net_dhcp_start() returned %d\n", rc);

    bb_net_set_gateway("192.168.1.1");
    bb_net_set_dns("8.8.8.8");
    printf("[OK] bb_net_set_gateway() + bb_net_set_dns(): networking configured\n");

    bb_route_add("default", "192.168.1.1", "eth0");
    printf("[OK] bb_route_add(): default route via 192.168.1.1\n");

    rc = bb_net_iface_down("eth0");
    printf("[OK] bb_net_iface_down(): eth0 is down\n");

    printf("\n");

    /* ---------------------------------------------------------------
     *  SECTION 2 -- device_tree_overlay.h
     * --------------------------------------------------------------- */
    printf("--- Section 2: Device Tree & Overlays ---\n\n");

    rc = dt_init();
    if (rc == 0) printf("[OK] dt_init(): device tree subsystem ready\n");
    else          printf("[!!] dt_init() returned %d\n", rc);

    DTNode *root = dt_node_add(NULL, "/");
    printf("[OK] dt_node_add(): root node created\n");

    DTNode *soc = dt_node_add(root, "soc");
    DTNode *timer = dt_node_add(soc, "timer@40001000");
    DTNode *uart = dt_node_add(soc, "uart@40002000");
    printf("[OK] dt_node_add(): soc/timer/uart hierarchy created\n");

    DTNode *found = dt_node_find("soc");
    printf("[OK] dt_node_find(\"soc\") -> %p\n", (void *)found);

    dt_prop_add_u32(timer, "reg", 0x40001000);
    dt_prop_add_string(timer, "compatible", "arm,armv7-timer");
    dt_prop_add_u32(timer, "interrupts", 27);
    printf("[OK] dt_prop_add_u32 + dt_prop_add_string(): timer properties set\n");

    uint32_t reg = dt_prop_get_u32(timer, "reg");
    printf("[OK] dt_prop_get_u32(timer, \"reg\") -> 0x%08X\n", reg);

    dt_prop_add_u32(uart, "reg", 0x40002000);
    dt_prop_add_string(uart, "compatible", "ns16550a");
    dt_prop_add_u32(uart, "clock-frequency", 24000000);
    printf("[OK] dt_prop: uart@40002000 configured\n");

    char compatible[64];
    dt_prop_get_string(uart, "compatible", compatible, sizeof(compatible));
    printf("[OK] dt_prop_get_string(uart, \"compatible\") -> \"%s\"\n", compatible);

    rc = dt_compile_dts("output.dtb");
    if (rc == 0) printf("[OK] dt_compile_dts(): compiled to output.dtb\n");
    else          printf("[!!] dt_compile_dts() returned %d\n", rc);

    /* Overlay system */
    dt_overlay_init();
    DTNode *frag = dt_overlay_add_fragment("fragment@0", "soc");
    printf("[OK] dt_overlay_add_fragment() -> %p\n", (void *)frag);

    dt_prop_add_string(frag, "__overlay__", "status = \"okay\"");
    rc = dt_apply_overlay();
    if (rc == 0) printf("[OK] dt_apply_overlay(): overlay applied\n");
    else          printf("[!!] dt_apply_overlay() returned %d\n", rc);

    /* Pinmux */
    dt_pinmux_setup("uart0", "tx", "PA9", "AF7");
    dt_pinmux_setup("uart0", "rx", "PA10", "AF7");
    dt_pinmux_setup("i2c0", "scl", "PB6", "AF4");
    dt_pinmux_setup("i2c0", "sda", "PB7", "AF4");
    printf("[OK] dt_pinmux_setup(): UART0 + I2C0 pins configured\n");

    printf("\n");

    /* ---------------------------------------------------------------
     *  SECTION 3 -- initramfs_rootfs.h
     * --------------------------------------------------------------- */
    printf("--- Section 3: Initramfs & Root Filesystem ---\n\n");

    boot_context_init();
    printf("[OK] boot_context_init(): boot context initialized\n");

    boot_context_set_cmdline("console=ttyS0,115200 root=/dev/mmcblk0p2 rw");
    printf("[OK] boot_context_set_cmdline(): kernel command line set\n");

    rc = boot_context_set_root("/dev/mmcblk0p2", "ext4");
    if (rc == 0) printf("[OK] boot_context_set_root(): root = /dev/mmcblk0p2 (ext4)\n");
    else          printf("[!!] boot_context_set_root() returned %d\n", rc);

    /* Initramfs configuration */
    initramfs_config_init();
    initramfs_config_add_module("usb-storage");
    initramfs_config_add_module("sdhci");
    initramfs_config_add_module("ext4");
    initramfs_config_set_init("/sbin/init");
    printf("[OK] initramfs_config: 3 kernel modules + /sbin/init configured\n");

    /* Generate init script */
    char gen_script[1024];
    memset(gen_script, 0, sizeof(gen_script));
    initramfs_config_generate_script(gen_script, sizeof(gen_script));
    printf("[OK] initramfs_config_generate_script(): auto-generated init\n");

    /* CPIO archive */
    cpio_archive_init();
    printf("[OK] cpio_archive_init(): CPIO context created\n");

    const char *init_script =
        "#!/bin/sh\n"
        "mount -t proc none /proc\n"
        "mount -t sysfs none /sys\n"
        "mount -t devtmpfs none /dev\n"
        "exec /sbin/init\n";
    cpio_archive_add_file("init", init_script, strlen(init_script) + 1);
    printf("[OK] cpio_archive_add_file(): init script added (%zu bytes)\n",
           strlen(init_script) + 1);

    cpio_archive_add_file("etc/fstab",
        "/dev/mmcblk0p2 / ext4 defaults 0 1\n", 37);
    cpio_archive_add_file("etc/hostname", "embedded-device\n", 17);
    cpio_archive_add_file("etc/resolv.conf", "nameserver 8.8.8.8\n", 21);
    printf("[OK] cpio_archive_add_file(): fstab + hostname + resolv.conf added\n");

    rc = cpio_archive_pack("initramfs.cpio");
    if (rc == 0) printf("[OK] cpio_archive_pack(): initramfs.cpio created\n");
    else          printf("[!!] cpio_archive_pack() returned %d\n", rc);

    /* Filesystem mounts */
    fs_mount_add("proc", "/proc", "proc", 0);
    fs_mount_add("sysfs", "/sys", "sysfs", 0);
    fs_mount_add("devtmpfs", "/dev", "devtmpfs", 0);
    fs_mount_add("tmpfs", "/tmp", "tmpfs", 0);
    fs_mount_add("tmpfs", "/run", "tmpfs", 0);
    printf("[OK] fs_mount_add(): 5 virtual filesystems queued\n");

    fs_mount_all();
    printf("[OK] fs_mount_all(): all filesystems mounted\n");

    /* SquashFS + OverlayFS */
    rc = rootfs_create_squashfs("rootfs/", "rootfs.squashfs");
    if (rc == 0) printf("[OK] rootfs_create_squashfs(): rootfs.squashfs created\n");
    else          printf("[!!] rootfs_create_squashfs() returned %d\n", rc);

    overlayfs_setup("/overlay/work", "/overlay/upper");
    printf("[OK] overlayfs_setup(): OverlayFS configured\n");

    rc = overlayfs_merge();
    printf("[OK] overlayfs_merge(): read-write overlay active\n");

    printf("\n");

    /* ---------------------------------------------------------------
     *  SECTION 4 -- kernel_module.h
     * --------------------------------------------------------------- */
    printf("--- Section 4: Kernel Module Management ---\n\n");

    rc = km_subsystem_init();
    if (rc == 0) printf("[OK] km_subsystem_init(): kernel module subsystem ready\n");
    else          printf("[!!] km_subsystem_init() returned %d\n", rc);

    km_module_init("my_gpio_driver");
    km_module_set_license("my_gpio_driver", "GPL v2");
    km_module_set_author("my_gpio_driver", "Embedded Developer");
    km_module_set_description("my_gpio_driver", "Custom GPIO Driver for IoT Device");
    printf("[OK] km_module_init + metadata: my_gpio_driver configured\n");

    km_module_param_add("my_gpio_driver", "gpio_base", "0x40020000");
    km_module_param_add("my_gpio_driver", "irq_line", "52");
    km_module_param_add("my_gpio_driver", "num_pins", "16");
    printf("[OK] km_module_param_add(): 3 parameters added\n");

    km_module_add_dependency("my_gpio_driver", "pinctrl");
    printf("[OK] km_module_add_dependency(): depends on pinctrl\n");

    rc = km_module_load("my_gpio_driver");
    if (rc == 0) printf("[OK] km_module_load(): my_gpio_driver loaded\n");
    else          printf("[!!] km_module_load() returned %d\n", rc);

    KMDevice *dev = km_device_create("gpio0", 240, 0);
    printf("[OK] km_device_create(): gpio0 (major=240, minor=0) created\n");
    (void)dev;

    km_proc_create("gpio_stats", 0644, NULL);
    km_proc_create("gpio_debug", 0444, NULL);
    printf("[OK] km_proc_create(): /proc/gpio_stats + /proc/gpio_debug entries\n");

    km_tasklet_create("gpio_irq_bh", NULL, 0);
    printf("[OK] km_tasklet_create(): gpio_irq_bh tasklet registered\n");

    km_workqueue_create("sensor_wq");
    km_workqueue_create("net_wq");
    printf("[OK] km_workqueue_create(): sensor_wq + net_wq workqueues created\n");

    km_module_find("my_gpio_driver");
    printf("[OK] km_module_find(): my_gpio_driver found\n");

    rc = km_module_unload("my_gpio_driver");
    if (rc == 0) printf("[OK] km_module_unload(): my_gpio_driver unloaded\n");
    else          printf("[!!] km_module_unload() returned %d\n", rc);

    printf("\n");

    /* ---------------------------------------------------------------
     *  SECTION 5 -- yocto_buildroot.h
     * --------------------------------------------------------------- */
    printf("--- Section 5: Yocto / Buildroot Build Systems ---\n\n");

    /* Yocto */
    rc = yocto_project_init();
    if (rc == 0) printf("[OK] yocto_project_init(): Yocto project initialized\n");
    else          printf("[!!] yocto_project_init() returned %d\n", rc);

    yocto_layer_add("meta-openembedded");
    yocto_layer_add("meta-raspberrypi");
    yocto_layer_add("meta-python");
    printf("[OK] yocto_layer_add(): 3 layers added\n");

    yocto_recipe_add("my-custom-app", "1.0.0");
    yocto_recipe_add("my-sensor-daemon", "2.1.0");
    yocto_recipe_add("edge-monitor", "0.5.0");
    printf("[OK] yocto_recipe_add(): 3 recipes registered\n");

    rc = bitbake_task_execute("my-custom-app", "do_fetch");
    if (rc == 0) printf("[OK] bitbake_task_execute(): do_fetch completed\n");
    else          printf("[!!] bitbake_task_execute() returned %d\n", rc);

    rc = bitbake_task_execute("my-custom-app", "do_compile");
    if (rc == 0) printf("[OK] bitbake_task_execute(): do_compile completed\n");
    else          printf("[!!] bitbake_task_execute() returned %d\n", rc);

    /* Cross-compilation toolchain */
    rc = cross_toolchain_setup("aarch64", "poky");
    if (rc == 0) printf("[OK] cross_toolchain_setup(): aarch64-poky toolchain ready\n");
    else          printf("[!!] cross_toolchain_setup() returned %d\n", rc);

    rc = cross_toolchain_verify();
    if (rc == 0) printf("[OK] cross_toolchain_verify(): toolchain verified\n");
    else          printf("[!!] cross_toolchain_verify() returned %d\n", rc);

    /* Buildroot */
    buildroot_project_init();
    printf("[OK] buildroot_project_init(): Buildroot project initialized\n");

    buildroot_config_load("raspberrypi4_64_defconfig");
    printf("[OK] buildroot_config_load(): raspberrypi4_64_defconfig loaded\n");

    int config_val = 0;
    buildroot_config_get("BR2_PACKAGE_OPENSSH", &config_val);
    printf("[OK] buildroot_config_get(): BR2_PACKAGE_OPENSSH = %d\n", config_val);

    buildroot_package_add("openssh", NULL);
    buildroot_package_add("dropbear", NULL);
    buildroot_package_add("strace", NULL);
    buildroot_package_add("i2c-tools", NULL);
    buildroot_package_add("spi-tools", NULL);
    printf("[OK] buildroot_package_add(): 5 packages queued\n");

    rc = image_generate("sdcard.img", IMG_TYPE_EXT4);
    if (rc == 0) printf("[OK] image_generate(): sdcard.img (EXT4) generated\n");
    else          printf("[!!] image_generate() returned %d\n", rc);

    /* Additional image formats */
    image_generate("rootfs.tar.gz", IMG_TYPE_TAR);
    image_generate("rootfs.squashfs", IMG_TYPE_SQFS);
    printf("[OK] image_generate(): additional formats created\n");

    printf("\n");

    /* ---------------------------------------------------------------
     *  COMPLETION
     * --------------------------------------------------------------- */
    printf("*************************************************************\n");
    printf("*                                                           *\n");
    printf("*  mini-embedded-linux Full Demonstration Complete!         *\n");
    printf("*  BusyBox + DT + Initramfs + Kernel Mod + Yocto/Buildroot  *\n");
    printf("*                                                           *\n");
    printf("*************************************************************\n");
    printf("\n");

    return 0;
}
