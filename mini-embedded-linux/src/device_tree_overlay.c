#include "device_tree_overlay.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void indent_print(int indent)
{
    for (int i = 0; i < indent; i++) printf("  ");
}

int dt_init(device_tree_t *dt)
{
    if (!dt) return -1;
    memset(dt, 0, sizeof(*dt));
    dt->root_index = dt_node_add(dt, -1, "/");
    if (dt->root_index < 0) return -1;
    dt->header_ok = 1;
    dt->boot_cpuid_phys = 0;
    return 0;
}

int dt_node_add(device_tree_t *dt, int parent, const char *name)
{
    if (!dt || !name || dt->node_count >= DT_MAX_NODES) return -1;
    int idx = dt->node_count;
    dt_node_t *node = &dt->nodes[idx];
    memset(node, 0, sizeof(*node));
    strncpy(node->name, name, DT_NAME_MAX - 1);
    if (parent >= 0 && parent < dt->node_count) {
        snprintf(node->full_path, DT_PATH_MAX, "%s/%s",
                 dt->nodes[parent].full_path, name);
        if (strcmp(dt->nodes[parent].full_path, "/") == 0) {
            snprintf(node->full_path, DT_PATH_MAX, "/%s", name);
        }
        node->parent_index = parent;
        dt_node_t *pnode = &dt->nodes[parent];
        if (pnode->child_count < DT_MAX_NODES) {
            pnode->child_indices[pnode->child_count++] = idx;
        }
    } else {
        snprintf(node->full_path, DT_PATH_MAX, "/%s", name);
        node->parent_index = -1;
    }
    node->child_count = 0;
    node->prop_count = 0;
    node->phandle = 0;
    node->is_overlay = 0;
    dt->node_count++;
    return idx;
}

int dt_node_find(device_tree_t *dt, const char *path)
{
    if (!dt || !path) return -1;
    for (int i = 0; i < dt->node_count; i++) {
        if (strcmp(dt->nodes[i].full_path, path) == 0) return i;
    }
    return -1;
}

int dt_node_find_by_phandle(device_tree_t *dt, uint32_t phandle)
{
    if (!dt) return -1;
    for (int i = 0; i < dt->node_count; i++) {
        if (dt->nodes[i].phandle == (int)phandle) return i;
    }
    return -1;
}

static int dt_prop_find_in_node(const dt_node_t *node, const char *name)
{
    for (int i = 0; i < node->prop_count; i++) {
        if (strcmp(node->properties[i].name, name) == 0) return i;
    }
    return -1;
}

int dt_prop_add_u32(device_tree_t *dt, int node, const char *name, uint32_t val)
{
    if (!dt || !name || node < 0 || node >= dt->node_count) return -1;
    dt_node_t *n = &dt->nodes[node];
    if (n->prop_count >= DT_MAX_PROPERTIES) return -2;
    int dup = dt_prop_find_in_node(n, name);
    int idx = (dup >= 0) ? dup : n->prop_count;
    dt_property_t *p = &n->properties[idx];
    strncpy(p->name, name, DT_NAME_MAX - 1);
    p->type = DT_PROP_U32;
    p->value.u32 = val;
    p->len = sizeof(uint32_t);
    if (dup < 0) n->prop_count++;
    return idx;
}

int dt_prop_add_string(device_tree_t *dt, int node, const char *name, const char *val)
{
    if (!dt || !name || !val || node < 0 || node >= dt->node_count) return -1;
    dt_node_t *n = &dt->nodes[node];
    if (n->prop_count >= DT_MAX_PROPERTIES) return -2;
    int dup = dt_prop_find_in_node(n, name);
    int idx = (dup >= 0) ? dup : n->prop_count;
    dt_property_t *p = &n->properties[idx];
    strncpy(p->name, name, DT_NAME_MAX - 1);
    p->type = DT_PROP_STRING;
    strncpy(p->value.string, val, DT_VALUE_MAX - 1);
    p->len = (int)strlen(val) + 1;
    if (dup < 0) n->prop_count++;
    return idx;
}

int dt_prop_add_bytes(device_tree_t *dt, int node, const char *name,
                       const uint8_t *val, int len)
{
    if (!dt || !name || !val || node < 0 || node >= dt->node_count) return -1;
    if (len > (int)sizeof(((dt_property_t *)0)->value.bytes)) return -2;
    dt_node_t *n = &dt->nodes[node];
    if (n->prop_count >= DT_MAX_PROPERTIES) return -3;
    int dup = dt_prop_find_in_node(n, name);
    int idx = (dup >= 0) ? dup : n->prop_count;
    dt_property_t *p = &n->properties[idx];
    strncpy(p->name, name, DT_NAME_MAX - 1);
    p->type = DT_PROP_BYTES;
    memcpy(p->value.bytes, val, (size_t)len);
    p->len = len;
    if (dup < 0) n->prop_count++;
    return idx;
}

int dt_prop_get_u32(device_tree_t *dt, int node, const char *name, uint32_t *val)
{
    if (!dt || !name || !val || node < 0 || node >= dt->node_count) return -1;
    int pi = dt_prop_find_in_node(&dt->nodes[node], name);
    if (pi < 0) return -2;
    if (dt->nodes[node].properties[pi].type != DT_PROP_U32) return -3;
    *val = dt->nodes[node].properties[pi].value.u32;
    return 0;
}

int dt_prop_get_string(device_tree_t *dt, int node, const char *name,
                        char *out, int maxlen)
{
    if (!dt || !name || !out || node < 0 || node >= dt->node_count) return -1;
    int pi = dt_prop_find_in_node(&dt->nodes[node], name);
    if (pi < 0) return -2;
    strncpy(out, dt->nodes[node].properties[pi].value.string, (size_t)(maxlen - 1));
    return 0;
}

int dt_phandle_assign(device_tree_t *dt, int node)
{
    if (!dt || node < 0 || node >= dt->node_count) return -1;
    dt->phandle_count++;
    dt->nodes[node].phandle = dt->phandle_count;
    dt->phandles[dt->phandle_count - 1].phandle = (uint32_t)dt->phandle_count;
    dt->phandles[dt->phandle_count - 1].node_index = node;
    dt_prop_add_u32(dt, node, "phandle", (uint32_t)dt->phandle_count);
    return dt->phandle_count;
}

int dt_phandle_resolve(device_tree_t *dt, int node, uint32_t *phandle)
{
    if (!dt || !phandle || node < 0 || node >= dt->node_count) return -1;
    for (int i = 0; i < dt->phandle_count; i++) {
        if (dt->phandles[i].node_index == node) {
            *phandle = dt->phandles[i].phandle;
            return 0;
        }
    }
    return -1;
}

int dt_compile_dts(const char *dts_path, const char *dtb_path)
{
    if (!dts_path || !dtb_path) return -1;
    printf("[DTC] Compiling %s -> %s\n", dts_path, dtb_path);
    return 0;
}

int dt_decompile_dtb(const char *dtb_path, const char *dts_path)
{
    if (!dtb_path || !dts_path) return -1;
    printf("[DTC] Decompiling %s -> %s\n", dtb_path, dts_path);
    return 0;
}

int dt_load_dtb(device_tree_t *dt, const char *dtb_path)
{
    if (!dt || !dtb_path) return -1;
    printf("[DTB] Loading %s\n", dtb_path);
    dt->dtb_size = 0;
    return 0;
}

int dt_write_dtb(device_tree_t *dt, const char *dtb_path)
{
    if (!dt || !dtb_path) return -1;
    printf("[DTB] Writing %s (%d nodes, %u bytes)\n", dtb_path, dt->node_count, dt->dtb_size);
    return 0;
}

int dt_overlay_init(dt_overlay_t *overlay)
{
    if (!overlay) return -1;
    memset(overlay, 0, sizeof(*overlay));
    return 0;
}

int dt_overlay_add_fragment(dt_overlay_t *overlay, const char *target_path)
{
    if (!overlay || !target_path || overlay->fragment_count >= DT_MAX_FRAGMENTS) return -1;
    dt_fragment_t *f = &overlay->fragments[overlay->fragment_count];
    memset(f, 0, sizeof(*f));
    strncpy(f->target_path, target_path, DT_PATH_MAX - 1);
    f->target_node = -1;
    f->fragment_count = 0;
    overlay->fragment_count++;
    return overlay->fragment_count - 1;
}

int dt_overlay_add_node(dt_overlay_t *overlay, int frag_idx, const char *name)
{
    if (!overlay || !name || frag_idx < 0 || frag_idx >= overlay->fragment_count) return -1;
    if (overlay->overlay_node_count >= DT_MAX_NODES) return -2;
    dt_fragment_t *f = &overlay->fragments[frag_idx];
    int idx = overlay->overlay_node_count;
    dt_node_t *node = &overlay->overlay_nodes[idx];
    memset(node, 0, sizeof(*node));
    strncpy(node->name, name, DT_NAME_MAX - 1);
    node->is_overlay = 1;
    if (f->fragment_count < DT_MAX_NODES) {
        f->fragment_nodes[f->fragment_count++] = idx;
    }
    overlay->overlay_node_count++;
    return idx;
}

int dt_overlay_compile(dt_overlay_t *overlay, const char *dtbo_path)
{
    if (!overlay || !dtbo_path) return -1;
    printf("[DTBO] Compiling overlay -> %s (%d fragments, %d nodes)\n",
           dtbo_path, overlay->fragment_count, overlay->overlay_node_count);
    return 0;
}

int dt_overlay_load(dt_overlay_t *overlay, const char *dtbo_path)
{
    if (!overlay || !dtbo_path) return -1;
    printf("[DTBO] Loading overlay from %s\n", dtbo_path);
    return 0;
}

int dt_apply_overlay(device_tree_t *base_dt, const dt_overlay_t *overlay)
{
    if (!base_dt || !overlay) return -1;
    printf("[OVERLAY] Applying overlay (%d fragments)\n", overlay->fragment_count);
    int merged = 0;
    for (int f = 0; f < overlay->fragment_count; f++) {
        const dt_fragment_t *frag = &overlay->fragments[f];
        const char *tpath = frag->target_path;
        if (strcmp(tpath, "__symbols__") == 0) continue;
        if (tpath[0] == '&') tpath++;
        if (strcmp(tpath, "overlay") == 0 || strcmp(tpath, "__overlay__") == 0) {
            for (int n = 0; n < frag->fragment_count; n++) {
                int ov_idx = frag->fragment_nodes[n];
                int base_node = dt_node_add(base_dt, base_dt->root_index,
                                           overlay->overlay_nodes[ov_idx].name);
                dt_merge_overlay_node(base_dt, base_node, ov_idx);
                merged++;
            }
        }
    }
    return merged;
}

int dt_apply_overlays(device_tree_t *base_dt, const dt_overlay_t *overlays, int count)
{
    int total = 0;
    for (int i = 0; i < count; i++) {
        int ret = dt_apply_overlay(base_dt, &overlays[i]);
        if (ret < 0) return ret;
        total += ret;
    }
    return total;
}

int dt_merge_overlay_node(device_tree_t *base_dt, int base_node, int overlay_node)
{
    if (!base_dt || base_node < 0 || overlay_node < 0) return -1;
    const dt_node_t *ov = &base_dt->nodes[overlay_node];
    for (int i = 0; i < ov->prop_count; i++) {
        const dt_property_t *p = &ov->properties[i];
        switch (p->type) {
            case DT_PROP_U32:
                dt_prop_add_u32(base_dt, base_node, p->name, p->value.u32);
                break;
            case DT_PROP_STRING:
                dt_prop_add_string(base_dt, base_node, p->name, p->value.string);
                break;
            case DT_PROP_BYTES:
                dt_prop_add_bytes(base_dt, base_node, p->name, p->value.bytes, p->len);
                break;
            default:
                break;
        }
    }
    return 0;
}

int dt_pinmux_setup(dt_pinmux_table_t *table, const char *compatible)
{
    if (!table || !compatible) return -1;
    memset(table, 0, sizeof(*table));
    return 0;
}

int dt_pinmux_add(dt_pinmux_table_t *table, int gpio_chip, int gpio_pin,
                   uint32_t func, uint32_t pull, uint32_t drive)
{
    if (!table || table->pin_count >= DT_PINMUX_MAX) return -1;
    dt_pinmux_t *p = &table->pins[table->pin_count];
    p->gpio_chip = gpio_chip;
    p->gpio_pin = gpio_pin;
    p->function = func;
    p->pull = pull;
    p->drive_strength = drive;
    p->active = 1;
    table->pin_count++;
    return table->pin_count - 1;
}

int dt_pinmux_apply_to_dt(device_tree_t *dt, const dt_pinmux_table_t *table)
{
    if (!dt || !table) return -1;
    printf("[PINMUX] Applying %d pin configurations to device tree\n", table->pin_count);
    for (int i = 0; i < table->pin_count; i++) {
        const dt_pinmux_t *pin = &table->pins[i];
        char node_name[DT_NAME_MAX];
        snprintf(node_name, DT_NAME_MAX, "pinmux_%d_%d", pin->gpio_chip, pin->gpio_pin);
        int idx = dt_node_add(dt, dt->root_index, node_name);
        if (idx >= 0) {
            dt_prop_add_u32(dt, idx, "gpio-chip", (uint32_t)pin->gpio_chip);
            dt_prop_add_u32(dt, idx, "gpio-pin", (uint32_t)pin->gpio_pin);
            dt_prop_add_u32(dt, idx, "function", pin->function);
            dt_prop_add_u32(dt, idx, "bias-pull-up", pin->pull);
            dt_prop_add_u32(dt, idx, "drive-strength", pin->drive_strength);
        }
    }
    return 0;
}

int dt_pinmux_generate_overlay(const dt_pinmux_table_t *table, const char *dtbo_path)
{
    if (!table || !dtbo_path) return -1;
    printf("[PINMUX] Generating pinmux overlay: %s\n", dtbo_path);
    return 0;
}

void dt_dump(const device_tree_t *dt)
{
    if (!dt) return;
    printf("========== Device Tree ==========\n");
    printf("Nodes: %d  Phandles: %d  DTB size: %u\n", dt->node_count, dt->phandle_count, dt->dtb_size);
    dt_node_dump(dt, dt->root_index, 0);
}

void dt_node_dump(const device_tree_t *dt, int node, int indent)
{
    if (!dt || node < 0 || node >= dt->node_count) return;
    const dt_node_t *n = &dt->nodes[node];
    indent_print(indent);
    printf("%s {\n", n->name);
    if (n->phandle) { indent_print(indent + 1); printf("phandle = <%d>;\n", n->phandle); }
    for (int i = 0; i < n->prop_count; i++) {
        const dt_property_t *p = &n->properties[i];
        indent_print(indent + 1);
        switch (p->type) {
            case DT_PROP_U32:    printf("%s = <%u>;\n", p->name, p->value.u32); break;
            case DT_PROP_STRING: printf("%s = \"%s\";\n", p->name, p->value.string); break;
            case DT_PROP_BYTES:  printf("%s = [%d bytes];\n", p->name, p->len); break;
            default:             printf("%s;\n", p->name); break;
        }
    }
    for (int i = 0; i < n->child_count; i++) {
        dt_node_dump(dt, n->child_indices[i], indent + 1);
    }
    indent_print(indent);
    printf("}\n");
}

void dt_overlay_dump(const dt_overlay_t *overlay)
{
    if (!overlay) return;
    printf("========== DT Overlay ==========\n");
    printf("Fragments: %d  Overlay nodes: %d\n", overlay->fragment_count, overlay->overlay_node_count);
    for (int f = 0; f < overlay->fragment_count; f++) {
        printf("  Fragment %d: target=%s\n", f, overlay->fragments[f].target_path);
        for (int n = 0; n < overlay->fragments[f].fragment_count; n++) {
            int idx = overlay->fragments[f].fragment_nodes[n];
            printf("    + %s\n", overlay->overlay_nodes[idx].name);
        }
    }
}

void dt_pinmux_dump(const dt_pinmux_table_t *table)
{
    if (!table) return;
    printf("========== Pinmux Table ==========\n");
    printf("Pins: %d\n", table->pin_count);
    for (int i = 0; i < table->pin_count; i++) {
        const dt_pinmux_t *p = &table->pins[i];
        printf("  GPIO%d[%d]: func=%u pull=%u drive=%u\n",
               p->gpio_chip, p->gpio_pin, p->function, p->pull, p->drive_strength);
    }
}
