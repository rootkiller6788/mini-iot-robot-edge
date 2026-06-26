#ifndef MINI_ROBOTICS_ROS_CORE_H
#define MINI_ROBOTICS_ROS_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define MR_ROS_MAX_TOPICS 256
#define MR_ROS_MAX_NODES 128
#define MR_ROS_MAX_SERVICES 64
#define MR_ROS_MAX_ACTIONS 32
#define MR_ROS_MAX_MSG_SIZE 65536
#define MR_ROS_MAX_TOPIC_NAME 128
#define MR_ROS_MAX_TYPE_NAME 128
#define MR_ROS_MAX_NODE_NAME 64

typedef enum {
    MR_MSG_INT8 = 0,
    MR_MSG_UINT8,
    MR_MSG_INT16,
    MR_MSG_UINT16,
    MR_MSG_INT32,
    MR_MSG_UINT32,
    MR_MSG_INT64,
    MR_MSG_UINT64,
    MR_MSG_FLOAT32,
    MR_MSG_FLOAT64,
    MR_MSG_STRING,
    MR_MSG_BOOL,
    MR_MSG_BYTES,
    MR_MSG_ARRAY
} mr_msg_type_enum;

typedef struct {
    int msg_type;
    int elem_type;
    int array_len;
    char name[64];
} mr_msg_field;

typedef struct {
    char name[MR_ROS_MAX_TOPIC_NAME];
    char type[MR_ROS_MAX_TYPE_NAME];
    mr_msg_field fields[32];
    int num_fields;
} mr_msg_definition;

typedef struct {
    uint8_t data[MR_ROS_MAX_MSG_SIZE];
    int length;
    mr_msg_definition *def;
} mr_message;

typedef struct {
    char name[MR_ROS_MAX_TOPIC_NAME];
    char type[MR_ROS_MAX_TYPE_NAME];
    mr_msg_definition *def;
    int subscriber_count;
    int publisher_count;
} mr_topic;

typedef struct {
    mr_topic topics[MR_ROS_MAX_TOPICS];
    int num_topics;
} mr_master;

typedef struct {
    char name[MR_ROS_MAX_NODE_NAME];
    char *subscribed_topics[MR_ROS_MAX_TOPICS];
    int num_subscriptions;
    char *published_topics[MR_ROS_MAX_TOPICS];
    int num_publications;
    void (*callbacks[MR_ROS_MAX_TOPICS])(const mr_message *msg);
    int num_callbacks;
    int running;
} mr_node;

typedef void (*mr_subscriber_cb)(const mr_message *msg);
typedef void (*mr_service_cb)(const mr_message *req, mr_message *resp);
typedef void (*mr_action_feedback_cb)(const mr_message *feedback);
typedef void (*mr_action_done_cb)(const mr_message *result);

typedef struct {
    char name[MR_ROS_MAX_TOPIC_NAME];
    mr_service_cb callback;
} mr_service_server;

typedef struct {
    char name[MR_ROS_MAX_TOPIC_NAME];
    mr_msg_definition *req_def;
    mr_msg_definition *resp_def;
    mr_action_feedback_cb feedback_cb;
    mr_action_done_cb done_cb;
    int active;
} mr_action_server;

typedef struct {
    char name[MR_ROS_MAX_TOPIC_NAME];
    mr_action_feedback_cb feedback_cb;
    mr_action_done_cb done_cb;
} mr_action_client;

typedef struct {
    char parent_frame[64];
    char child_frame[64];
    double translation[3];
    double rotation[4]; /* quaternion: x,y,z,w */
    uint64_t timestamp;
} mr_tf_transform;

typedef struct {
    mr_tf_transform transforms[MR_ROS_MAX_TOPICS];
    int num_transforms;
} mr_tf_tree;

typedef struct {
    char topic_name[MR_ROS_MAX_TOPIC_NAME];
    mr_msg_definition *def;
    struct {
        uint64_t timestamp;
        uint8_t data[MR_ROS_MAX_MSG_SIZE];
        int length;
    } messages[1024];
    int num_messages;
    FILE *file;
} mr_bag_writer;

typedef struct {
    char topic_name[MR_ROS_MAX_TOPIC_NAME];
    mr_msg_definition *def;
    int index;
    FILE *file;
} mr_bag_reader;

typedef struct {
    char pkg_name[64];
    char node_name[MR_ROS_MAX_NODE_NAME];
    char executable[256];
    char args[512];
    char remap_from[MR_ROS_MAX_TOPIC_NAME][16];
    char remap_to[MR_ROS_MAX_TOPIC_NAME][16];
    int num_remaps;
    double respawn_delay;
    int required;
} mr_launch_node;

typedef struct {
    mr_launch_node nodes[MR_ROS_MAX_NODES];
    int num_nodes;
} mr_launch_file;

void mr_master_init(mr_master *m);

int mr_master_register_topic(mr_master *m, const char *name,
                             const char *type);

int mr_master_find_topic(const mr_master *m, const char *name);

int mr_master_unregister_topic(mr_master *m, const char *name);

void mr_node_init(mr_node *node, const char *name);

void mr_node_spin(mr_node *node);

void mr_node_shutdown(mr_node *node);

int mr_node_subscribe(mr_node *node, const char *topic_name,
                      mr_subscriber_cb cb);

int mr_node_publish(mr_node *node, const char *topic_name,
                    const mr_message *msg);

int mr_node_call_service(mr_node *node, const char *service_name,
                         const mr_message *req, mr_message *resp);

int mr_node_advertise_service(mr_node *node, const char *service_name,
                              mr_service_cb cb);

int mr_node_send_action_goal(mr_node *node, const char *action_name,
                             const mr_message *goal);

int mr_node_register_action(mr_node *node, const char *action_name,
                            mr_action_feedback_cb fb_cb,
                            mr_action_done_cb done_cb);

void mr_message_init(mr_message *msg, mr_msg_definition *def);

void mr_message_serialize(const mr_message *msg, uint8_t *buf, int *len);

int mr_message_deserialize(const uint8_t *buf, int len, mr_message *msg);

void mr_message_set_float32(mr_message *msg, int field_idx, float v);

float mr_message_get_float32(const mr_message *msg, int field_idx);

void mr_message_set_string(mr_message *msg, int field_idx, const char *v);

const char *mr_message_get_string(const mr_message *msg, int field_idx);

void mr_bag_writer_open(mr_bag_writer *bag, const char *filename);

void mr_bag_writer_write(mr_bag_writer *bag, const char *topic,
                         const mr_message *msg, uint64_t timestamp);

void mr_bag_writer_close(mr_bag_writer *bag);

int mr_bag_reader_open(mr_bag_reader *bag, const char *filename);

int mr_bag_reader_next(mr_bag_reader *bag, mr_message *msg, uint64_t *ts);

void mr_bag_reader_close(mr_bag_reader *bag);

void mr_tf_tree_init(mr_tf_tree *tree);

int mr_tf_tree_add_transform(mr_tf_tree *tree, const char *parent,
                             const char *child, const double *trans,
                             const double *rot);

int mr_tf_tree_lookup(const mr_tf_tree *tree, const char *parent,
                      const char *child, double *trans, double *rot);

void mr_launch_file_parse(const char *xml_data, mr_launch_file *lf);

int mr_launch_file_spawn_all(const mr_launch_file *lf);

#endif
