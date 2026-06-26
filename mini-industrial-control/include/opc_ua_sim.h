#ifndef OPC_UA_SIM_H
#define OPC_UA_SIM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define OPCUA_MAX_NODES            256
#define OPCUA_MAX_BROWSE_RESULTS   64
#define OPCUA_MAX_SUBSCRIPTIONS    32
#define OPCUA_MAX_MONITORED_ITEMS  128
#define OPCUA_MAX_NAME_LEN         64
#define OPCUA_NAMESPACE_DEFAULT    0
#define OPCUA_PUBLISH_INTERVAL_MS  100

typedef enum {
    OPCUA_NODE_OBJECT   = 0,
    OPCUA_NODE_VARIABLE = 1,
    OPCUA_NODE_METHOD   = 2,
    OPCUA_NODE_VIEW     = 3,
    OPCUA_NODE_DATATYPE = 4,
    OPCUA_NODE_REFERENCE = 5
} opcua_node_class_t;

typedef enum {
    OPCUA_TYPE_NULL      = 0,
    OPCUA_TYPE_BOOLEAN   = 1,
    OPCUA_TYPE_SBYTE     = 2,
    OPCUA_TYPE_BYTE      = 3,
    OPCUA_TYPE_INT16     = 4,
    OPCUA_TYPE_UINT16    = 5,
    OPCUA_TYPE_INT32     = 6,
    OPCUA_TYPE_UINT32    = 7,
    OPCUA_TYPE_INT64     = 8,
    OPCUA_TYPE_UINT64    = 9,
    OPCUA_TYPE_FLOAT     = 10,
    OPCUA_TYPE_DOUBLE    = 11,
    OPCUA_TYPE_STRING    = 12,
    OPCUA_TYPE_BYTESTRING = 13
} opcua_data_type_t;

typedef enum {
    OPCUA_ACCESS_READ   = 1,
    OPCUA_ACCESS_WRITE  = 2,
    OPCUA_ACCESS_RW     = 3
} opcua_access_level_t;

typedef union {
    bool     b;
    int8_t   s8;
    uint8_t  u8;
    int16_t  s16;
    uint16_t u16;
    int32_t  s32;
    uint32_t u32;
    int64_t  s64;
    uint64_t u64;
    float    f;
    double   d;
    char     str[OPCUA_MAX_NAME_LEN];
} opcua_variant_value_t;

typedef struct {
    opcua_data_type_t      type;
    opcua_variant_value_t  value;
    uint32_t               array_length;
    bool                   is_array;
} opcua_variant_t;

typedef struct {
    uint32_t              namespace_index;
    uint32_t              identifier;
    opcua_node_class_t    node_class;
    char                  display_name[OPCUA_MAX_NAME_LEN];
    char                  description[OPCUA_MAX_NAME_LEN];
    opcua_variant_t       value;
    opcua_data_type_t     data_type;
    opcua_access_level_t  access_level;
    uint32_t              parent_node_id;
    uint32_t              num_children;
    uint32_t              children[OPCUA_MAX_BROWSE_RESULTS];
    bool                  active;
} opcua_node_t;

typedef struct {
    uint32_t namespace_index;
    uint32_t identifier;
} opcua_node_id_t;

typedef struct {
    uint32_t              subscription_id;
    double                publishing_interval_ms;
    uint32_t              num_monitored_items;
    uint32_t              monitored_item_ids[OPCUA_MAX_MONITORED_ITEMS];
    double                sampling_interval_ms;
    bool                  enabled;
    uint32_t              keep_alive_count;
    uint32_t              lifetime_count;
} opcua_subscription_t;

typedef struct {
    uint32_t              monitored_item_id;
    uint32_t              subscription_id;
    opcua_node_id_t       node_id;
    uint32_t              attribute_id;
    double                sampling_interval_ms;
    uint32_t              queue_size;
    bool                  discard_oldest;
    opcua_variant_t       last_value;
    uint64_t              last_timestamp;
    bool                  triggered;
} opcua_monitored_item_t;

typedef struct {
    opcua_node_t            nodes[OPCUA_MAX_NODES];
    uint32_t                num_nodes;
    opcua_subscription_t    subscriptions[OPCUA_MAX_SUBSCRIPTIONS];
    uint32_t                num_subscriptions;
    opcua_monitored_item_t  monitored_items[OPCUA_MAX_MONITORED_ITEMS];
    uint32_t                num_monitored_items;
    uint32_t                next_node_id;
    uint32_t                next_subscription_id;
    uint32_t                next_monitored_item_id;
    bool                    server_running;
} opcua_address_space_t;

void opcua_init_address_space(opcua_address_space_t *as);

uint32_t opcua_add_object_node(opcua_address_space_t *as,
                               const char *name, uint32_t parent_id);
uint32_t opcua_add_variable_node(opcua_address_space_t *as,
                                 const char *name, uint32_t parent_id,
                                 opcua_data_type_t data_type,
                                 opcua_access_level_t access);
uint32_t opcua_add_method_node(opcua_address_space_t *as,
                               const char *name, uint32_t parent_id);

opcua_node_t* opcua_find_node(opcua_address_space_t *as, uint32_t node_id);
opcua_node_t* opcua_find_node_by_name(opcua_address_space_t *as, const char *name);

int opcua_browse_node(opcua_address_space_t *as, uint32_t node_id,
                      opcua_node_id_t *results, int max_results);

bool opcua_read_value(opcua_address_space_t *as, uint32_t node_id,
                      opcua_variant_t *value);
bool opcua_write_value(opcua_address_space_t *as, uint32_t node_id,
                       const opcua_variant_t *value);

bool opcua_set_bool_value(opcua_address_space_t *as, uint32_t node_id, bool val);
bool opcua_set_int32_value(opcua_address_space_t *as, uint32_t node_id, int32_t val);
bool opcua_set_double_value(opcua_address_space_t *as, uint32_t node_id, double val);
bool opcua_set_string_value(opcua_address_space_t *as, uint32_t node_id, const char *val);

bool opcua_get_bool_value(opcua_address_space_t *as, uint32_t node_id, bool *val);
bool opcua_get_int32_value(opcua_address_space_t *as, uint32_t node_id, int32_t *val);
bool opcua_get_double_value(opcua_address_space_t *as, uint32_t node_id, double *val);

uint32_t opcua_create_subscription(opcua_address_space_t *as,
                                   double publishing_interval_ms);
bool     opcua_delete_subscription(opcua_address_space_t *as, uint32_t sub_id);

uint32_t opcua_add_monitored_item(opcua_address_space_t *as,
                                  uint32_t subscription_id,
                                  uint32_t node_id,
                                  double sampling_interval_ms);
bool     opcua_remove_monitored_item(opcua_address_space_t *as,
                                     uint32_t monitored_item_id);

bool opcua_check_data_change(opcua_address_space_t *as,
                             uint32_t monitored_item_id,
                             opcua_variant_t *new_value);
int  opcua_check_all_monitored_items(opcua_address_space_t *as,
                                     uint32_t *changed_item_ids,
                                     int max_results);

void opcua_update_monitored_items(opcua_address_space_t *as, uint64_t current_time_ms);

const char* opcua_node_class_string(opcua_node_class_t node_class);
const char* opcua_data_type_string(opcua_data_type_t data_type);

#endif /* OPC_UA_SIM_H */
