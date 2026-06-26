#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "yocto_buildroot.h"

static int my_fetch_func(void *recipe)
{
    yocto_recipe_t *r = (yocto_recipe_t *)recipe;
    printf("  [fetch] Downloading source for %s...\n", r->name);
    return 0;
}

static int my_compile_func(void *recipe)
{
    yocto_recipe_t *r = (yocto_recipe_t *)recipe;
    printf("  [compile] Cross-compiling %s...\n", r->name);
    return 0;
}

int main(void)
{
    printf("=== Yocto / Buildroot Example ===\n\n");

    yocto_project_t yp;
    yocto_project_init(&yp, "/home/user/yocto/poky/build");

    yocto_layer_add(&yp, "meta-openembedded", "/sources/meta-openembedded", 10);
    yocto_layer_add(&yp, "meta-raspberrypi", "/sources/meta-raspberrypi", 20);
    yocto_layer_add(&yp, "meta-custom", "/sources/meta-custom", 30);

    yocto_recipe_t recipe;
    yocto_recipe_parse(&recipe, "myapp_1.0.bb");
    recipe.depends_count = 0;
    strncpy(recipe.depends[recipe.depends_count++], "glibc", 63);
    strncpy(recipe.depends[recipe.depends_count++], "openssl", 63);
    yocto_recipe_add(&yp, &recipe);

    bitbake_exec_t exec;
    bitbake_task_init(&exec, &recipe);
    exec.tasks[YOCTO_TASK_FETCH].exec = my_fetch_func;
    exec.tasks[YOCTO_TASK_COMPILE].exec = my_compile_func;

    printf("Running bitbake tasks for %s:\n", recipe.name);
    bitbake_task_execute_all(&exec);
    printf("\n");

    cross_toolchain_print(&yp.toolchain);
    printf("Toolchain verified: %s\n", cross_toolchain_verify(&yp.toolchain) ? "YES" : "NO");
    printf("\n");

    image_config_t img_cfg = {0};
    img_cfg.format = IMAGE_FMT_EXT4;
    img_cfg.size_mb = 1024;
    strncpy(img_cfg.rootfs_dir, yp.deploy_dir, YOCTO_PATH_MAX - 1);
    image_generate(&img_cfg, "output/core-image-minimal.rootfs.ext4");
    printf("\n");

    buildroot_project_t bp;
    buildroot_project_init(&bp, "/home/user/buildroot/output");
    buildroot_config_load(&bp, "configs/raspberrypi3_defconfig");
    buildroot_package_add(&bp, "busybox", "1.36.1");
    buildroot_package_add(&bp, "openssh", "9.6p1");
    buildroot_package_add(&bp, "dropbear", "2022.83");
    buildroot_package_add(&bp, "wpa_supplicant", "2.10");
    buildroot_package_select(&bp, "busybox", 1);

    yocto_project_dump(&yp);
    printf("\n");
    buildroot_project_dump(&bp);

    return 0;
}
