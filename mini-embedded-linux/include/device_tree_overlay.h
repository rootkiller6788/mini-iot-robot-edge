#ifndef DEVICE_TREE_OVERLAY_H
#define DEVICE_TREE_OVERLAY_H

#include <stdint.h>
#include <stddef.h>

#define DT_MAX_NODES          256
#define DT_MAX_PROPERTIES     128
#define DT_MAX_PHANDLES       64
#define DT_MAX_FRAGMENTS      32
#define DT_MAX_OVERLAYS       16
#define DT_NAME_MAX           64
#define DT_PATH_MAX           256
#define DT_VALUE_MAX          512
#define DT_PINMUX_MAX         64

#define DT_MAGIC              0xD00DFEED
#define DT_VERSION            17
#define DT_COMPAT_VERSION     16

#define FDT_BEGIN_NODE        0x00000001
#define FDT_END_NODE          0x00000002
#define FDT_PROP              0x00000003
#define FDT_NOP               0x00000004
#define FDT_END               0x00000009

typedef enum {
    DT_PROP_EMPTY,
    DT_PROP_U32,
    DT_PROP_U64,
    DT_PROP_STRING,
    DT_PROP_STRING_LIST,
    DT_PROP_BYTES,
    DT_PROP_PHANDLE,
    DT_PROP_COUNT
} dt_prop_type_t;

typedef enum {
    DT_OVERLAY_FIXUP,
    DT_OVERLAY_MERGE,
    DT_OVERLAY_FRAGMENT,
    DT_OVERLAY_COUNT
} dt_overlay_type_t;

typedef struct {
    char   name[DT_NAME_MAX];
    int    len;
    union {
        uint32_t  u32;
        uint64_t  u64;
        char      string[DT_VALUE_MAX];
        uint8_t   bytes[DT_VALUE_MAX];
    } value;
    dt_prop_type_t type;
} dt_property_t;

typedef struct {
    char           name[DT_NAME_MAX];
    char           full_path[DT_PATH_MAX];
    dt_property_t  properties[DT_MAX_PROPERTIES];
    int            prop_count;
    int            phandle;
    int            parent_index;
    int            child_indices[DT_MAX_NODES];
    int            child_count;
    uint8_t        is_overlay;
} dt_node_t;

typedef struct {
    uint32_t phandle;
    int      node_index;
    uint32_t target_phandle;
} dt_phandle_entry_t;

typedef struct {
    int     target_node;
    char    target_path[DT_PATH_MAX];
    int     fragment_nodes[DT_MAX_NODES];
    int     fragment_count;
} dt_fragment_t;

typedef struct {
    dt_node_t       nodes[DT_MAX_NODES];
    int             node_count;
    int             root_index;
    dt_phandle_entry_t phandles[DT_MAX_PHANDLES];
    int             phandle_count;
    uint32_t        boot_cpuid_phys;
    uint32_t        dtb_size;
    uint8_t         header_ok;
} device_tree_t;

typedef struct {
    dt_fragment_t fragments[DT_MAX_FRAGMENTS];
    int           fragment_count;
    dt_node_t     overlay_nodes[DT_MAX_NODES];
    int           overlay_node_count;
} dt_overlay_t;

typedef struct {
    char     compatible[64];
    int      gpio_chip;
    int      gpio_pin;
    uint32_t function;
    uint32_t pull;
    uint32_t drive_strength;
    uint8_t  active;
} dt_pinmux_t;

typedef struct {
    dt_pinmux_t pins[DT_PINMUX_MAX];
    int         pin_count;
} dt_pinmux_table_t;

int   dt_init(device_tree_t *dt);
int   dt_node_add(device_tree_t *dt, int parent, const char *name);
int   dt_node_find(device_tree_t *dt, const char *path);
int   dt_node_find_by_phandle(device_tree_t *dt, uint32_t phandle);

int   dt_prop_add_u32(device_tree_t *dt, int node, const char *name, uint32_t val);
int   dt_prop_add_string(device_tree_t *dt, int node, const char *name, const char *val);
int   dt_prop_add_bytes(device_tree_t *dt, int node, const char *name, const uint8_t *val, int len);
int   dt_prop_get_u32(device_tree_t *dt, int node, const char *name, uint32_t *val);
int   dt_prop_get_string(device_tree_t *dt, int node, const char *name, char *out, int maxlen);

int   dt_phandle_assign(device_tree_t *dt, int node);
int   dt_phandle_resolve(device_tree_t *dt, int node, uint32_t *phandle);

int   dt_compile_dts(const char *dts_path, const char *dtb_path);
int   dt_decompile_dtb(const char *dtb_path, const char *dts_path);
int   dt_load_dtb(device_tree_t *dt, const char *dtb_path);
int   dt_write_dtb(device_tree_t *dt, const char *dtb_path);

int   dt_overlay_init(dt_overlay_t *overlay);
int   dt_overlay_add_fragment(dt_overlay_t *overlay, const char *target_path);
int   dt_overlay_add_node(dt_overlay_t *overlay, int frag_idx, const char *name);
int   dt_overlay_compile(dt_overlay_t *overlay, const char *dtbo_path);
int   dt_overlay_load(dt_overlay_t *overlay, const char *dtbo_path);

int   dt_apply_overlay(device_tree_t *base_dt, const dt_overlay_t *overlay);
int   dt_apply_overlays(device_tree_t *base_dt, const dt_overlay_t *overlays, int count);
int   dt_merge_overlay_node(device_tree_t *base_dt, int base_node, int overlay_node);

int   dt_pinmux_setup(dt_pinmux_table_t *table, const char *compatible);
int   dt_pinmux_add(dt_pinmux_table_t *table, int gpio_chip, int gpio_pin,
                    uint32_t func, uint32_t pull, uint32_t drive);
int   dt_pinmux_apply_to_dt(device_tree_t *dt, const dt_pinmux_table_t *table);
int   dt_pinmux_generate_overlay(const dt_pinmux_table_t *table, const char *dtbo_path);

void  dt_dump(const device_tree_t *dt);
void  dt_node_dump(const device_tree_t *dt, int node, int indent);
void  dt_overlay_dump(const dt_overlay_t *overlay);
void  dt_pinmux_dump(const dt_pinmux_table_t *table);

#endif
