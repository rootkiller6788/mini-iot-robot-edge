/*
 * test_core.c - Core Unit Tests for mini-embedded-linux
 *
 * Tests all five sub-modules:
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

/* ---- test harness ---- */
static int tests_run = 0, tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  TEST %s ... ", name); } while(0)
#define PASS()     do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg)  do { printf("FAIL: %s\n", msg); return 1; } while(0)
#define CHECK(cond, msg) if (!(cond)) FAIL(msg)

/* ================================================================
 *  busybox_app.h
 * ================================================================ */

static int test_busybox_init(void) {
    TEST("busybox_init");
    int rc = busybox_init();
    CHECK(rc == 0, "busybox_init failed");
    PASS();
    return 0;
}

static int test_bb_applet_register_find(void) {
    TEST("bb_applet_register + bb_applet_find");
    busybox_init();
    bb_applet_register("echo", NULL);
    BBApplet *a = bb_applet_find("echo");
    CHECK(a != NULL, "bb_applet_find returned NULL for registered applet");
    PASS();
    return 0;
}

static int test_bb_applet_invoke(void) {
    TEST("bb_applet_invoke");
    busybox_init();
    bb_applet_register("true", NULL);
    BBApplet *a = bb_applet_find("true");
    int rc = bb_applet_invoke(a, 0, NULL);
    CHECK(rc >= 0, "bb_applet_invoke failed");
    PASS();
    return 0;
}

static int test_bb_inittab(void) {
    TEST("bb_inittab_load + bb_inittab_add + bb_inittab_execute");
    busybox_init();
    bb_inittab_load("/etc/inittab");
    bb_inittab_add("ttyS0", "/bin/sh", "respawn");
    bb_inittab_add("ttyS1", "/sbin/getty", "askfirst");
    int rc = bb_inittab_execute();
    CHECK(rc == 0, "bb_inittab_execute failed");
    PASS();
    return 0;
}

static int test_bb_sysinit(void) {
    TEST("bb_sysinit_run");
    busybox_init();
    bb_inittab_load("/etc/inittab");
    int rc = bb_sysinit_run();
    CHECK(rc == 0, "bb_sysinit_run failed");
    PASS();
    return 0;
}

/* ================================================================
 *  device_tree_overlay.h
 * ================================================================ */

static int test_dt_init(void) {
    TEST("dt_init");
    int rc = dt_init();
    CHECK(rc == 0, "dt_init failed");
    PASS();
    return 0;
}

static int test_dt_node_add_find(void) {
    TEST("dt_node_add + dt_node_find");
    dt_init();
    DTNode *soc = dt_node_add(NULL, "soc");
    CHECK(soc != NULL, "dt_node_add(soc) returned NULL");
    DTNode *timer = dt_node_add(soc, "timer@40001000");
    CHECK(timer != NULL, "dt_node_add(timer) returned NULL");
    DTNode *found = dt_node_find("timer@40001000");
    CHECK(found != NULL, "dt_node_find returned NULL for existing node");
    PASS();
    return 0;
}

static int test_dt_prop_add_get_u32(void) {
    TEST("dt_prop_add_u32 + dt_prop_get_u32");
    dt_init();
    DTNode *node = dt_node_add(NULL, "timer");
    dt_prop_add_u32(node, "reg", 0x40001000);
    uint32_t val = dt_prop_get_u32(node, "reg");
    CHECK(val == 0x40001000, "dt_prop_get_u32 returned wrong value");
    dt_prop_add_u32(node, "interrupts", 27);
    val = dt_prop_get_u32(node, "interrupts");
    CHECK(val == 27, "dt_prop_get_u32 returned wrong value for interrupts");
    PASS();
    return 0;
}

static int test_dt_overlay(void) {
    TEST("dt_overlay_init + fragment + apply");
    dt_init();
    DTNode *root = dt_node_add(NULL, "/");
    DTNode *soc = dt_node_add(root, "soc");
    (void)soc;
    dt_overlay_init();
    DTNode *frag = dt_overlay_add_fragment("fragment@0", "soc");
    CHECK(frag != NULL, "dt_overlay_add_fragment returned NULL");
    int rc = dt_apply_overlay();
    CHECK(rc == 0, "dt_apply_overlay failed");
    PASS();
    return 0;
}

static int test_dt_compile_write(void) {
    TEST("dt_compile_dts + dt_write_dtb");
    dt_init();
    DTNode *node = dt_node_add(NULL, "timer");
    dt_prop_add_u32(node, "reg", 0x40001000);
    dt_prop_add_string(node, "compatible", "arm,armv7-timer");
    int rc = dt_compile_dts("output.dtb");
    CHECK(rc == 0, "dt_compile_dts failed");
    PASS();
    return 0;
}

/* ================================================================
 *  initramfs_rootfs.h
 * ================================================================ */

static int test_boot_context_init(void) {
    TEST("boot_context_init + boot_context_set_root");
    boot_context_init();
    int rc = boot_context_set_root("/dev/mmcblk0p2", "ext4");
    CHECK(rc == 0, "boot_context_set_root failed");
    PASS();
    return 0;
}

static int test_cpio_archive(void) {
    TEST("cpio_archive_init + add_file + pack");
    cpio_archive_init();
    const char *init_data = "#!/bin/sh\necho hello";
    cpio_archive_add_file("init", init_data, strlen(init_data) + 1);
    const char *fstab_data = "/dev/sda1 / ext4 defaults 0 1\n";
    cpio_archive_add_file("etc/fstab", fstab_data, strlen(fstab_data) + 1);
    int rc = cpio_archive_pack("/tmp/test_initramfs.cpio");
    CHECK(rc == 0, "cpio_archive_pack failed");
    PASS();
    return 0;
}

static int test_initramfs_config(void) {
    TEST("initramfs_config_init + add_module + set_init");
    initramfs_config_init();
    initramfs_config_add_module("usb-storage");
    initramfs_config_add_module("sdhci");
    initramfs_config_add_module("ext4");
    initramfs_config_set_init("/sbin/init");
    PASS();
    return 0;
}

/* ================================================================
 *  kernel_module.h
 * ================================================================ */

static int test_km_subsystem_init(void) {
    TEST("km_subsystem_init");
    int rc = km_subsystem_init();
    CHECK(rc == 0, "km_subsystem_init failed");
    PASS();
    return 0;
}

static int test_km_module_lifecycle(void) {
    TEST("km_module_init + param + load + unload");
    km_subsystem_init();
    km_module_init("testdrv");
    km_module_param_add("testdrv", "debug", "1");
    km_module_param_add("testdrv", "bufsize", "4096");
    int rc = km_module_load("testdrv");
    CHECK(rc == 0, "km_module_load failed");
    rc = km_module_unload("testdrv");
    CHECK(rc == 0, "km_module_unload failed");
    PASS();
    return 0;
}

/* ================================================================
 *  yocto_buildroot.h
 * ================================================================ */

static int test_yocto_project_init(void) {
    TEST("yocto_project_init + layer_add");
    int rc = yocto_project_init();
    CHECK(rc == 0, "yocto_project_init failed");
    yocto_layer_add("meta-openembedded");
    yocto_layer_add("meta-raspberrypi");
    PASS();
    return 0;
}

static int test_cross_toolchain_setup(void) {
    TEST("cross_toolchain_setup");
    int rc = cross_toolchain_setup("arm", "poky");
    CHECK(rc == 0, "cross_toolchain_setup failed");
    PASS();
    return 0;
}

static int test_buildroot_project(void) {
    TEST("buildroot_project_init + config + package");
    buildroot_project_init();
    buildroot_config_load("raspberrypi4_defconfig");
    int rc = buildroot_package_add("openssh", NULL);
    CHECK(rc == 0, "buildroot_package_add(openssh) failed");
    rc = buildroot_package_add("dropbear", NULL);
    CHECK(rc == 0, "buildroot_package_add(dropbear) failed");
    PASS();
    return 0;
}

/* ================================================================
 *  main
 * ================================================================ */

int main(void) {
    printf("mini-embedded-linux  --  Core Unit Tests\n\n");

    /* busybox_app.h */
    test_busybox_init();
    test_bb_applet_register_find();
    test_bb_applet_invoke();
    test_bb_inittab();
    test_bb_sysinit();

    /* device_tree_overlay.h */
    test_dt_init();
    test_dt_node_add_find();
    test_dt_prop_add_get_u32();
    test_dt_overlay();
    test_dt_compile_write();

    /* initramfs_rootfs.h */
    test_boot_context_init();
    test_cpio_archive();
    test_initramfs_config();

    /* kernel_module.h */
    test_km_subsystem_init();
    test_km_module_lifecycle();

    /* yocto_buildroot.h */
    test_yocto_project_init();
    test_cross_toolchain_setup();
    test_buildroot_project();

    printf("\n%d / %d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
