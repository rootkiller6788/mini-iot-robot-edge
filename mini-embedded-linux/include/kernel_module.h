#ifndef KERNEL_MODULE_H
#define KERNEL_MODULE_H

#include <stdint.h>
#include <stddef.h>

#define KM_MAX_NAME            64
#define KM_MAX_VERSION         32
#define KM_MAX_PATH            256
#define KM_MAX_PARAMS          16
#define KM_MAX_DEPENDENCIES    8
#define KM_MAX_SYMBOLS         512
#define KM_MAX_PROCFILES       8
#define KM_MAX_DEVFILES        8
#define KM_PROC_READ_BUFSZ     4096
#define KM_PARAM_DESC_MAX      128

typedef enum {
    KM_STATE_UNLOADED,
    KM_STATE_LOADING,
    KM_STATE_LIVE,
    KM_STATE_UNLOADING,
    KM_STATE_COUNT
} km_state_t;

typedef enum {
    KM_PARAM_BOOL,
    KM_PARAM_INT,
    KM_PARAM_UINT,
    KM_PARAM_LONG,
    KM_PARAM_ULONG,
    KM_PARAM_STRING,
    KM_PARAM_SHORT,
    KM_PARAM_USHORT,
    KM_PARAM_ARRAY_INT,
    KM_PARAM_COUNT
} km_param_type_t;

typedef enum {
    KM_LICENSE_GPL,
    KM_LICENSE_GPL_V2,
    KM_LICENSE_DUAL_BSD_GPL,
    KM_LICENSE_DUAL_MIT_GPL,
    KM_LICENSE_PROPRIETARY,
    KM_LICENSE_COUNT
} km_license_t;

typedef enum {
    KM_DEVTYPE_CHAR,
    KM_DEVTYPE_BLOCK,
    KM_DEVTYPE_MISC,
    KM_DEVTYPE_COUNT
} km_devtype_t;

typedef struct {
    char            name[KM_MAX_NAME];
    char            description[KM_PARAM_DESC_MAX];
    km_param_type_t type;
    union {
        int      bool_val;
        int      int_val;
        unsigned uint_val;
        long     long_val;
        unsigned long ulong_val;
        char     string_val[128];
        short    short_val;
        unsigned short ushort_val;
        int      array_int_val[32];
    } value;
    int             array_len;
} km_module_param_t;

typedef struct {
    char            name[KM_MAX_NAME];
    char            version[KM_MAX_VERSION];
    char            author[64];
    char            description[256];
    km_license_t    license;
    km_state_t      state;
    km_module_param_t params[KM_MAX_PARAMS];
    int             param_count;
    char            depends[KM_MAX_DEPENDENCIES][KM_MAX_NAME];
    int             depends_count;
    char            alias[KM_MAX_NAME];
    void           *module_core;
    uint64_t        core_size;
    uint32_t        checksum;
    int             ref_count;
    int             use_count;
    uint8_t         persistent;
    uint8_t         tainted;
    uint32_t        taint_flags;
} km_module_t;

typedef struct {
    char proc_name[KM_MAX_NAME];
    char proc_dir[KM_MAX_PATH];
    int  mode;
    int  (*read_func)(char *buf, int maxlen, void *data);
    int  (*write_func)(const char *buf, int len, void *data);
    void *user_data;
} km_proc_file_t;

typedef struct {
    char          dev_name[KM_MAX_NAME];
    km_devtype_t  dev_type;
    unsigned int  major;
    unsigned int  minor;
    int           mode;
    void         *dev_data;
    int           (*open)(void *data);
    int           (*release)(void *data);
    int           (*read)(void *data, char *buf, int len);
    int           (*write)(void *data, const char *buf, int len);
} km_device_t;

typedef struct {
    void  (*tasklet_func)(unsigned long data);
    unsigned long tasklet_data;
    uint8_t scheduled;
    uint8_t active;
} km_tasklet_t;

typedef struct {
    void  (*work_func)(void *data);
    void *work_data;
    uint8_t queued;
    uint8_t executing;
} km_workqueue_t;

typedef struct {
    km_module_t       modules[KM_MAX_SYMBOLS];
    int               module_count;
    km_proc_file_t    proc_files[KM_MAX_PROCFILES];
    int               proc_count;
    km_device_t       devices[KM_MAX_DEVFILES];
    int               device_count;
    km_tasklet_t      tasklets[8];
    int               tasklet_count;
    km_workqueue_t    workqueues[8];
    int               workqueue_count;
} km_subsystem_t;

int   km_subsystem_init(km_subsystem_t *ks);

int   km_module_init(km_module_t *mod, const char *name, const char *version);
int   km_module_set_license(km_module_t *mod, km_license_t lic);
int   km_module_set_author(km_module_t *mod, const char *author);
int   km_module_set_description(km_module_t *mod, const char *desc);

int   km_module_param_add(km_module_t *mod, const char *name, km_param_type_t type,
                           const char *desc);
int   km_module_param_set_int(km_module_t *mod, const char *name, int val);
int   km_module_param_set_string(km_module_t *mod, const char *name, const char *val);
int   km_module_param_get_int(km_module_t *mod, const char *name, int *val);

int   km_module_add_dependency(km_module_t *mod, const char *dep_name);
int   km_module_check_dependencies(km_subsystem_t *ks, const km_module_t *mod);

int   km_module_load(km_subsystem_t *ks, km_module_t *mod);
int   km_module_unload(km_subsystem_t *ks, km_module_t *mod);
int   km_module_find(km_subsystem_t *ks, const char *name);
void  km_module_list(km_subsystem_t *ks);

int   km_proc_create(km_subsystem_t *ks, const char *name, const char *dir,
                      int mode, void *data,
                      int (*read_func)(char *buf, int maxlen, void *data),
                      int (*write_func)(const char *buf, int len, void *data));
int   km_proc_read(const km_subsystem_t *ks, const char *name, char *buf, int maxlen);
int   km_proc_write(km_subsystem_t *ks, const char *name, const char *buf, int len);
int   km_proc_remove(km_subsystem_t *ks, const char *name);

int   km_device_create(km_subsystem_t *ks, const char *name, km_devtype_t type,
                        unsigned int major, unsigned int minor, int mode, void *data,
                        int (*open)(void *), int (*release)(void *),
                        int (*read)(void *, char *, int),
                        int (*write)(void *, const char *, int));
int   km_device_open(km_subsystem_t *ks, const char *name);
int   km_device_read(km_subsystem_t *ks, const char *name, char *buf, int len);
int   km_device_write(km_subsystem_t *ks, const char *name, const char *buf, int len);

int   km_tasklet_create(km_subsystem_t *ks, void (*func)(unsigned long), unsigned long data);
int   km_tasklet_schedule(km_subsystem_t *ks, int idx);
int   km_tasklet_kill(km_subsystem_t *ks, int idx);

int   km_workqueue_create(km_subsystem_t *ks, void (*func)(void *), void *data);
int   km_workqueue_queue(km_subsystem_t *ks, int idx);
int   km_workqueue_cancel(km_subsystem_t *ks, int idx);

void  km_subsystem_dump(const km_subsystem_t *ks);
void  km_module_dump(const km_module_t *mod);

#endif
