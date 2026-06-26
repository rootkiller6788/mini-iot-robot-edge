#include "busybox_app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *inittab_action_names[BB_INITTAB_COUNT] = {
    "sysinit", "wait", "once", "respawn", "askfirst", "shutdown", "restart", "ctrlaltdel"
};

int busybox_init(busybox_ctx_t *ctx, const char *install_path)
{
    if (!ctx) return -1;
    memset(ctx, 0, sizeof(*ctx));
    if (install_path) {
        strncpy(ctx->busybox_path, install_path, BB_PATH_MAX - 1);
    } else {
        strncpy(ctx->busybox_path, "/bin/busybox", BB_PATH_MAX - 1);
    }
    ctx->applet_mask = 0;
    return 0;
}

int bb_applet_register(busybox_ctx_t *ctx, bb_applet_id_t id, const char *name,
                        bb_applet_main_t func, const char *usage)
{
    if (!ctx || !name || ctx->applet_count >= BB_MAX_APPLETS) return -1;
    bb_applet_t *a = &ctx->applets[ctx->applet_count];
    memset(a, 0, sizeof(*a));
    a->id = id;
    strncpy(a->name, name, BB_MAX_NAME - 1);
    a->main_func = func;
    if (usage) strncpy(a->usage, usage, 255);
    a->enabled = 1;
    ctx->applet_mask |= (1U << (unsigned)id);
    ctx->applet_count++;
    return ctx->applet_count - 1;
}

int bb_applet_find(busybox_ctx_t *ctx, const char *name)
{
    if (!ctx || !name) return -1;
    for (int i = 0; i < ctx->applet_count; i++) {
        if (strcmp(ctx->applets[i].name, name) == 0) return i;
    }
    return -1;
}

int bb_applet_invoke(busybox_ctx_t *ctx, const char *name, int argc, char *argv[])
{
    if (!ctx || !name) return -1;
    int idx = bb_applet_find(ctx, name);
    if (idx < 0) {
        fprintf(stderr, "%s: applet not found\n", name);
        return -1;
    }
    bb_applet_t *a = &ctx->applets[idx];
    if (!a->enabled || !a->main_func) return -2;
    return a->main_func(argc, argv);
}

int bb_applet_install_symlinks(busybox_ctx_t *ctx, const char *target_dir)
{
    if (!ctx || !target_dir) return -1;
    int count = 0;
    for (int i = 0; i < ctx->applet_count; i++) {
        printf("[LN] %s/%s -> %s\n", target_dir, ctx->applets[i].name, ctx->busybox_path);
        count++;
    }
    return count;
}

int bb_inittab_load(busybox_ctx_t *ctx, const char *inittab_path)
{
    (void)inittab_path;
    if (!ctx) return -1;
    ctx->inittab.entry_count = 0;
    bb_inittab_add(ctx, BB_INITTAB_SYSINIT, "S0", 0, "/etc/init.d/rcS", "");
    bb_inittab_add(ctx, BB_INITTAB_SYSINIT, "S1", 0, "/bin/mount -t proc proc /proc", "");
    bb_inittab_add(ctx, BB_INITTAB_SYSINIT, "S2", 0, "/bin/mount -t sysfs sysfs /sys", "");
    bb_inittab_add(ctx, BB_INITTAB_WAIT, "W0", 0, "/sbin/mdev -s", "");
    bb_inittab_add(ctx, BB_INITTAB_RESPAWN, "R0", 0, "/sbin/getty 115200 ttyS0", "/dev/ttyS0");
    bb_inittab_add(ctx, BB_INITTAB_RESPAWN, "R1", 0, "/sbin/getty 115200 tty1", "/dev/tty1");
    bb_inittab_add(ctx, BB_INITTAB_ONCE, "O0", 0, "/usr/sbin/telnetd", "");
    bb_inittab_add(ctx, BB_INITTAB_CTRLALTDEL, "C0", 0, "/sbin/reboot", "");
    bb_inittab_add(ctx, BB_INITTAB_SHUTDOWN, "H0", 0, "/bin/umount -a -r", "");
    return ctx->inittab.entry_count;
}

int bb_inittab_parse_line(bb_inittab_entry_t *entry, const char *line)
{
    if (!entry || !line) return -1;
    memset(entry, 0, sizeof(*entry));
    char id[16], action[32], process[BB_ARG_MAX], tty[BB_PATH_MAX];
    int fields = 0;
    const char *p = line;
    while (*p == '#' || *p == ' ' || *p == '\t') p++;
    char *rest = (char *)p;
    char *token = strtok(rest, ":");
    while (token && fields < 4) {
        switch (fields) {
            case 0: strncpy(id, token, 15); break;
            case 1: /* runlevel - skip */ break;
            case 2: strncpy(action, token, 31); break;
            case 3: strncpy(process, token, BB_ARG_MAX - 1); break;
        }
        token = strtok(NULL, ":");
        fields++;
    }
    if (fields >= 3) {
        strncpy(entry->id, id, 15);
        strncpy(entry->process, process, BB_ARG_MAX - 1);
        entry->active = 1;
        for (int i = 0; i < BB_INITTAB_COUNT; i++) {
            if (strcmp(action, inittab_action_names[i]) == 0) {
                entry->action = (bb_inittab_action_t)i;
                return 0;
            }
        }
    }
    return -1;
}

int bb_inittab_add(busybox_ctx_t *ctx, bb_inittab_action_t action,
                    const char *id, int runlevel, const char *process, const char *tty)
{
    if (!ctx || !id || !process || ctx->inittab.entry_count >= BB_INITTAB_MAX) return -1;
    bb_inittab_entry_t *e = &ctx->inittab.entries[ctx->inittab.entry_count];
    memset(e, 0, sizeof(*e));
    e->action = action;
    strncpy(e->id, id, 15);
    e->runlevel = runlevel;
    strncpy(e->process, process, BB_ARG_MAX - 1);
    if (tty) strncpy(e->tty, tty, BB_PATH_MAX - 1);
    e->active = 1;
    ctx->inittab.entry_count++;
    return ctx->inittab.entry_count - 1;
}

int bb_inittab_execute(busybox_ctx_t *ctx, bb_inittab_action_t action)
{
    if (!ctx) return -1;
    int count = 0;
    for (int i = 0; i < ctx->inittab.entry_count; i++) {
        bb_inittab_entry_t *e = &ctx->inittab.entries[i];
        if (e->action == action && e->active) {
            printf("[INIT:%s] %s %s\n", inittab_action_names[action], e->id, e->process);
            count++;
        }
    }
    return count;
}

int bb_inittab_execute_all(busybox_ctx_t *ctx, int runlevel)
{
    (void)runlevel;
    if (!ctx) return -1;
    int total = 0;
    for (int a = 0; a < BB_INITTAB_COUNT; a++) {
        total += bb_inittab_execute(ctx, (bb_inittab_action_t)a);
    }
    return total;
}

void bb_inittab_dump(const busybox_ctx_t *ctx)
{
    if (!ctx) return;
    printf("========== /etc/inittab ==========\n");
    for (int i = 0; i < ctx->inittab.entry_count; i++) {
        const bb_inittab_entry_t *e = &ctx->inittab.entries[i];
        printf("%s:%d:%s:%s", e->id, e->runlevel, inittab_action_names[e->action], e->process);
        if (e->tty[0]) printf(" [%s]", e->tty);
        printf("\n");
    }
}

int bb_shell_run(const char *command)
{
    if (!command) return -1;
    printf("[SH] %s\n", command);
    return 0;
}

int bb_shell_ash_run(int argc, char *argv[])
{
    (void)argc; (void)argv;
    printf("[ASH] BusyBox ash shell\n");
    return 0;
}

int bb_shell_hush_run(int argc, char *argv[])
{
    (void)argc; (void)argv;
    printf("[HUSH] BusyBox hush shell\n");
    return 0;
}

int bb_sysinit_run(busybox_ctx_t *ctx)
{
    if (!ctx) return -1;
    printf("[SYSINIT] Running system initialization...\n");
    bb_sysinit_mount_proc(ctx);
    bb_sysinit_mount_sys(ctx);
    bb_sysinit_mount_dev(ctx);
    bb_sysinit_mdev_setup(ctx);
    bb_sysinit_syslogd_start(ctx);
    bb_inittab_execute(ctx, BB_INITTAB_SYSINIT);
    return 0;
}

int bb_sysinit_mount_proc(busybox_ctx_t *ctx)
{
    (void)ctx;
    printf("[SYSINIT] mount -t proc proc /proc\n");
    return 0;
}

int bb_sysinit_mount_sys(busybox_ctx_t *ctx)
{
    (void)ctx;
    printf("[SYSINIT] mount -t sysfs sysfs /sys\n");
    return 0;
}

int bb_sysinit_mount_dev(busybox_ctx_t *ctx)
{
    (void)ctx;
    printf("[SYSINIT] mount -t devtmpfs devtmpfs /dev\n");
    return 0;
}

int bb_sysinit_mdev_setup(busybox_ctx_t *ctx)
{
    (void)ctx;
    printf("[SYSINIT] mdev -s (coldplug)\n");
    return 0;
}

int bb_sysinit_syslogd_start(busybox_ctx_t *ctx)
{
    (void)ctx;
    printf("[SYSINIT] syslogd started\n");
    return 0;
}

int bb_net_iface_add(busybox_ctx_t *ctx, const char *name)
{
    if (!ctx || !name || ctx->net_config.iface_count >= BB_IFACE_MAX) return -1;
    bb_net_iface_t *iface = &ctx->net_config.ifaces[ctx->net_config.iface_count];
    memset(iface, 0, sizeof(*iface));
    strncpy(iface->name, name, BB_IFACE_MAX - 1);
    iface->state = BB_NET_STATE_DOWN;
    iface->mtu = 1500;
    strncpy(iface->mac_addr, "00:11:22:33:44:55", 17);
    ctx->net_config.iface_count++;
    return ctx->net_config.iface_count - 1;
}

int bb_net_iface_config(busybox_ctx_t *ctx, const char *name, const char *ip,
                         const char *mask, const char *bcast)
{
    if (!ctx || !name || !ip) return -1;
    for (int i = 0; i < ctx->net_config.iface_count; i++) {
        if (strcmp(ctx->net_config.ifaces[i].name, name) == 0) {
            bb_net_iface_t *iface = &ctx->net_config.ifaces[i];
            strncpy(iface->ip_addr, ip, 31);
            if (mask) strncpy(iface->netmask, mask, 31); else strncpy(iface->netmask, "255.255.255.0", 31);
            if (bcast) strncpy(iface->broadcast, bcast, 31);
            printf("[NET] %s: %s/%s\n", name, ip, iface->netmask);
            return 0;
        }
    }
    return -1;
}

int bb_net_iface_up(busybox_ctx_t *ctx, const char *name)
{
    if (!ctx || !name) return -1;
    for (int i = 0; i < ctx->net_config.iface_count; i++) {
        if (strcmp(ctx->net_config.ifaces[i].name, name) == 0) {
            ctx->net_config.ifaces[i].state = BB_NET_STATE_UP;
            printf("[NET] ifup %s\n", name);
            return 0;
        }
    }
    return -1;
}

int bb_net_iface_down(busybox_ctx_t *ctx, const char *name)
{
    if (!ctx || !name) return -1;
    for (int i = 0; i < ctx->net_config.iface_count; i++) {
        if (strcmp(ctx->net_config.ifaces[i].name, name) == 0) {
            ctx->net_config.ifaces[i].state = BB_NET_STATE_DOWN;
            printf("[NET] ifdown %s\n", name);
            return 0;
        }
    }
    return -1;
}

int bb_net_dhcp_start(busybox_ctx_t *ctx, const char *iface)
{
    if (!ctx || !iface) return -1;
    printf("[DHCP] udhcpc on %s (sending discover...)\n", iface);
    return 0;
}

int bb_net_dhcp_renew(busybox_ctx_t *ctx, const char *iface)
{
    (void)ctx;
    printf("[DHCP] udhcpc renew on %s\n", iface);
    return 0;
}

int bb_net_set_gateway(busybox_ctx_t *ctx, const char *gw)
{
    if (!ctx || !gw) return -1;
    strncpy(ctx->net_config.default_gateway, gw, 31);
    printf("[NET] default gateway: %s\n", gw);
    return 0;
}

int bb_net_set_dns(busybox_ctx_t *ctx, const char *dns)
{
    if (!ctx || !dns) return -1;
    strncpy(ctx->net_config.dns_server, dns, 31);
    printf("[NET] DNS: %s\n", dns);
    return 0;
}

int bb_route_add(busybox_ctx_t *ctx, const char *dest, const char *gw,
                  const char *mask, const char *iface, int metric)
{
    if (!ctx || !dest || ctx->route_table.entry_count >= BB_ROUTE_MAX) return -1;
    bb_route_entry_t *r = &ctx->route_table.entries[ctx->route_table.entry_count];
    memset(r, 0, sizeof(*r));
    strncpy(r->destination, dest, 31);
    if (gw) strncpy(r->gateway, gw, 31);
    if (mask) strncpy(r->netmask, mask, 31); else strncpy(r->netmask, "0.0.0.0", 31);
    if (iface) strncpy(r->iface, iface, BB_IFACE_MAX - 1);
    r->metric = metric;
    r->active = 1;
    ctx->route_table.entry_count++;
    printf("[ROUTE] add: %s via %s\n", dest, gw ? gw : "*");
    return ctx->route_table.entry_count - 1;
}

int bb_route_del(busybox_ctx_t *ctx, const char *dest)
{
    if (!ctx || !dest) return -1;
    printf("[ROUTE] del: %s\n", dest);
    return 0;
}

int bb_route_flush(busybox_ctx_t *ctx)
{
    if (!ctx) return -1;
    printf("[ROUTE] flushing all routes\n");
    ctx->route_table.entry_count = 0;
    return 0;
}

void bb_route_dump(const busybox_ctx_t *ctx)
{
    if (!ctx) return;
    printf("Kernel IP routing table\n");
    printf("%-20s %-16s %-16s %s\n", "Destination", "Gateway", "Genmask", "Iface");
    for (int i = 0; i < ctx->route_table.entry_count; i++) {
        const bb_route_entry_t *r = &ctx->route_table.entries[i];
        printf("%-20s %-16s %-16s %s\n", r->destination, r->gateway, r->netmask, r->iface);
    }
}

int bb_mount_add(busybox_ctx_t *ctx, const char *name, const char *source,
                  const char *target, const char *fs_type, const char *options)
{
    (void)ctx; (void)name;
    printf("[MOUNT] %s -> %s (type: %s)\n", source, target, fs_type);
    return 0;
}

int bb_mount_do(busybox_ctx_t *ctx, const char *source, const char *target,
                const char *fs_type, const char *options)
{
    (void)ctx;
    printf("[MOUNT] %s on %s type %s (%s)\n", source, target, fs_type, options ? options : "");
    return 0;
}

int bb_umount_do(const char *target)
{
    printf("[UMOUNT] %s\n", target);
    return 0;
}

int bb_syslogd_init(void)
{
    printf("[SYSLOGD] System log daemon started\n");
    return 0;
}

int bb_syslogd_log(int priority, const char *tag, const char *msg)
{
    printf("[%d] %s: %s\n", priority, tag, msg);
    return 0;
}

int bb_klogd_init(void)
{
    printf("[KLOGD] Kernel log daemon started\n");
    return 0;
}

int bb_module_load(const char *module_name)
{
    if (!module_name) return -1;
    printf("[INSMOD] %s\n", module_name);
    return 0;
}

int bb_module_unload(const char *module_name)
{
    if (!module_name) return -1;
    printf("[RMMOD] %s\n", module_name);
    return 0;
}

int bb_module_list(char *buf, int maxlen)
{
    if (!buf) return -1;
    snprintf(buf, (size_t)maxlen, "Module Size Used by\n");
    return (int)strlen(buf);
}

int bb_reboot_system(void)
{
    printf("[REBOOT] System going down for reboot NOW!\n");
    return 0;
}

int bb_poweroff_system(void)
{
    printf("[POWEROFF] System halted\n");
    return 0;
}

void busybox_ctx_dump(const busybox_ctx_t *ctx)
{
    if (!ctx) return;
    printf("========== Busybox Context ==========\n");
    printf("Applets: %d (mask=0x%08X)\n", ctx->applet_count, ctx->applet_mask);
    for (int i = 0; i < ctx->applet_count; i++) {
        printf("  %s%s\n", ctx->applets[i].name,
               ctx->applets[i].enabled ? "" : " [disabled]");
    }
    bb_inittab_dump(ctx);
    bb_net_iface_dump(ctx);
    bb_route_dump(ctx);
}

void bb_net_iface_dump(const busybox_ctx_t *ctx)
{
    if (!ctx) return;
    printf("Network interfaces:\n");
    for (int i = 0; i < ctx->net_config.iface_count; i++) {
        const bb_net_iface_t *iface = &ctx->net_config.ifaces[i];
        printf("  %s: %s/%s [%s] mtu=%d\n", iface->name, iface->ip_addr,
               iface->netmask, iface->state == BB_NET_STATE_UP ? "UP" : "DOWN", iface->mtu);
    }
}
