#ifndef BUSYBOX_APP_H
#define BUSYBOX_APP_H

#include <stdint.h>
#include <stddef.h>

#define BB_MAX_APPLETS         256
#define BB_MAX_NAME            32
#define BB_MAX_PATH            256
#define BB_MAX_ARGS            32
#define BB_ARG_MAX             256
#define BB_INITTAB_MAX         64
#define BB_INITTAB_LINE_MAX    256
#define BB_MAX_SERVICES        32
#define BB_IFACE_MAX           16
#define BB_ROUTE_MAX           32

typedef enum {
    BB_APPLET_INIT,
    BB_APPLET_SH,
    BB_APPLET_ASH,
    BB_APPLET_HUSH,
    BB_APPLET_MOUNT,
    BB_APPLET_UMOUNT,
    BB_APPLET_IFCONFIG,
    BB_APPLET_ROUTE,
    BB_APPLET_UDHCPC,
    BB_APPLET_IFUP,
    BB_APPLET_IFDOWN,
    BB_APPLET_MDEV,
    BB_APPLET_SYSLOGD,
    BB_APPLET_KLOGD,
    BB_APPLET_GETTY,
    BB_APPLET_LOGIN,
    BB_APPLET_REBOOT,
    BB_APPLET_POWEROFF,
    BB_APPLET_HALT,
    BB_APPLET_SWAPON,
    BB_APPLET_SWAPOFF,
    BB_APPLET_LSMOD,
    BB_APPLET_INSMOD,
    BB_APPLET_RMMOD,
    BB_APPLET_DMESG,
    BB_APPLET_SYSCTL,
    BB_APPLET_PING,
    BB_APPLET_WGET,
    BB_APPLET_TELNET,
    BB_APPLET_HTTPD,
    BB_APPLET_CROND,
    BB_APPLET_SETSID,
    BB_APPLET_CTTYHACK,
    BB_APPLET_FDISK,
    BB_APPLET_MKFS,
    BB_APPLET_FSCK,
    BB_APPLET_COUNT
} bb_applet_id_t;

typedef enum {
    BB_INITTAB_SYSINIT,
    BB_INITTAB_WAIT,
    BB_INITTAB_ONCE,
    BB_INITTAB_RESPAWN,
    BB_INITTAB_ASKFIRST,
    BB_INITTAB_SHUTDOWN,
    BB_INITTAB_RESTART,
    BB_INITTAB_CTRLALTDEL,
    BB_INITTAB_COUNT
} bb_inittab_action_t;

typedef enum {
    BB_NET_STATE_DOWN,
    BB_NET_STATE_UP,
    BB_NET_STATE_CONFIGURING,
    BB_NET_STATE_COUNT
} bb_net_state_t;

typedef int (*bb_applet_main_t)(int argc, char *argv[]);

typedef struct {
    bb_applet_id_t id;
    char           name[BB_MAX_NAME];
    bb_applet_main_t main_func;
    char           usage[256];
    char           full_name[64];
    uint8_t        enabled;
} bb_applet_t;

typedef struct {
    bb_inittab_action_t action;
    char                id[16];
    int                 runlevel;
    char                process[BB_ARG_MAX];
    char                tty[BB_PATH_MAX];
    uint8_t             active;
} bb_inittab_entry_t;

typedef struct {
    bb_inittab_entry_t entries[BB_INITTAB_MAX];
    int                entry_count;
} bb_inittab_t;

typedef struct {
    char name[BB_IFACE_MAX];
    char ip_addr[32];
    char netmask[32];
    char broadcast[32];
    char gateway[32];
    char mac_addr[18];
    int  mtu;
    bb_net_state_t state;
} bb_net_iface_t;

typedef struct {
    bb_net_iface_t ifaces[BB_IFACE_MAX];
    int            iface_count;
    char           default_gateway[32];
    char           dns_server[32];
    char           hostname[64];
} bb_net_config_t;

typedef struct {
    char          destination[32];
    char          gateway[32];
    char          netmask[32];
    char          iface[BB_IFACE_MAX];
    int           metric;
    uint8_t       active;
} bb_route_entry_t;

typedef struct {
    bb_route_entry_t entries[BB_ROUTE_MAX];
    int              entry_count;
} bb_route_table_t;

typedef struct {
    char name[BB_MAX_NAME];
    char source[BB_PATH_MAX];
    char target[BB_PATH_MAX];
    char fs_type[16];
    char options[128];
} bb_mount_t;

typedef struct {
    bb_applet_t   applets[BB_MAX_APPLETS];
    int           applet_count;
    bb_inittab_t  inittab;
    bb_net_config_t net_config;
    bb_route_table_t route_table;
    char          busybox_path[BB_PATH_MAX];
    uint32_t      applet_mask;
} busybox_ctx_t;

int   busybox_init(busybox_ctx_t *ctx, const char *install_path);

int   bb_applet_register(busybox_ctx_t *ctx, bb_applet_id_t id, const char *name,
                           bb_applet_main_t func, const char *usage);
int   bb_applet_find(busybox_ctx_t *ctx, const char *name);
int   bb_applet_invoke(busybox_ctx_t *ctx, const char *name, int argc, char *argv[]);
int   bb_applet_install_symlinks(busybox_ctx_t *ctx, const char *target_dir);

int   bb_inittab_load(busybox_ctx_t *ctx, const char *inittab_path);
int   bb_inittab_parse_line(bb_inittab_entry_t *entry, const char *line);
int   bb_inittab_add(busybox_ctx_t *ctx, bb_inittab_action_t action,
                      const char *id, int runlevel, const char *process, const char *tty);
int   bb_inittab_execute(busybox_ctx_t *ctx, bb_inittab_action_t action);
int   bb_inittab_execute_all(busybox_ctx_t *ctx, int runlevel);
void  bb_inittab_dump(const busybox_ctx_t *ctx);

int   bb_shell_run(const char *command);
int   bb_shell_ash_run(int argc, char *argv[]);
int   bb_shell_hush_run(int argc, char *argv[]);

int   bb_sysinit_run(busybox_ctx_t *ctx);
int   bb_sysinit_mount_proc(busybox_ctx_t *ctx);
int   bb_sysinit_mount_sys(busybox_ctx_t *ctx);
int   bb_sysinit_mount_dev(busybox_ctx_t *ctx);
int   bb_sysinit_mdev_setup(busybox_ctx_t *ctx);
int   bb_sysinit_syslogd_start(busybox_ctx_t *ctx);

int   bb_net_iface_add(busybox_ctx_t *ctx, const char *name);
int   bb_net_iface_config(busybox_ctx_t *ctx, const char *name, const char *ip,
                           const char *mask, const char *bcast);
int   bb_net_iface_up(busybox_ctx_t *ctx, const char *name);
int   bb_net_iface_down(busybox_ctx_t *ctx, const char *name);
int   bb_net_dhcp_start(busybox_ctx_t *ctx, const char *iface);
int   bb_net_dhcp_renew(busybox_ctx_t *ctx, const char *iface);
int   bb_net_set_gateway(busybox_ctx_t *ctx, const char *gw);
int   bb_net_set_dns(busybox_ctx_t *ctx, const char *dns);

int   bb_route_add(busybox_ctx_t *ctx, const char *dest, const char *gw,
                    const char *mask, const char *iface, int metric);
int   bb_route_del(busybox_ctx_t *ctx, const char *dest);
int   bb_route_flush(busybox_ctx_t *ctx);
void  bb_route_dump(const busybox_ctx_t *ctx);

int   bb_mount_add(busybox_ctx_t *ctx, const char *name, const char *source,
                    const char *target, const char *fs_type, const char *options);
int   bb_mount_do(busybox_ctx_t *ctx, const char *source, const char *target,
                  const char *fs_type, const char *options);
int   bb_umount_do(const char *target);

int   bb_syslogd_init(void);
int   bb_syslogd_log(int priority, const char *tag, const char *msg);
int   bb_klogd_init(void);
int   bb_module_load(const char *module_name);
int   bb_module_unload(const char *module_name);
int   bb_module_list(char *buf, int maxlen);

int   bb_reboot_system(void);
int   bb_poweroff_system(void);

void  busybox_ctx_dump(const busybox_ctx_t *ctx);
void  bb_net_iface_dump(const busybox_ctx_t *ctx);

#endif
