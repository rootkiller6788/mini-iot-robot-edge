#include "initramfs_rootfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *boot_stage_names[BOOT_STAGE_COUNT] = {
    "Bootloader", "Kernel", "initramfs", "switch_root", "Rootfs", "Userspace", "Running"
};

static const char *rootfs_type_names[ROOTFS_TYPE_COUNT] = {
    "initramfs", "squashfs", "ubifs", "ext4", "nfs", "overlayfs", "tmpfs"
};

int boot_context_init(boot_context_t *ctx)
{
    if (!ctx) return -1;
    memset(ctx, 0, sizeof(*ctx));
    ctx->current_stage = BOOT_STAGE_BOOTLOADER;
    initramfs_config_init(&ctx->initramfs_cfg, "/initramfs");
    strncpy(ctx->switch_root_target, "/mnt/rootfs", IR_PATH_MAX - 1);
    ctx->boot_completed = 0;
    return 0;
}

int boot_context_set_root(boot_context_t *ctx, const char *device, rootfs_type_t type)
{
    if (!ctx || !device) return -1;
    strncpy(ctx->root_device, device, IR_PATH_MAX - 1);
    ctx->rootfs_type = type;
    if (type == ROOTFS_TYPE_OVERLAYFS) {
        strncpy(ctx->overlayfs.lower_dir, device, IR_PATH_MAX - 1);
    }
    return 0;
}

int boot_context_set_cmdline(boot_context_t *ctx, const char *cmdline)
{
    if (!ctx || !cmdline) return -1;
    strncpy(ctx->kernel_cmdline, cmdline, IR_CMDLINE_MAX - 1);
    return 0;
}

void boot_context_advance_stage(boot_context_t *ctx, boot_stage_t stage)
{
    if (!ctx) return;
    ctx->current_stage = stage;
    printf("[BOOT] Stage: %s\n", boot_stage_names[stage]);
}

int initramfs_config_init(initramfs_config_t *cfg, const char *root_dir)
{
    if (!cfg) return -1;
    memset(cfg, 0, sizeof(*cfg));
    if (root_dir) strncpy(cfg->root_dir, root_dir, IR_PATH_MAX - 1);
    else strncpy(cfg->root_dir, "/", IR_PATH_MAX - 1);
    strncpy(cfg->module_dir, "/lib/modules", IR_PATH_MAX - 1);
    strncpy(cfg->init_script, "/init", IR_INIT_MAX - 1);
    cfg->module_count = 0;
    initramfs_config_add_module(cfg, "usb-common");
    initramfs_config_add_module(cfg, "usb-storage");
    initramfs_config_add_module(cfg, "sdhci");
    initramfs_config_add_module(cfg, "mmc-block");
    return 0;
}

int initramfs_config_add_module(initramfs_config_t *cfg, const char *module_name)
{
    if (!cfg || !module_name || cfg->module_count >= IR_MAX_MODULES) return -1;
    strncpy(cfg->modules_to_load[cfg->module_count], module_name, IR_NAME_MAX - 1);
    cfg->module_count++;
    return 0;
}

int initramfs_config_set_init(initramfs_config_t *cfg, const char *init_script)
{
    if (!cfg || !init_script) return -1;
    strncpy(cfg->init_script, init_script, IR_INIT_MAX - 1);
    return 0;
}

int initramfs_config_generate_script(const initramfs_config_t *cfg, char *out, int maxlen)
{
    if (!cfg || !out) return -1;
    int pos = 0;
    pos += snprintf(out + pos, (size_t)(maxlen - pos), "#!/bin/sh\n");
    pos += snprintf(out + pos, (size_t)(maxlen - pos), "# initramfs init script\n");
    pos += snprintf(out + pos, (size_t)(maxlen - pos), "mount -t proc proc /proc\n");
    pos += snprintf(out + pos, (size_t)(maxlen - pos), "mount -t sysfs sysfs /sys\n");
    pos += snprintf(out + pos, (size_t)(maxlen - pos), "mount -t devtmpfs devtmpfs /dev\n");
    for (int i = 0; i < cfg->module_count; i++) {
        pos += snprintf(out + pos, (size_t)(maxlen - pos), "insmod %s/%s.ko\n",
                        cfg->module_dir, cfg->modules_to_load[i]);
    }
    pos += snprintf(out + pos, (size_t)(maxlen - pos),
                    "mount /dev/mmcblk0p2 /mnt/rootfs\n");
    pos += snprintf(out + pos, (size_t)(maxlen - pos), "switch_root /mnt/rootfs /sbin/init\n");
    return pos;
}

int cpio_archive_init(cpio_archive_t *arch)
{
    if (!arch) return -1;
    memset(arch, 0, sizeof(*arch));
    return 0;
}

int cpio_archive_add_file(cpio_archive_t *arch, const char *name, const char *path,
                           const uint8_t *data, uint32_t size, uint32_t mode)
{
    if (!arch || !name || arch->file_count >= IR_MAX_FILES) return -1;
    cpio_file_t *f = &arch->files[arch->file_count];
    memset(f, 0, sizeof(*f));
    strncpy(f->name, name, IR_NAME_MAX - 1);
    if (path) strncpy(f->path, path, IR_PATH_MAX - 1);
    f->data = data ? (uint8_t *)data : NULL;
    f->data_size = size;
    f->mode = mode ? mode : CPIO_MODE_REG;
    f->uid = 0;
    f->gid = 0;
    f->is_dir = 0;
    f->is_symlink = 0;
    arch->total_size += size;
    arch->file_count++;
    return arch->file_count - 1;
}

int cpio_archive_add_dir(cpio_archive_t *arch, const char *name, uint32_t mode)
{
    if (!arch || !name || arch->file_count >= IR_MAX_FILES) return -1;
    cpio_file_t *f = &arch->files[arch->file_count];
    memset(f, 0, sizeof(*f));
    strncpy(f->name, name, IR_NAME_MAX - 1);
    f->mode = mode ? mode : CPIO_MODE_DIR;
    f->is_dir = 1;
    f->data = NULL;
    f->data_size = 0;
    arch->file_count++;
    return arch->file_count - 1;
}

int cpio_archive_add_symlink(cpio_archive_t *arch, const char *name, const char *target)
{
    if (!arch || !name || !target || arch->file_count >= IR_MAX_FILES) return -1;
    cpio_file_t *f = &arch->files[arch->file_count];
    memset(f, 0, sizeof(*f));
    strncpy(f->name, name, IR_NAME_MAX - 1);
    f->mode = CPIO_MODE_SYMLINK;
    f->is_symlink = 1;
    strncpy(f->symlink_target, target, IR_PATH_MAX - 1);
    f->data_size = (uint32_t)strlen(target);
    arch->file_count++;
    return arch->file_count - 1;
}

int cpio_archive_remove(cpio_archive_t *arch, const char *name)
{
    if (!arch || !name) return -1;
    for (int i = 0; i < arch->file_count; i++) {
        if (strcmp(arch->files[i].name, name) == 0) {
            arch->total_size -= arch->files[i].data_size;
            for (int j = i; j < arch->file_count - 1; j++) {
                arch->files[j] = arch->files[j + 1];
            }
            arch->file_count--;
            return 0;
        }
    }
    return -1;
}

int cpio_archive_pack(const cpio_archive_t *arch, const char *output_path)
{
    if (!arch || !output_path) return -1;
    printf("[CPIO] Packing archive (%d files, %u bytes) -> %s\n",
           arch->file_count, arch->total_size, output_path);
    return 0;
}

int cpio_archive_unpack(const char *input_path, cpio_archive_t *arch)
{
    if (!input_path || !arch) return -1;
    printf("[CPIO] Unpacking archive from %s\n", input_path);
    return 0;
}

int cpio_archive_list(const cpio_archive_t *arch)
{
    if (!arch) return -1;
    printf("cpio archive: %d files, %u bytes\n", arch->file_count, arch->total_size);
    for (int i = 0; i < arch->file_count; i++) {
        char type = arch->files[i].is_dir ? 'd' : (arch->files[i].is_symlink ? 'l' : '-');
        printf("%c %s (%u bytes)\n", type, arch->files[i].name, arch->files[i].data_size);
    }
    return 0;
}

void cpio_archive_free(cpio_archive_t *arch)
{
    if (!arch) return;
    arch->file_count = 0;
    arch->total_size = 0;
}

int fs_mount_add(boot_context_t *ctx, const char *device, const char *mount_point,
                  rootfs_type_t type, const char *options)
{
    if (!ctx || !device || !mount_point || ctx->mount_count >= IR_MAX_MOUNTS) return -1;
    fs_mount_t *m = &ctx->mounts[ctx->mount_count];
    memset(m, 0, sizeof(*m));
    strncpy(m->device, device, IR_PATH_MAX - 1);
    strncpy(m->mount_point, mount_point, IR_PATH_MAX - 1);
    m->fs_type = type;
    if (options) strncpy(m->mount_options, options, 255);
    m->mounted = 0;
    ctx->mount_count++;
    return ctx->mount_count - 1;
}

int fs_mount_all(boot_context_t *ctx)
{
    if (!ctx) return -1;
    int count = 0;
    for (int i = 0; i < ctx->mount_count; i++) {
        if (fs_mount_single(ctx, i) == 0) count++;
    }
    return count;
}

int fs_mount_single(boot_context_t *ctx, int idx)
{
    if (!ctx || idx < 0 || idx >= ctx->mount_count) return -1;
    fs_mount_t *m = &ctx->mounts[idx];
    printf("[MOUNT] %s -> %s (type: %s)\n", m->device, m->mount_point,
           rootfs_type_names[m->fs_type]);
    m->mounted = 1;
    return 0;
}

int fs_umount_all(boot_context_t *ctx)
{
    if (!ctx) return -1;
    for (int i = ctx->mount_count - 1; i >= 0; i--) {
        printf("[UMOUNT] %s\n", ctx->mounts[i].mount_point);
        ctx->mounts[i].mounted = 0;
    }
    ctx->mount_count = 0;
    return 0;
}

int rootfs_create_squashfs(const char *source_dir, const char *output_path, int compression)
{
    if (!source_dir || !output_path) return -1;
    printf("[SQUASHFS] Creating (compression=%d): %s -> %s\n", compression, source_dir, output_path);
    return 0;
}

int rootfs_create_ubifs(const char *source_dir, const char *output_path, int leb_size)
{
    if (!source_dir || !output_path) return -1;
    printf("[UBIFS] Creating (LEB=%d): %s -> %s\n", leb_size, source_dir, output_path);
    return 0;
}

int rootfs_create_initramfs(const char *source_dir, const char *output_path)
{
    if (!source_dir || !output_path) return -1;
    printf("[INITRAMFS] Creating: %s -> %s\n", source_dir, output_path);
    return 0;
}

int overlayfs_setup(boot_context_t *ctx, const char *lower, const char *upper,
                     const char *work, const char *merge)
{
    if (!ctx || !lower || !upper || !work || !merge) return -1;
    overlayfs_config_t *ov = &ctx->overlayfs;
    strncpy(ov->lower_dir, lower, IR_PATH_MAX - 1);
    strncpy(ov->upper_dir, upper, IR_PATH_MAX - 1);
    strncpy(ov->work_dir, work, IR_PATH_MAX - 1);
    strncpy(ov->merge_dir, merge, IR_PATH_MAX - 1);
    ov->active = 0;
    printf("[OVERLAYFS] lower=%s, upper=%s, work=%s, merge=%s\n", lower, upper, work, merge);
    return 0;
}

int overlayfs_merge(boot_context_t *ctx)
{
    if (!ctx) return -1;
    overlayfs_config_t *ov = &ctx->overlayfs;
    if (!ov->lower_dir[0] || !ov->upper_dir[0]) return -2;
    ov->active = 1;
    printf("[OVERLAYFS] Merging: %s + %s -> %s\n", ov->lower_dir, ov->upper_dir, ov->merge_dir);
    return 0;
}

int overlayfs_dissolve(boot_context_t *ctx)
{
    if (!ctx) return -1;
    ctx->overlayfs.active = 0;
    printf("[OVERLAYFS] Dissolved\n");
    return 0;
}

int boot_sequence_simulate(boot_context_t *ctx)
{
    if (!ctx) return -1;

    boot_context_advance_stage(ctx, BOOT_STAGE_BOOTLOADER);
    printf("  U-Boot SPL -> ATF -> U-Boot -> Loading kernel...\n");

    boot_context_advance_stage(ctx, BOOT_STAGE_KERNEL);
    printf("  Kernel decompressing, initializing subsystems...\n");
    printf("  cmdline: %s\n", ctx->kernel_cmdline);

    boot_context_advance_stage(ctx, BOOT_STAGE_INITRAMFS);
    printf("  Running /init in initramfs...\n");
    printf("  Loading %d kernel modules...\n", ctx->initramfs_cfg.module_count);

    boot_context_advance_stage(ctx, BOOT_STAGE_SWITCH_ROOT);
    printf("  switch_root to %s\n", ctx->switch_root_target);

    boot_context_advance_stage(ctx, BOOT_STAGE_ROOTFS);
    printf("  Mounting root filesystem (%s)\n", rootfs_type_names[ctx->rootfs_type]);

    boot_context_advance_stage(ctx, BOOT_STAGE_USERSPACE);
    printf("  Starting /sbin/init...\n");

    boot_context_advance_stage(ctx, BOOT_STAGE_RUNNING);
    ctx->boot_completed = 1;
    printf("  System ready!\n");
    return 0;
}

int switch_root_perform(boot_context_t *ctx)
{
    if (!ctx) return -1;
    printf("[SWITCH_ROOT] Moving mounts from initramfs to %s\n", ctx->switch_root_target);
    printf("[SWITCH_ROOT] pivot_root + chroot to new root\n");
    printf("[SWITCH_ROOT] Executing /sbin/init in new rootfs\n");
    return 0;
}

void boot_context_dump(const boot_context_t *ctx)
{
    if (!ctx) return;
    printf("========== Boot Context ==========\n");
    printf("Stage        : %s\n", boot_stage_names[ctx->current_stage]);
    printf("Kernel cmdline: %s\n", ctx->kernel_cmdline);
    printf("Root device  : %s\n", ctx->root_device);
    printf("Rootfs type  : %s\n", rootfs_type_names[ctx->rootfs_type]);
    printf("Switch target: %s\n", ctx->switch_root_target);
    printf("Boot done    : %s\n", ctx->boot_completed ? "yes" : "no");
    printf("Mounts       : %d\n", ctx->mount_count);
    fs_mount_dump(ctx);
}

void cpio_archive_dump(const cpio_archive_t *arch)
{
    if (!arch) return;
    printf("========== CPIO Archive ==========\n");
    cpio_archive_list(arch);
}

void fs_mount_dump(const boot_context_t *ctx)
{
    if (!ctx) return;
    printf("Mount table:\n");
    for (int i = 0; i < ctx->mount_count; i++) {
        const fs_mount_t *m = &ctx->mounts[i];
        printf("  %s on %s type %s (%s) [%s]\n",
               m->device, m->mount_point, rootfs_type_names[m->fs_type],
               m->mount_options, m->mounted ? "mounted" : "pending");
    }
}
