#ifndef YOCTO_BUILDROOT_H
#define YOCTO_BUILDROOT_H

#include <stdint.h>
#include <stddef.h>

#define YOCTO_MAX_LAYERS        32
#define YOCTO_MAX_RECIPES       128
#define YOCTO_MAX_TASKS         16
#define YOCTO_MAX_DEPENDS       32
#define YOCTO_PATH_MAX          512
#define BUILDROOT_MAX_PACKAGES  256
#define BUILDROOT_MAX_CONFIGS   64

typedef enum {
    YOCTO_TASK_FETCH,
    YOCTO_TASK_UNPACK,
    YOCTO_TASK_PATCH,
    YOCTO_TASK_CONFIGURE,
    YOCTO_TASK_COMPILE,
    YOCTO_TASK_INSTALL,
    YOCTO_TASK_PACKAGE,
    YOCTO_TASK_ROOTFS,
    YOCTO_TASK_IMAGE,
    YOCTO_TASK_COUNT
} yocto_task_type_t;

typedef enum {
    IMAGE_FMT_TAR,
    IMAGE_FMT_EXT4,
    IMAGE_FMT_WIC,
    IMAGE_FMT_CPIO,
    IMAGE_FMT_SQUASHFS,
    IMAGE_FMT_UBI,
    IMAGE_FMT_COUNT
} image_format_t;

typedef enum {
    ARCH_ARM,
    ARCH_ARM64,
    ARCH_X86,
    ARCH_X86_64,
    ARCH_MIPS,
    ARCH_RISCV,
    ARCH_COUNT
} target_arch_t;

typedef enum {
    LIBC_GLIBC,
    LIBC_MUSL,
    LIBC_UCLIBC,
    LIBC_COUNT
} target_libc_t;

typedef struct {
    char name[64];
    char path[YOCTO_PATH_MAX];
    int  priority;
    int  enabled;
} yocto_layer_t;

typedef struct {
    char        name[128];
    char        version[32];
    char        description[256];
    char        license[64];
    char        src_uri[YOCTO_PATH_MAX];
    char        depends[YOCTO_MAX_DEPENDS][64];
    int         depends_count;
    int         layer_index;
    uint8_t     built;
} yocto_recipe_t;

typedef struct {
    yocto_task_type_t type;
    const char       *name;
    int               (*exec)(void *recipe);
    int               (*prefunc)(void *recipe);
    int               (*postfunc)(void *recipe);
    uint8_t           completed;
    uint8_t           failed;
} yocto_task_t;

typedef struct {
    yocto_task_t   tasks[YOCTO_MAX_TASKS];
    int            task_count;
    yocto_recipe_t recipe;
} bitbake_exec_t;

typedef struct {
    char name[64];
    char prompt[128];
    char help[256];
    int  value;
    int  default_val;
    int  depends_on;
} buildroot_config_t;

typedef struct {
    char name[64];
    char version[32];
    char source[YOCTO_PATH_MAX];
    char depends[YOCTO_MAX_DEPENDS][64];
    int  depends_count;
} buildroot_package_t;

typedef struct {
    char sysroot_path[YOCTO_PATH_MAX];
    char cross_compile_prefix[128];
    char target_triplet[64];
    char staging_dir[YOCTO_PATH_MAX];
} cross_toolchain_t;

typedef struct {
    yocto_layer_t          layers[YOCTO_MAX_LAYERS];
    int                    layer_count;
    yocto_recipe_t         recipes[YOCTO_MAX_RECIPES];
    int                    recipe_count;
    cross_toolchain_t      toolchain;
    target_arch_t          arch;
    target_libc_t          libc;
    char                   build_dir[YOCTO_PATH_MAX];
    char                   deploy_dir[YOCTO_PATH_MAX];
} yocto_project_t;

typedef struct {
    buildroot_package_t    packages[BUILDROOT_MAX_PACKAGES];
    int                    package_count;
    buildroot_config_t     configs[BUILDROOT_MAX_CONFIGS];
    int                    config_count;
    cross_toolchain_t      toolchain;
    target_arch_t          arch;
    char                   output_dir[YOCTO_PATH_MAX];
    char                   target_dir[YOCTO_PATH_MAX];
} buildroot_project_t;

typedef struct {
    image_format_t format;
    char           output_name[256];
    char           rootfs_dir[YOCTO_PATH_MAX];
    uint64_t       size_mb;
    int            compression;
} image_config_t;

int   yocto_project_init(yocto_project_t *proj, const char *build_dir);
int   yocto_layer_add(yocto_project_t *proj, const char *name, const char *path, int priority);
int   yocto_layer_find(yocto_project_t *proj, const char *name);

int   yocto_recipe_parse(yocto_recipe_t *recipe, const char *bb_file_path);
int   yocto_recipe_add(yocto_project_t *proj, const yocto_recipe_t *recipe);
int   yocto_recipe_find(yocto_project_t *proj, const char *name);

int   bitbake_task_init(bitbake_exec_t *exec, yocto_recipe_t *recipe);
int   bitbake_task_execute(bitbake_exec_t *exec, yocto_task_type_t type);
int   bitbake_task_execute_all(bitbake_exec_t *exec);

int   cross_toolchain_setup(cross_toolchain_t *tc, target_arch_t arch, target_libc_t libc);
void  cross_toolchain_print(const cross_toolchain_t *tc);
int   cross_toolchain_verify(const cross_toolchain_t *tc);

int   buildroot_project_init(buildroot_project_t *proj, const char *output_dir);
int   buildroot_config_load(buildroot_project_t *proj, const char *config_file);
int   buildroot_config_get(buildroot_project_t *proj, const char *name, int *value);
int   buildroot_package_add(buildroot_project_t *proj, const char *name, const char *version);
int   buildroot_package_select(buildroot_project_t *proj, const char *name, int enable);

int   image_generate(const image_config_t *cfg, const char *output_path);
int   image_create_tar(const image_config_t *cfg, const char *output_path);
int   image_create_ext4(const image_config_t *cfg, const char *output_path);
int   image_create_wic(const image_config_t *cfg, const char *output_path);

void  yocto_project_dump(const yocto_project_t *proj);
void  buildroot_project_dump(const buildroot_project_t *proj);

#endif
