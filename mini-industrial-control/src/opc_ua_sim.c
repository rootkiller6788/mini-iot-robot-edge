#include "opc_ua_sim.h"
#include <string.h>
#include <stdio.h>

void opcua_init_address_space(opcua_address_space_t *as)
{
    memset(as, 0, sizeof(opcua_address_space_t));
    as->next_node_id = 1;
    as->next_subscription_id = 1;
    as->next_monitored_item_id = 1;
    as->server_running = false;

    uint32_t root_id = opcua_add_object_node(as, "Root", 0);
    (void)root_id;
}

uint32_t opcua_add_object_node(opcua_address_space_t *as,
                               const char *name, uint32_t parent_id)
{
    if (!as || !name || as->num_nodes >= OPCUA_MAX_NODES) return 0;

    uint32_t id = as->next_node_id++;
    opcua_node_t *node = &as->nodes[as->num_nodes];
    memset(node, 0, sizeof(opcua_node_t));
    node->namespace_index = OPCUA_NAMESPACE_DEFAULT;
    node->identifier = id;
    node->node_class = OPCUA_NODE_OBJECT;
    strncpy(node->display_name, name, OPCUA_MAX_NAME_LEN - 1);
    node->display_name[OPCUA_MAX_NAME_LEN - 1] = '\0';
    node->parent_node_id = parent_id;
    node->active = true;

    if (parent_id > 0 && parent_id <= as->num_nodes) {
        opcua_node_t *parent = &as->nodes[parent_id - 1];
        if (parent->num_children < OPCUA_MAX_BROWSE_RESULTS) {
            parent->children[parent->num_children++] = id;
        }
    }

    as->num_nodes++;
    return id;
}

uint32_t opcua_add_variable_node(opcua_address_space_t *as,
                                 const char *name, uint32_t parent_id,
                                 opcua_data_type_t data_type,
                                 opcua_access_level_t access)
{
    if (!as || !name || as->num_nodes >= OPCUA_MAX_NODES) return 0;

    uint32_t id = as->next_node_id++;
    opcua_node_t *node = &as->nodes[as->num_nodes];
    memset(node, 0, sizeof(opcua_node_t));
    node->namespace_index = OPCUA_NAMESPACE_DEFAULT;
    node->identifier = id;
    node->node_class = OPCUA_NODE_VARIABLE;
    strncpy(node->display_name, name, OPCUA_MAX_NAME_LEN - 1);
    node->display_name[OPCUA_MAX_NAME_LEN - 1] = '\0';
    node->data_type = data_type;
    node->access_level = access;
    node->value.type = data_type;
    node->parent_node_id = parent_id;
    node->active = true;

    if (parent_id > 0 && parent_id <= as->num_nodes) {
        opcua_node_t *parent = &as->nodes[parent_id - 1];
        if (parent->num_children < OPCUA_MAX_BROWSE_RESULTS) {
            parent->children[parent->num_children++] = id;
        }
    }

    as->num_nodes++;
    return id;
}

uint32_t opcua_add_method_node(opcua_address_space_t *as,
                               const char *name, uint32_t parent_id)
{
    if (!as || !name || as->num_nodes >= OPCUA_MAX_NODES) return 0;

    uint32_t id = as->next_node_id++;
    opcua_node_t *node = &as->nodes[as->num_nodes];
    memset(node, 0, sizeof(opcua_node_t));
    node->namespace_index = OPCUA_NAMESPACE_DEFAULT;
    node->identifier = id;
    node->node_class = OPCUA_NODE_METHOD;
    strncpy(node->display_name, name, OPCUA_MAX_NAME_LEN - 1);
    node->display_name[OPCUA_MAX_NAME_LEN - 1] = '\0';
    node->parent_node_id = parent_id;
    node->active = true;

    if (parent_id > 0 && parent_id <= as->num_nodes) {
        opcua_node_t *parent = &as->nodes[parent_id - 1];
        if (parent->num_children < OPCUA_MAX_BROWSE_RESULTS) {
            parent->children[parent->num_children++] = id;
        }
    }

    as->num_nodes++;
    return id;
}

opcua_node_t* opcua_find_node(opcua_address_space_t *as, uint32_t node_id)
{
    if (!as || node_id == 0) return NULL;
    for (uint32_t i = 0; i < as->num_nodes; i++) {
        if (as->nodes[i].identifier == node_id) {
            return &as->nodes[i];
        }
    }
    return NULL;
}

opcua_node_t* opcua_find_node_by_name(opcua_address_space_t *as, const char *name)
{
    if (!as || !name) return NULL;
    for (uint32_t i = 0; i < as->num_nodes; i++) {
        if (strcmp(as->nodes[i].display_name, name) == 0) {
            return &as->nodes[i];
        }
    }
    return NULL;
}

int opcua_browse_node(opcua_address_space_t *as, uint32_t node_id,
                      opcua_node_id_t *results, int max_results)
{
    if (!as || !results || max_results <= 0) return 0;

    opcua_node_t *node = opcua_find_node(as, node_id);
    if (!node) return 0;

    int count = 0;
    for (uint32_t i = 0; i < node->num_children && count < max_results; i++) {
        uint32_t child_id = node->children[i];
        opcua_node_t *child = opcua_find_node(as, child_id);
        if (child && child->active) {
            results[count].namespace_index = child->namespace_index;
            results[count].identifier = child->identifier;
            count++;
        }
    }

    return count;
}

bool opcua_read_value(opcua_address_space_t *as, uint32_t node_id,
                      opcua_variant_t *value)
{
    opcua_node_t *node = opcua_find_node(as, node_id);
    if (!node || !value) return false;
    if (node->node_class != OPCUA_NODE_VARIABLE) return false;
    memcpy(value, &node->value, sizeof(opcua_variant_t));
    return true;
}

bool opcua_write_value(opcua_address_space_t *as, uint32_t node_id,
                       const opcua_variant_t *value)
{
    opcua_node_t *node = opcua_find_node(as, node_id);
    if (!node || !value) return false;
    if (node->node_class != OPCUA_NODE_VARIABLE) return false;
    if (!(node->access_level & OPCUA_ACCESS_WRITE)) return false;
    if (node->data_type != value->type) return false;
    memcpy(&node->value, value, sizeof(opcua_variant_t));
    return true;
}

bool opcua_set_bool_value(opcua_address_space_t *as, uint32_t node_id, bool val)
{
    opcua_variant_t v;
    memset(&v, 0, sizeof(v));
    v.type = OPCUA_TYPE_BOOLEAN;
    v.value.b = val;
    return opcua_write_value(as, node_id, &v);
}

bool opcua_set_int32_value(opcua_address_space_t *as, uint32_t node_id, int32_t val)
{
    opcua_variant_t v;
    memset(&v, 0, sizeof(v));
    v.type = OPCUA_TYPE_INT32;
    v.value.s32 = val;
    return opcua_write_value(as, node_id, &v);
}

bool opcua_set_double_value(opcua_address_space_t *as, uint32_t node_id, double val)
{
    opcua_variant_t v;
    memset(&v, 0, sizeof(v));
    v.type = OPCUA_TYPE_DOUBLE;
    v.value.d = val;
    return opcua_write_value(as, node_id, &v);
}

bool opcua_set_string_value(opcua_address_space_t *as, uint32_t node_id, const char *val)
{
    opcua_variant_t v;
    memset(&v, 0, sizeof(v));
    v.type = OPCUA_TYPE_STRING;
    strncpy(v.value.str, val, OPCUA_MAX_NAME_LEN - 1);
    v.value.str[OPCUA_MAX_NAME_LEN - 1] = '\0';
    return opcua_write_value(as, node_id, &v);
}

bool opcua_get_bool_value(opcua_address_space_t *as, uint32_t node_id, bool *val)
{
    opcua_variant_t v;
    memset(&v, 0, sizeof(v));
    if (!opcua_read_value(as, node_id, &v)) return false;
    if (v.type != OPCUA_TYPE_BOOLEAN) return false;
    *val = v.value.b;
    return true;
}

bool opcua_get_int32_value(opcua_address_space_t *as, uint32_t node_id, int32_t *val)
{
    opcua_variant_t v;
    memset(&v, 0, sizeof(v));
    if (!opcua_read_value(as, node_id, &v)) return false;
    if (v.type != OPCUA_TYPE_INT32) return false;
    *val = v.value.s32;
    return true;
}

bool opcua_get_double_value(opcua_address_space_t *as, uint32_t node_id, double *val)
{
    opcua_variant_t v;
    memset(&v, 0, sizeof(v));
    if (!opcua_read_value(as, node_id, &v)) return false;
    if (v.type != OPCUA_TYPE_DOUBLE) return false;
    *val = v.value.d;
    return true;
}

uint32_t opcua_create_subscription(opcua_address_space_t *as,
                                   double publishing_interval_ms)
{
    if (!as || as->num_subscriptions >= OPCUA_MAX_SUBSCRIPTIONS) return 0;

    uint32_t id = as->next_subscription_id++;
    opcua_subscription_t *sub = &as->subscriptions[as->num_subscriptions];
    memset(sub, 0, sizeof(opcua_subscription_t));
    sub->subscription_id = id;
    sub->publishing_interval_ms = publishing_interval_ms;
    sub->enabled = true;
    sub->keep_alive_count = 3;
    sub->lifetime_count = 10;

    as->num_subscriptions++;
    return id;
}

bool opcua_delete_subscription(opcua_address_space_t *as, uint32_t sub_id)
{
    if (!as) return false;

    for (uint32_t i = 0; i < as->num_subscriptions; i++) {
        if (as->subscriptions[i].subscription_id == sub_id) {
            for (uint32_t j = i; j < as->num_subscriptions - 1; j++) {
                as->subscriptions[j] = as->subscriptions[j + 1];
            }
            as->num_subscriptions--;
            return true;
        }
    }
    return false;
}

uint32_t opcua_add_monitored_item(opcua_address_space_t *as,
                                  uint32_t subscription_id,
                                  uint32_t node_id,
                                  double sampling_interval_ms)
{
    if (!as || as->num_monitored_items >= OPCUA_MAX_MONITORED_ITEMS) return 0;

    uint32_t id = as->next_monitored_item_id++;
    opcua_monitored_item_t *mi = &as->monitored_items[as->num_monitored_items];
    memset(mi, 0, sizeof(opcua_monitored_item_t));
    mi->monitored_item_id = id;
    mi->subscription_id = subscription_id;
    mi->node_id.namespace_index = 0;
    mi->node_id.identifier = node_id;
    mi->attribute_id = 13;
    mi->sampling_interval_ms = sampling_interval_ms;
    mi->queue_size = 10;
    mi->discard_oldest = true;

    as->num_monitored_items++;
    return id;
}

bool opcua_remove_monitored_item(opcua_address_space_t *as,
                                 uint32_t monitored_item_id)
{
    if (!as) return false;

    for (uint32_t i = 0; i < as->num_monitored_items; i++) {
        if (as->monitored_items[i].monitored_item_id == monitored_item_id) {
            for (uint32_t j = i; j < as->num_monitored_items - 1; j++) {
                as->monitored_items[j] = as->monitored_items[j + 1];
            }
            as->num_monitored_items--;
            return true;
        }
    }
    return false;
}

bool opcua_check_data_change(opcua_address_space_t *as,
                             uint32_t monitored_item_id,
                             opcua_variant_t *new_value)
{
    if (!as) return false;

    for (uint32_t i = 0; i < as->num_monitored_items; i++) {
        opcua_monitored_item_t *mi = &as->monitored_items[i];
        if (mi->monitored_item_id == monitored_item_id) {
            opcua_node_t *node = opcua_find_node(as, mi->node_id.identifier);
            if (!node) return false;

            bool changed = (memcmp(&mi->last_value, &node->value, sizeof(opcua_variant_t)) != 0);
            if (changed) {
                mi->last_value = node->value;
                mi->triggered = true;
                if (new_value) {
                    *new_value = node->value;
                }
                return true;
            }
            return false;
        }
    }
    return false;
}

int opcua_check_all_monitored_items(opcua_address_space_t *as,
                                    uint32_t *changed_item_ids,
                                    int max_results)
{
    if (!as || !changed_item_ids || max_results <= 0) return 0;

    int count = 0;
    for (uint32_t i = 0; i < as->num_monitored_items && count < max_results; i++) {
        opcua_variant_t new_val;
        memset(&new_val, 0, sizeof(new_val));
        if (opcua_check_data_change(as, as->monitored_items[i].monitored_item_id, &new_val)) {
            changed_item_ids[count++] = as->monitored_items[i].monitored_item_id;
        }
    }
    return count;
}

void opcua_update_monitored_items(opcua_address_space_t *as, uint64_t current_time_ms)
{
    if (!as) return;

    for (uint32_t i = 0; i < as->num_monitored_items; i++) {
        opcua_monitored_item_t *mi = &as->monitored_items[i];
        mi->last_timestamp = current_time_ms;
        (void)opcua_check_data_change(as, mi->monitored_item_id, NULL);
    }
}

const char* opcua_node_class_string(opcua_node_class_t node_class)
{
    switch (node_class) {
    case OPCUA_NODE_OBJECT:   return "Object";
    case OPCUA_NODE_VARIABLE: return "Variable";
    case OPCUA_NODE_METHOD:   return "Method";
    case OPCUA_NODE_VIEW:     return "View";
    case OPCUA_NODE_DATATYPE: return "DataType";
    case OPCUA_NODE_REFERENCE:return "Reference";
    default:                  return "Unknown";
    }
}

const char* opcua_data_type_string(opcua_data_type_t data_type)
{
    switch (data_type) {
    case OPCUA_TYPE_NULL:      return "Null";
    case OPCUA_TYPE_BOOLEAN:   return "Boolean";
    case OPCUA_TYPE_SBYTE:     return "SByte";
    case OPCUA_TYPE_BYTE:      return "Byte";
    case OPCUA_TYPE_INT16:     return "Int16";
    case OPCUA_TYPE_UINT16:    return "UInt16";
    case OPCUA_TYPE_INT32:     return "Int32";
    case OPCUA_TYPE_UINT32:    return "UInt32";
    case OPCUA_TYPE_INT64:     return "Int64";
    case OPCUA_TYPE_UINT64:    return "UInt64";
    case OPCUA_TYPE_FLOAT:     return "Float";
    case OPCUA_TYPE_DOUBLE:    return "Double";
    case OPCUA_TYPE_STRING:    return "String";
    case OPCUA_TYPE_BYTESTRING:return "ByteString";
    default:                   return "Unknown";
    }
}
