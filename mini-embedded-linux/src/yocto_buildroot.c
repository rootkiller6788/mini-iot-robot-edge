#include "yocto_buildroot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *task_names[YOCTO_TASK_COUNT] = {
    "fetch", "unpack", "patch", "configure", "compile",
    "install", "package", "rootfs", "image"
};

static const char *image_fmt_names[IMAGE_FMT_COUNT] = {
    "tar", "ext4", "wic", "cpio", "squashfs", "ubi"
};

static const char *arch_names[ARCH_COUNT] = {
    "arm", "aarch64", "i386", "x86_64", "mips", "riscv"
};

static const char *libc_names[LIBC_COUNT] = {
    "glibc", "musl", "uclibc"
};

int yocto_project_init(yocto_project_t *proj, const char *build_dir)
{
    if (!proj || !build_dir) return -1;
    memset(proj, 0, sizeof(*proj));
    strncpy(proj->build_dir, build_dir, YOCTO_PATH_MAX - 1);
    snprintf(proj->deploy_dir, YOCTO_PATH_MAX, "%s/tmp/deploy", build_dir);
    proj->arch = ARCH_ARM64;
    proj->libc = LIBC_GLIBC;
    cross_toolchain_setup(&proj->toolchain, proj->arch, proj->libc);
    return 0;
}

int yocto_layer_add(yocto_project_t *proj, const char *name, const char *path, int priority)
{
    if (!proj || !name || !path) return -1;
    if (proj->layer_count >= YOCTO_MAX_LAYERS) return -2;
    yocto_layer_t *l = &proj->layers[proj->layer_count];
    strncpy(l->name, name, 63);
    strncpy(l->path, path, YOCTO_PATH_MAX - 1);
    l->priority = priority;
    l->enabled = 1;
    proj->layer_count++;
    return proj->layer_count - 1;
}

int yocto_layer_find(yocto_project_t *proj, const char *name)
{
    if (!proj || !name) return -1;
    for (int i = 0; i < proj->layer_count; i++) {
        if (strcmp(proj->layers[i].name, name) == 0) return i;
    }
    return -1;
}

int yocto_recipe_parse(yocto_recipe_t *recipe, const char *bb_file_path)
{
    if (!recipe || !bb_file_path) return -1;
    memset(recipe, 0, sizeof(*recipe));
    const char *basename = strrchr(bb_file_path, '/');
    basename = basename ? basename + 1 : bb_file_path;
    char tmp[256];
    strncpy(tmp, basename, 255);
    char *dot = strstr(tmp, ".bb");
    if (dot) *dot = '\0';
    char *us = strrchr(tmp, '_');
    if (us) {
        *us = '\0';
        strncpy(recipe->version, us + 1, 31);
    }
    strncpy(recipe->name, tmp, 127);
    strncpy(recipe->src_uri, bb_file_path, YOCTO_PATH_MAX - 1);
    return 0;
}

int yocto_recipe_add(yocto_project_t *proj, const yocto_recipe_t *recipe)
{
    if (!proj || !recipe) return -1;
    if (proj->recipe_count >= YOCTO_MAX_RECIPES) return -2;
    proj->recipes[proj->recipe_count] = *recipe;
    proj->recipe_count++;
    return proj->recipe_count - 1;
}

int yocto_recipe_find(yocto_project_t *proj, const char *name)
{
    if (!proj || !name) return -1;
    for (int i = 0; i < proj->recipe_count; i++) {
        if (strcmp(proj->recipes[i].name, name) == 0) return i;
    }
    return -1;
}

int bitbake_task_init(bitbake_exec_t *exec, yocto_recipe_t *recipe)
{
    if (!exec || !recipe) return -1;
    memset(exec, 0, sizeof(*exec));
    exec->recipe = *recipe;
    for (int i = 0; i < YOCTO_TASK_COUNT; i++) {
        exec->tasks[i].type = (yocto_task_type_t)i;
        exec->tasks[i].name = task_names[i];
        exec->tasks[i].completed = 0;
        exec->tasks[i].failed = 0;
    }
    exec->task_count = YOCTO_TASK_COUNT;
    return 0;
}

static int task_exec_stub(void *recipe)
{
    (void)recipe;
    return 0;
}

int bitbake_task_execute(bitbake_exec_t *exec, yocto_task_type_t type)
{
    if (!exec || type >= YOCTO_TASK_COUNT) return -1;
    yocto_task_t *t = &exec->tasks[type];
    if (t->completed) return 0;
    int ret;
    if (t->prefunc) {
        ret = t->prefunc(&exec->recipe);
        if (ret != 0) { t->failed = 1; return ret; }
    }
    if (t->exec) {
        ret = t->exec(&exec->recipe);
    } else {
        ret = task_exec_stub(&exec->recipe);
    }
    if (ret != 0) { t->failed = 1; return ret; }
    if (t->postfunc) {
        ret = t->postfunc(&exec->recipe);
        if (ret != 0) { t->failed = 1; return ret; }
    }
    t->completed = 1;
    return 0;
}

int bitbake_task_execute_all(bitbake_exec_t *exec)
{
    if (!exec) return -1;
    int total = 0;
    for (int i = 0; i < YOCTO_TASK_COUNT; i++) {
        int ret = bitbake_task_execute(exec, (yocto_task_type_t)i);
        if (ret != 0) return ret;
        total++;
    }
    return total;
}

int cross_toolchain_setup(cross_toolchain_t *tc, target_arch_t arch, target_libc_t libc)
{
    if (!tc) return -1;
    memset(tc, 0, sizeof(*tc));
    const char *arch_str = (arch < ARCH_COUNT) ? arch_names[arch] : "unknown";
    const char *libc_str = (libc < LIBC_COUNT) ? libc_names[libc] : "unknown";
    snprintf(tc->target_triplet, 64, "%s-linux-%s", arch_str, libc_str);
    snprintf(tc->cross_compile_prefix, 128, "%s-", tc->target_triplet);
    snprintf(tc->sysroot_path, YOCTO_PATH_MAX, "/opt/sysroots/%s", tc->target_triplet);
    snprintf(tc->staging_dir, YOCTO_PATH_MAX, "/opt/staging/%s", tc->target_triplet);
    return 0;
}

void cross_toolchain_print(const cross_toolchain_t *tc)
{
    if (!tc) return;
    printf("=== Cross-compilation Toolchain ===\n");
    printf("  Target triplet: %s\n", tc->target_triplet);
    printf("  Compile prefix: %s\n", tc->cross_compile_prefix);
    printf("  Sysroot       : %s\n", tc->sysroot_path);
    printf("  Staging dir   : %s\n", tc->staging_dir);
}

int cross_toolchain_verify(const cross_toolchain_t *tc)
{
    if (!tc) return 0;
    return (tc->target_triplet[0] != '\0' &&
            tc->cross_compile_prefix[0] != '\0' &&
            tc->sysroot_path[0] != '\0');
}

int buildroot_project_init(buildroot_project_t *proj, const char *output_dir)
{
    if (!proj || !output_dir) return -1;
    memset(proj, 0, sizeof(*proj));
    strncpy(proj->output_dir, output_dir, YOCTO_PATH_MAX - 1);
    snprintf(proj->target_dir, YOCTO_PATH_MAX, "%s/target", output_dir);
    proj->arch = ARCH_ARM;
    cross_toolchain_setup(&proj->toolchain, proj->arch, LIBC_UCLIBC);
    return 0;
}

int buildroot_config_load(buildroot_project_t *proj, const char *config_file)
{
    (void)config_file;
    if (!proj) return -1;
    proj->config_count = 0;
    static const char *default_names[] = {
        "BR2_arm", "BR2_cortex_a53", "BR2_TOOLCHAIN_BUILDROOT",
        "BR2_PACKAGE_BUSYBOX", "BR2_PACKAGE_OPENSSH"
    };
    for (int i = 0; i < 5 && i < BUILDROOT_MAX_CONFIGS; i++) {
        strncpy(proj->configs[i].name, default_names[i], 63);
        proj->configs[i].value = 1;
        proj->config_count++;
    }
    return proj->config_count;
}

int buildroot_config_get(buildroot_project_t *proj, const char *name, int *value)
{
    if (!proj || !name || !value) return -1;
    for (int i = 0; i < proj->config_count; i++) {
        if (strcmp(proj->configs[i].name, name) == 0) {
            *value = proj->configs[i].value;
            return 0;
        }
    }
    return -1;
}

int buildroot_package_add(buildroot_project_t *proj, const char *name, const char *version)
{
    if (!proj || !name) return -1;
    if (proj->package_count >= BUILDROOT_MAX_PACKAGES) return -2;
    buildroot_package_t *p = &proj->packages[proj->package_count];
    strncpy(p->name, name, 63);
    if (version) strncpy(p->version, version, 31);
    proj->package_count++;
    return proj->package_count - 1;
}

int buildroot_package_select(buildroot_project_t *proj, const char *name, int enable)
{
    if (!proj || !name) return -1;
    for (int i = 0; i < proj->package_count; i++) {
        if (strcmp(proj->packages[i].name, name) == 0) {
            return 0;
        }
    }
    if (enable) buildroot_package_add(proj, name, "1.0");
    return 0;
}

int image_generate(const image_config_t *cfg, const char *output_path)
{
    if (!cfg || !output_path) return -1;
    printf("[IMAGE] Generating %s image: %s\n", image_fmt_names[cfg->format], output_path);
    switch (cfg->format) {
        case IMAGE_FMT_TAR:     return image_create_tar(cfg, output_path);
        case IMAGE_FMT_EXT4:    return image_create_ext4(cfg, output_path);
        case IMAGE_FMT_WIC:     return image_create_wic(cfg, output_path);
        case IMAGE_FMT_CPIO:    return image_create_tar(cfg, output_path);
        case IMAGE_FMT_SQUASHFS: return image_create_tar(cfg, output_path);
        case IMAGE_FMT_UBI:     return image_create_tar(cfg, output_path);
        default: return -1;
    }
}

int image_create_tar(const image_config_t *cfg, const char *output_path)
{
    (void)cfg;
    printf("[TAR] Creating tar image: %s\n", output_path);
    return 0;
}

int image_create_ext4(const image_config_t *cfg, const char *output_path)
{
    (void)cfg;
    printf("[EXT4] Creating ext4 image: %s (%llu MB)\n", output_path,
           (unsigned long long)cfg->size_mb);
    return 0;
}

int image_create_wic(const image_config_t *cfg, const char *output_path)
{
    (void)cfg;
    printf("[WIC] Creating wic image: %s\n", output_path);
    return 0;
}

void yocto_project_dump(const yocto_project_t *proj)
{
    if (!proj) return;
    printf("========== Yocto Project ==========\n");
    printf("Build dir : %s\n", proj->build_dir);
    printf("Deploy dir: %s\n", proj->deploy_dir);
    printf("Arch      : %s\n", arch_names[proj->arch]);
    printf("Libc      : %s\n", libc_names[proj->libc]);
    cross_toolchain_print(&proj->toolchain);
    printf("Layers (%d):\n", proj->layer_count);
    for (int i = 0; i < proj->layer_count; i++) {
        printf("  [%d] %s (priority=%d) -> %s\n", i,
               proj->layers[i].name, proj->layers[i].priority, proj->layers[i].path);
    }
    printf("Recipes (%d):\n", proj->recipe_count);
    for (int i = 0; i < proj->recipe_count && i < 8; i++) {
        printf("  [%d] %s v%s\n", i, proj->recipes[i].name, proj->recipes[i].version);
    }
}

void buildroot_project_dump(const buildroot_project_t *proj)
{
    if (!proj) return;
    printf("========== Buildroot Project ==========\n");
    printf("Output dir: %s\n", proj->output_dir);
    printf("Target dir: %s\n", proj->target_dir);
    cross_toolchain_print(&proj->toolchain);
    printf("Configs (%d):\n", proj->config_count);
    for (int i = 0; i < proj->config_count; i++) {
        printf("  %s = %d\n", proj->configs[i].name, proj->configs[i].value);
    }
    printf("Packages (%d):\n", proj->package_count);
    for (int i = 0; i < proj->package_count && i < 10; i++) {
        printf("  [%d] %s v%s\n", i, proj->packages[i].name, proj->packages[i].version);
    }
}
