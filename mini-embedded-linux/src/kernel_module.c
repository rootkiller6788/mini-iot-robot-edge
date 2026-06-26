#include "kernel_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *license_names[] = {
    "GPL", "GPL v2", "Dual BSD/GPL", "Dual MIT/GPL", "Proprietary"
};

int km_subsystem_init(km_subsystem_t *ks)
{
    if (!ks) return -1;
    memset(ks, 0, sizeof(*ks));
    return 0;
}

int km_module_init(km_module_t *mod, const char *name, const char *version)
{
    if (!mod || !name) return -1;
    memset(mod, 0, sizeof(*mod));
    strncpy(mod->name, name, KM_MAX_NAME - 1);
    if (version) strncpy(mod->version, version, KM_MAX_VERSION - 1);
    mod->license = KM_LICENSE_GPL;
    mod->state = KM_STATE_UNLOADED;
    mod->ref_count = 0;
    mod->tainted = 0;
    mod->taint_flags = 0;
    return 0;
}

int km_module_set_license(km_module_t *mod, km_license_t lic)
{
    if (!mod || lic >= KM_LICENSE_COUNT) return -1;
    mod->license = lic;
    if (lic == KM_LICENSE_PROPRIETARY) {
        mod->tainted = 1;
        mod->taint_flags |= 1;
    }
    return 0;
}

int km_module_set_author(km_module_t *mod, const char *author)
{
    if (!mod || !author) return -1;
    strncpy(mod->author, author, 63);
    return 0;
}

int km_module_set_description(km_module_t *mod, const char *desc)
{
    if (!mod || !desc) return -1;
    strncpy(mod->description, desc, 255);
    return 0;
}

int km_module_param_add(km_module_t *mod, const char *name, km_param_type_t type,
                         const char *desc)
{
    if (!mod || !name || mod->param_count >= KM_MAX_PARAMS) return -1;
    km_module_param_t *p = &mod->params[mod->param_count];
    memset(p, 0, sizeof(*p));
    strncpy(p->name, name, KM_MAX_NAME - 1);
    p->type = type;
    if (desc) strncpy(p->description, desc, KM_PARAM_DESC_MAX - 1);
    mod->param_count++;
    return mod->param_count - 1;
}

int km_module_param_set_int(km_module_t *mod, const char *name, int val)
{
    if (!mod || !name) return -1;
    for (int i = 0; i < mod->param_count; i++) {
        if (strcmp(mod->params[i].name, name) == 0) {
            mod->params[i].value.int_val = val;
            return 0;
        }
    }
    return -1;
}

int km_module_param_set_string(km_module_t *mod, const char *name, const char *val)
{
    if (!mod || !name || !val) return -1;
    for (int i = 0; i < mod->param_count; i++) {
        if (strcmp(mod->params[i].name, name) == 0) {
            strncpy(mod->params[i].value.string_val, val, 127);
            return 0;
        }
    }
    return -1;
}

int km_module_param_get_int(km_module_t *mod, const char *name, int *val)
{
    if (!mod || !name || !val) return -1;
    for (int i = 0; i < mod->param_count; i++) {
        if (strcmp(mod->params[i].name, name) == 0) {
            *val = mod->params[i].value.int_val;
            return 0;
        }
    }
    return -1;
}

int km_module_add_dependency(km_module_t *mod, const char *dep_name)
{
    if (!mod || !dep_name || mod->depends_count >= KM_MAX_DEPENDENCIES) return -1;
    strncpy(mod->depends[mod->depends_count], dep_name, KM_MAX_NAME - 1);
    mod->depends_count++;
    return 0;
}

int km_module_check_dependencies(km_subsystem_t *ks, const km_module_t *mod)
{
    if (!ks || !mod) return -1;
    for (int d = 0; d < mod->depends_count; d++) {
        int found = 0;
        for (int i = 0; i < ks->module_count; i++) {
            if (strcmp(ks->modules[i].name, mod->depends[d]) == 0 &&
                ks->modules[i].state == KM_STATE_LIVE) {
                found = 1;
                break;
            }
        }
        if (!found) return -2;
    }
    return 0;
}

int km_module_load(km_subsystem_t *ks, km_module_t *mod)
{
    if (!ks || !mod) return -1;
    if (mod->state == KM_STATE_LIVE) return 0;
    if (km_module_check_dependencies(ks, mod) != 0) {
        fprintf(stderr, "[KM] Dependency check failed for %s\n", mod->name);
        return -2;
    }
    mod->state = KM_STATE_LOADING;
    printf("[KM] init_module: %s\n", mod->name);
    if (ks->module_count < KM_MAX_SYMBOLS) {
        if (km_module_find(ks, mod->name) < 0) {
            ks->modules[ks->module_count] = *mod;
            ks->module_count++;
        }
    }
    mod->state = KM_STATE_LIVE;
    mod->ref_count = 1;
    printf("[KM] Module %s loaded (license: %s)\n", mod->name, license_names[mod->license]);
    return 0;
}

int km_module_unload(km_subsystem_t *ks, km_module_t *mod)
{
    if (!ks || !mod) return -1;
    if (mod->state != KM_STATE_LIVE) return -2;
    if (mod->use_count > 0) return -3;
    mod->state = KM_STATE_UNLOADING;
    printf("[KM] cleanup_module: %s\n", mod->name);
    mod->state = KM_STATE_UNLOADED;
    return 0;
}

int km_module_find(km_subsystem_t *ks, const char *name)
{
    if (!ks || !name) return -1;
    for (int i = 0; i < ks->module_count; i++) {
        if (strcmp(ks->modules[i].name, name) == 0) return i;
    }
    return -1;
}

void km_module_list(km_subsystem_t *ks)
{
    if (!ks) return;
    printf("Module                  Size  Used by\n");
    for (int i = 0; i < ks->module_count; i++) {
        km_module_t *m = &ks->modules[i];
        printf("%-20s %6llu %d\n", m->name, (unsigned long long)m->core_size, m->use_count);
    }
}

int km_proc_create(km_subsystem_t *ks, const char *name, const char *dir,
                    int mode, void *data,
                    int (*read_func)(char *buf, int maxlen, void *data),
                    int (*write_func)(const char *buf, int len, void *data))
{
    if (!ks || !name || ks->proc_count >= KM_MAX_PROCFILES) return -1;
    km_proc_file_t *p = &ks->proc_files[ks->proc_count];
    memset(p, 0, sizeof(*p));
    strncpy(p->proc_name, name, KM_MAX_NAME - 1);
    if (dir) strncpy(p->proc_dir, dir, KM_MAX_PATH - 1);
    p->mode = mode;
    p->user_data = data;
    p->read_func = read_func;
    p->write_func = write_func;
    ks->proc_count++;
    return ks->proc_count - 1;
}

int km_proc_read(const km_subsystem_t *ks, const char *name, char *buf, int maxlen)
{
    if (!ks || !name || !buf) return -1;
    for (int i = 0; i < ks->proc_count; i++) {
        if (strcmp(ks->proc_files[i].proc_name, name) == 0) {
            if (ks->proc_files[i].read_func) {
                return ks->proc_files[i].read_func(buf, maxlen, ks->proc_files[i].user_data);
            }
            return -3;
        }
    }
    return -2;
}

int km_proc_write(km_subsystem_t *ks, const char *name, const char *buf, int len)
{
    if (!ks || !name || !buf) return -1;
    for (int i = 0; i < ks->proc_count; i++) {
        if (strcmp(ks->proc_files[i].proc_name, name) == 0) {
            if (ks->proc_files[i].write_func) {
                return ks->proc_files[i].write_func(buf, len, ks->proc_files[i].user_data);
            }
            return -3;
        }
    }
    return -2;
}

int km_proc_remove(km_subsystem_t *ks, const char *name)
{
    (void)ks;
    (void)name;
    return 0;
}

int km_device_create(km_subsystem_t *ks, const char *name, km_devtype_t type,
                      unsigned int major, unsigned int minor, int mode, void *data,
                      int (*open_func)(void *), int (*release_func)(void *),
                      int (*read_func)(void *, char *, int),
                      int (*write_func)(void *, const char *, int))
{
    if (!ks || !name || ks->device_count >= KM_MAX_DEVFILES) return -1;
    km_device_t *d = &ks->devices[ks->device_count];
    memset(d, 0, sizeof(*d));
    strncpy(d->dev_name, name, KM_MAX_NAME - 1);
    d->dev_type = type;
    d->major = major;
    d->minor = minor;
    d->mode = mode;
    d->dev_data = data;
    d->open = open_func;
    d->release = release_func;
    d->read = read_func;
    d->write = write_func;
    ks->device_count++;
    printf("[KM] Device created: /dev/%s (major=%u, minor=%u)\n", name, major, minor);
    return ks->device_count - 1;
}

int km_device_open(km_subsystem_t *ks, const char *name)
{
    if (!ks || !name) return -1;
    for (int i = 0; i < ks->device_count; i++) {
        if (strcmp(ks->devices[i].dev_name, name) == 0) {
            if (ks->devices[i].open) return ks->devices[i].open(ks->devices[i].dev_data);
            return 0;
        }
    }
    return -1;
}

int km_device_read(km_subsystem_t *ks, const char *name, char *buf, int len)
{
    if (!ks || !name || !buf) return -1;
    for (int i = 0; i < ks->device_count; i++) {
        if (strcmp(ks->devices[i].dev_name, name) == 0) {
            if (ks->devices[i].read) return ks->devices[i].read(ks->devices[i].dev_data, buf, len);
            return 0;
        }
    }
    return -1;
}

int km_device_write(km_subsystem_t *ks, const char *name, const char *buf, int len)
{
    if (!ks || !name || !buf) return -1;
    for (int i = 0; i < ks->device_count; i++) {
        if (strcmp(ks->devices[i].dev_name, name) == 0) {
            if (ks->devices[i].write) return ks->devices[i].write(ks->devices[i].dev_data, buf, len);
            return 0;
        }
    }
    return -1;
}

int km_tasklet_create(km_subsystem_t *ks, void (*func)(unsigned long), unsigned long data)
{
    if (!ks || !func || ks->tasklet_count >= 8) return -1;
    km_tasklet_t *t = &ks->tasklets[ks->tasklet_count];
    t->tasklet_func = func;
    t->tasklet_data = data;
    t->scheduled = 0;
    t->active = 0;
    ks->tasklet_count++;
    return ks->tasklet_count - 1;
}

int km_tasklet_schedule(km_subsystem_t *ks, int idx)
{
    if (!ks || idx < 0 || idx >= ks->tasklet_count) return -1;
    km_tasklet_t *t = &ks->tasklets[idx];
    t->scheduled = 1;
    t->active = 1;
    if (t->tasklet_func) t->tasklet_func(t->tasklet_data);
    t->active = 0;
    t->scheduled = 0;
    return 0;
}

int km_tasklet_kill(km_subsystem_t *ks, int idx)
{
    if (!ks || idx < 0 || idx >= ks->tasklet_count) return -1;
    ks->tasklets[idx].scheduled = 0;
    ks->tasklets[idx].active = 0;
    return 0;
}

int km_workqueue_create(km_subsystem_t *ks, void (*func)(void *), void *data)
{
    if (!ks || !func || ks->workqueue_count >= 8) return -1;
    km_workqueue_t *w = &ks->workqueues[ks->workqueue_count];
    w->work_func = func;
    w->work_data = data;
    w->queued = 0;
    w->executing = 0;
    ks->workqueue_count++;
    return ks->workqueue_count - 1;
}

int km_workqueue_queue(km_subsystem_t *ks, int idx)
{
    if (!ks || idx < 0 || idx >= ks->workqueue_count) return -1;
    km_workqueue_t *w = &ks->workqueues[idx];
    w->queued = 1;
    w->executing = 1;
    if (w->work_func) w->work_func(w->work_data);
    w->executing = 0;
    w->queued = 0;
    return 0;
}

int km_workqueue_cancel(km_subsystem_t *ks, int idx)
{
    if (!ks || idx < 0 || idx >= ks->workqueue_count) return -1;
    ks->workqueues[idx].queued = 0;
    return 0;
}

void km_subsystem_dump(const km_subsystem_t *ks)
{
    if (!ks) return;
    printf("========== Kernel Module Subsystem ==========\n");
    printf("Modules loaded: %d\n", ks->module_count);
    km_module_list(ks);
    printf("\nProc files: %d\n", ks->proc_count);
    for (int i = 0; i < ks->proc_count; i++) {
        printf("  /proc/%s/%s\n", ks->proc_files[i].proc_dir, ks->proc_files[i].proc_name);
    }
    printf("Devices: %d\n", ks->device_count);
    for (int i = 0; i < ks->device_count; i++) {
        printf("  /dev/%s (%u:%u)\n", ks->devices[i].dev_name,
               ks->devices[i].major, ks->devices[i].minor);
    }
}

void km_module_dump(const km_module_t *mod)
{
    if (!mod) return;
    printf("========== Module: %s ==========\n", mod->name);
    printf("  Version    : %s\n", mod->version);
    printf("  Author     : %s\n", mod->author);
    printf("  Description: %s\n", mod->description);
    printf("  License    : %s\n", license_names[mod->license]);
    printf("  State      : %d\n", mod->state);
    printf("  Params     : %d\n", mod->param_count);
    for (int i = 0; i < mod->param_count; i++) {
        printf("    %s = %d\n", mod->params[i].name, mod->params[i].value.int_val);
    }
    printf("  Deps       : %d\n", mod->depends_count);
    for (int i = 0; i < mod->depends_count; i++) {
        printf("    depends: %s\n", mod->depends[i]);
    }
}
