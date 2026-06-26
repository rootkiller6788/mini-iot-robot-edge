#ifndef INITRAMFS_ROOTFS_H
#define INITRAMFS_ROOTFS_H

#include <stdint.h>
#include <stddef.h>

#define IR_PATH_MAX            512
#define IR_NAME_MAX            128
#define IR_MAX_FILES           256
#define IR_MAX_MODULES         32
#define IR_MAX_MOUNTS          16
#define IR_CMDLINE_MAX         1024
#define IR_INIT_MAX            256

#define IR_MAGIC_NEWC          0x070701
#define IR_MAGIC_CRC           0x070702
#define IR_MAGIC_END           "TRAILER!!!"

#define CPIO_MODE_DIR          0040000
#define CPIO_MODE_REG          0100000
#define CPIO_MODE_SYMLINK      0120000
#define CPIO_MODE_BLK          0060000
#define CPIO_MODE_CHR          0020000
#define CPIO_MODE_FIFO         0010000
#define CPIO_MODE_SOCK         0140000

typedef enum {
    BOOT_STAGE_BOOTLOADER,
    BOOT_STAGE_KERNEL,
    BOOT_STAGE_INITRAMFS,
    BOOT_STAGE_SWITCH_ROOT,
    BOOT_STAGE_ROOTFS,
    BOOT_STAGE_USERSPACE,
    BOOT_STAGE_RUNNING,
    BOOT_STAGE_COUNT
} boot_stage_t;

typedef enum {
    ROOTFS_TYPE_INITRAMFS,
    ROOTFS_TYPE_SQUASHFS,
    ROOTFS_TYPE_UBIFS,
    ROOTFS_TYPE_EXT4,
    ROOTFS_TYPE_NFS,
    ROOTFS_TYPE_OVERLAYFS,
    ROOTFS_TYPE_TMPFS,
    ROOTFS_TYPE_COUNT
} rootfs_type_t;

typedef enum {
    FS_MOUNT_BIND,
    FS_MOUNT_MOVE,
    FS_MOUNT_OVERLAY,
    FS_MOUNT_COUNT
} fs_mount_op_t;

typedef struct {
    uint32_t c_magic;
    uint32_t c_ino;
    uint32_t c_mode;
    uint32_t c_uid;
    uint32_t c_gid;
    uint32_t c_nlink;
    uint32_t c_mtime;
    uint32_t c_filesize;
    uint32_t c_dev_major;
    uint32_t c_dev_minor;
    uint32_t c_rdev_major;
    uint32_t c_rdev_minor;
    uint32_t c_namesize;
    uint32_t c_chksum;
} cpio_newc_header_t;

typedef struct {
    char   name[IR_NAME_MAX];
    char   path[IR_PATH_MAX];
    uint8_t *data;
    uint32_t data_size;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint8_t  is_dir;
    uint8_t  is_symlink;
    char     symlink_target[IR_PATH_MAX];
} cpio_file_t;

typedef struct {
    cpio_file_t files[IR_MAX_FILES];
    int         file_count;
    uint32_t    total_size;
} cpio_archive_t;

typedef struct {
    char root_dir[IR_PATH_MAX];
    char module_dir[IR_PATH_MAX];
    char modules_to_load[IR_MAX_MODULES][IR_NAME_MAX];
    int  module_count;
    char init_script[IR_INIT_MAX];
} initramfs_config_t;

typedef struct {
    char             name[IR_NAME_MAX];
    char             device[IR_PATH_MAX];
    char             mount_point[IR_PATH_MAX];
    rootfs_type_t    fs_type;
    char             mount_options[256];
    uint8_t          mounted;
} fs_mount_t;

typedef struct {
    char             lower_dir[IR_PATH_MAX];
    char             upper_dir[IR_PATH_MAX];
    char             work_dir[IR_PATH_MAX];
    char             merge_dir[IR_PATH_MAX];
    uint8_t          active;
} overlayfs_config_t;

typedef struct {
    boot_stage_t        current_stage;
    initramfs_config_t  initramfs_cfg;
    cpio_archive_t      cpio_archive;
    fs_mount_t          mounts[IR_MAX_MOUNTS];
    int                 mount_count;
    char                kernel_cmdline[IR_CMDLINE_MAX];
    char                root_device[IR_PATH_MAX];
    rootfs_type_t       rootfs_type;
    overlayfs_config_t  overlayfs;
    char                switch_root_target[IR_PATH_MAX];
    uint8_t             boot_completed;
} boot_context_t;

int   boot_context_init(boot_context_t *ctx);
int   boot_context_set_root(boot_context_t *ctx, const char *device, rootfs_type_t type);
int   boot_context_set_cmdline(boot_context_t *ctx, const char *cmdline);
void  boot_context_advance_stage(boot_context_t *ctx, boot_stage_t stage);

int   initramfs_config_init(initramfs_config_t *cfg, const char *root_dir);
int   initramfs_config_add_module(initramfs_config_t *cfg, const char *module_name);
int   initramfs_config_set_init(initramfs_config_t *cfg, const char *init_script);
int   initramfs_config_generate_script(const initramfs_config_t *cfg, char *out, int maxlen);

int   cpio_archive_init(cpio_archive_t *arch);
int   cpio_archive_add_file(cpio_archive_t *arch, const char *name, const char *path,
                             const uint8_t *data, uint32_t size, uint32_t mode);
int   cpio_archive_add_dir(cpio_archive_t *arch, const char *name, uint32_t mode);
int   cpio_archive_add_symlink(cpio_archive_t *arch, const char *name, const char *target);
int   cpio_archive_remove(cpio_archive_t *arch, const char *name);
int   cpio_archive_pack(const cpio_archive_t *arch, const char *output_path);
int   cpio_archive_unpack(const char *input_path, cpio_archive_t *arch);
int   cpio_archive_list(const cpio_archive_t *arch);
void  cpio_archive_free(cpio_archive_t *arch);

int   fs_mount_add(boot_context_t *ctx, const char *device, const char *mount_point,
                    rootfs_type_t type, const char *options);
int   fs_mount_all(boot_context_t *ctx);
int   fs_mount_single(boot_context_t *ctx, int idx);
int   fs_umount_all(boot_context_t *ctx);

int   rootfs_create_squashfs(const char *source_dir, const char *output_path, int compression);
int   rootfs_create_ubifs(const char *source_dir, const char *output_path, int leb_size);
int   rootfs_create_initramfs(const char *source_dir, const char *output_path);

int   overlayfs_setup(boot_context_t *ctx, const char *lower, const char *upper,
                       const char *work, const char *merge);
int   overlayfs_merge(boot_context_t *ctx);
int   overlayfs_dissolve(boot_context_t *ctx);

int   boot_sequence_simulate(boot_context_t *ctx);
int   switch_root_perform(boot_context_t *ctx);

void  boot_context_dump(const boot_context_t *ctx);
void  cpio_archive_dump(const cpio_archive_t *arch);
void  fs_mount_dump(const boot_context_t *ctx);

#endif
