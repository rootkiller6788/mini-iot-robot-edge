/*
 * bench_core.c - Core Benchmarks for mini-embedded-linux
 *
 * Measures performance of the major API functions across all five sub-modules:
 *   busybox_app.h, device_tree_overlay.h, initramfs_rootfs.h, kernel_module.h, yocto_buildroot.h
 *
 * Usage: bench_core [N]
 *   N = iteration scale factor (default 5000)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#include "busybox_app.h"
#include "device_tree_overlay.h"
#include "initramfs_rootfs.h"
#include "kernel_module.h"
#include "yocto_buildroot.h"

/* ---- helper: high-resolution timer ---- */
static double now_ms(void) {
    return (double)clock() * 1000.0 / (double)CLOCKS_PER_SEC;
}

/* ---- benchmark runner ---- */
static void bench_run(const char *name, void (*fn)(int), int n) {
    double t0 = now_ms();
    fn(n);
    double t1 = now_ms();
    double elapsed = t1 - t0;
    printf("  %-42s %d ops in %9.1f ms  (%8.1f us/op)\n",
           name, n, elapsed, (elapsed * 1000.0) / (double)n);
}

/* ================================================================
 *  BENCHMARKS -- busybox_app.h
 * ================================================================ */

static void bm_busybox_applet_register_find(int n) {
    busybox_init();
    for (int i = 0; i < 10; i++) {
        bb_applet_register("test_applet", NULL);
    }
    for (int i = 0; i < n; i++) {
        BBApplet *a = bb_applet_find("test_applet");
        (void)a;
    }
}

static void bm_busybox_applet_invoke(int n) {
    busybox_init();
    bb_applet_register("sh", NULL);
    BBApplet *a = bb_applet_find("sh");
    for (int i = 0; i < n; i++) {
        bb_applet_invoke(a, 0, NULL);
    }
}

static void bm_busybox_net_ops(int n) {
    busybox_init();
    int scaled = n / 5;
    for (int i = 0; i < scaled; i++) {
        bb_net_iface_config("eth0", "192.168.1.100", "255.255.255.0");
        bb_net_iface_up("eth0");
        bb_net_iface_down("eth0");
    }
}

/* ================================================================
 *  BENCHMARKS -- device_tree_overlay.h
 * ================================================================ */

static void bm_dt_node_add_find(int n) {
    dt_init();
    int scaled = n / 5;
    for (int i = 0; i < scaled; i++) {
        DTNode *node = dt_node_add(NULL, "test_node");
        dt_node_find("test_node");
        (void)node;
    }
}

static void bm_dt_prop_ops(int n) {
    dt_init();
    DTNode *node = dt_node_add(NULL, "props");
    for (int i = 0; i < n; i++) {
        dt_prop_add_u32(node, "reg", (uint32_t)i);
        uint32_t val = dt_prop_get_u32(node, "reg");
        (void)val;
    }
}

static void bm_dt_overlay_apply(int n) {
    dt_init();
    DTNode *root = dt_node_add(NULL, "/");
    DTNode *soc = dt_node_add(root, "soc");
    (void)soc;
    dt_overlay_init();
    dt_overlay_add_fragment("fragment@0", "soc");
    int scaled = n / 20;
    for (int i = 0; i < scaled; i++) {
        dt_apply_overlay();
    }
}

/* ================================================================
 *  BENCHMARKS -- initramfs_rootfs.h
 * ================================================================ */

static void bm_boot_context_init(int n) {
    for (int i = 0; i < n; i++) {
        boot_context_init();
    }
}

static void bm_cpio_archive_pack(int n) {
    cpio_archive_init();
    int scaled = n / 20;
    char content[4096];
    memset(content, 0x00, sizeof(content));
    snprintf(content, sizeof(content), "#!/bin/sh\necho boot");
    cpio_archive_add_file("init", content, strlen(content) + 1);
    for (int i = 0; i < scaled; i++) {
        cpio_archive_pack("/tmp/bench_initramfs.cpio");
    }
}

static void bm_fs_mount_add_all(int n) {
    boot_context_init();
    int scaled = n / 10;
    for (int i = 0; i < scaled; i++) {
        fs_mount_add("proc", "/proc", "proc", 0);
        fs_mount_add("sysfs", "/sys", "sysfs", 0);
        fs_mount_add("devtmpfs", "/dev", "devtmpfs", 0);
        fs_mount_all();
    }
}

/* ================================================================
 *  BENCHMARKS -- kernel_module.h
 * ================================================================ */

static void bm_km_module_load_unload(int n) {
    km_subsystem_init();
    km_module_init("bench_driver");
    int scaled = n / 20;
    for (int i = 0; i < scaled; i++) {
        km_module_load("bench_driver");
        km_module_unload("bench_driver");
    }
}

static void bm_km_proc_device_ops(int n) {
    km_subsystem_init();
    km_module_init("devdrv");
    km_module_load("devdrv");
    int scaled = n / 5;
    for (int i = 0; i < scaled; i++) {
        km_proc_create("bench_stat", 0644, NULL);
        km_device_create("bench_dev", 240, (unsigned int)i);
    }
}

/* ================================================================
 *  BENCHMARKS -- yocto_buildroot.h
 * ================================================================ */

static void bm_yocto_layer_recipe(int n) {
    yocto_project_init();
    int scaled = n / 10;
    for (int i = 0; i < scaled; i++) {
        yocto_layer_add("meta-layer");
        yocto_recipe_add("recipe", "1.0");
    }
}

static void bm_bitbake_task_exec(int n) {
    yocto_project_init();
    yocto_layer_add("meta-bench");
    yocto_recipe_add("bench-recipe", "1.0");
    int scaled = n / 20;
    for (int i = 0; i < scaled; i++) {
        bitbake_task_execute("bench-recipe", "do_fetch");
    }
}

static void bm_buildroot_package(int n) {
    buildroot_project_init();
    buildroot_config_load("bench_defconfig");
    int scaled = n / 10;
    for (int i = 0; i < scaled; i++) {
        buildroot_package_add("pkg", NULL);
    }
}

/* ================================================================
 *  main
 * ================================================================ */

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 5000;
    if (N < 1) N = 5000;

    printf("mini-embedded-linux  --  Core Benchmarks  (N=%d)\n\n", N);

    bench_run("bb_applet_find",                       bm_busybox_applet_register_find, N);
    bench_run("bb_applet_invoke",                     bm_busybox_applet_invoke, N);
    bench_run("bb_net_iface_config + up + down",      bm_busybox_net_ops, N);
    bench_run("dt_node_add + dt_node_find",           bm_dt_node_add_find, N);
    bench_run("dt_prop_add_u32 + dt_prop_get_u32",    bm_dt_prop_ops, N);
    bench_run("dt_overlay_add_fragment + dt_apply",   bm_dt_overlay_apply, N);
    bench_run("boot_context_init",                    bm_boot_context_init, N);
    bench_run("cpio_archive_pack",                    bm_cpio_archive_pack, N);
    bench_run("fs_mount_add + fs_mount_all",          bm_fs_mount_add_all, N);
    bench_run("km_module_load + unload (roundtrip)",  bm_km_module_load_unload, N);
    bench_run("km_proc_create + km_device_create",    bm_km_proc_device_ops, N);
    bench_run("yocto_layer_add + yocto_recipe_add",   bm_yocto_layer_recipe, N);
    bench_run("bitbake_task_execute",                 bm_bitbake_task_exec, N);
    bench_run("buildroot_package_add",                bm_buildroot_package, N);

    printf("\nDone.\n");
    return 0;
}
