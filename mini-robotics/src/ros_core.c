#include "ros_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void mr_master_init(mr_master *m)
{
    memset(m, 0, sizeof(mr_master));
}

int mr_master_register_topic(mr_master *m, const char *name,
                             const char *type)
{
    if (m->num_topics >= MR_ROS_MAX_TOPICS) return -1;
    int idx = m->num_topics;
    strncpy(m->topics[idx].name, name, MR_ROS_MAX_TOPIC_NAME - 1);
    strncpy(m->topics[idx].type, type, MR_ROS_MAX_TYPE_NAME - 1);
    m->topics[idx].subscriber_count = 0;
    m->topics[idx].publisher_count = 0;
    m->num_topics++;
    return idx;
}

int mr_master_find_topic(const mr_master *m, const char *name)
{
    int i;
    for (i = 0; i < m->num_topics; i++)
        if (strcmp(m->topics[i].name, name) == 0)
            return i;
    return -1;
}

int mr_master_unregister_topic(mr_master *m, const char *name)
{
    int idx = mr_master_find_topic(m, name);
    if (idx < 0) return 0;
    if (idx < m->num_topics - 1)
        m->topics[idx] = m->topics[m->num_topics - 1];
    m->num_topics--;
    return 1;
}

void mr_node_init(mr_node *node, const char *name)
{
    memset(node, 0, sizeof(mr_node));
    strncpy(node->name, name, MR_ROS_MAX_NODE_NAME - 1);
    node->running = 0;
}

void mr_node_spin(mr_node *node)
{
    node->running = 1;
    while (node->running) {
        int cb;
        for (cb = 0; cb < node->num_callbacks; cb++) {
            mr_message dummy;
            memset(&dummy, 0, sizeof(dummy));
            if (node->callbacks[cb])
                node->callbacks[cb](&dummy);
        }
    }
}

void mr_node_shutdown(mr_node *node)
{
    node->running = 0;
}

int mr_node_subscribe(mr_node *node, const char *topic_name,
                      mr_subscriber_cb cb)
{
    if (node->num_subscriptions >= MR_ROS_MAX_TOPICS) return -1;
    int idx = node->num_subscriptions;
    node->subscribed_topics[idx] = strdup(topic_name);
    node->callbacks[idx] = (void (*)(const mr_message *))cb;
    node->num_subscriptions++;
    node->num_callbacks++;
    return idx;
}

int mr_node_publish(mr_node *node, const char *topic_name,
                    const mr_message *msg)
{
    if (node->num_publications >= MR_ROS_MAX_TOPICS) return -1;
    int idx = node->num_publications;
    node->published_topics[idx] = strdup(topic_name);
    node->num_publications++;
    (void)msg;
    return idx;
}

int mr_node_call_service(mr_node *node, const char *service_name,
                         const mr_message *req, mr_message *resp)
{
    (void)node;
    (void)service_name;
    (void)req;
    (void)resp;
    return 0;
}

int mr_node_advertise_service(mr_node *node, const char *service_name,
                              mr_service_cb cb)
{
    (void)node;
    (void)service_name;
    (void)cb;
    return 0;
}

int mr_node_send_action_goal(mr_node *node, const char *action_name,
                             const mr_message *goal)
{
    (void)node;
    (void)action_name;
    (void)goal;
    return 0;
}

int mr_node_register_action(mr_node *node, const char *action_name,
                            mr_action_feedback_cb fb_cb,
                            mr_action_done_cb done_cb)
{
    (void)node;
    (void)action_name;
    (void)fb_cb;
    (void)done_cb;
    return 0;
}

void mr_message_init(mr_message *msg, mr_msg_definition *def)
{
    memset(msg, 0, sizeof(mr_message));
    msg->def = def;
    msg->length = 0;
}

void mr_message_serialize(const mr_message *msg, uint8_t *buf, int *len)
{
    if (msg->length < MR_ROS_MAX_MSG_SIZE) {
        memcpy(buf, msg->data, (size_t)msg->length);
        *len = msg->length;
    } else {
        *len = 0;
    }
}

int mr_message_deserialize(const uint8_t *buf, int len, mr_message *msg)
{
    if (len > MR_ROS_MAX_MSG_SIZE) return 0;
    memcpy(msg->data, buf, (size_t)len);
    msg->length = len;
    return 1;
}

void mr_message_set_float32(mr_message *msg, int field_idx, float v)
{
    if (field_idx >= 0 && (field_idx + 1) * 4 <= MR_ROS_MAX_MSG_SIZE) {
        memcpy(&msg->data[field_idx * 4], &v, sizeof(float));
        msg->length = (field_idx + 1) * 4;
    }
}

float mr_message_get_float32(const mr_message *msg, int field_idx)
{
    float v = 0.0f;
    if (field_idx >= 0 && (field_idx + 1) * 4 <= msg->length)
        memcpy(&v, &msg->data[field_idx * 4], sizeof(float));
    return v;
}

void mr_message_set_string(mr_message *msg, int field_idx, const char *v)
{
    int off = field_idx * 64;
    if (off >= 0 && off + 64 <= MR_ROS_MAX_MSG_SIZE) {
        strncpy((char *)&msg->data[off], v, 63);
        msg->data[off + 63] = 0;
        msg->length = off + 64;
    }
}

const char *mr_message_get_string(const mr_message *msg, int field_idx)
{
    int off = field_idx * 64;
    if (off >= 0 && off + 64 <= msg->length)
        return (const char *)&msg->data[off];
    return "";
}

void mr_bag_writer_open(mr_bag_writer *bag, const char *filename)
{
    bag->file = fopen(filename, "wb");
    bag->num_messages = 0;
}

void mr_bag_writer_write(mr_bag_writer *bag, const char *topic,
                         const mr_message *msg, uint64_t timestamp)
{
    if (!bag->file) return;
    uint32_t topic_len = (uint32_t)strlen(topic);
    fwrite(&timestamp, sizeof(uint64_t), 1, bag->file);
    fwrite(&topic_len, sizeof(uint32_t), 1, bag->file);
    fwrite(topic, 1, topic_len, bag->file);
    uint32_t msg_len = (uint32_t)msg->length;
    fwrite(&msg_len, sizeof(uint32_t), 1, bag->file);
    fwrite(msg->data, 1, msg_len, bag->file);
    bag->num_messages++;
}

void mr_bag_writer_close(mr_bag_writer *bag)
{
    if (bag->file) {
        fclose(bag->file);
        bag->file = NULL;
    }
}

int mr_bag_reader_open(mr_bag_reader *bag, const char *filename)
{
    bag->file = fopen(filename, "rb");
    bag->index = 0;
    return bag->file ? 1 : 0;
}

int mr_bag_reader_next(mr_bag_reader *bag, mr_message *msg, uint64_t *ts)
{
    if (!bag->file || feof(bag->file)) return 0;
    uint32_t topic_len;
    if (fread(ts, sizeof(uint64_t), 1, bag->file) != 1) return 0;
    if (fread(&topic_len, sizeof(uint32_t), 1, bag->file) != 1) return 0;
    char topic_name[256] = {0};
    if (fread(topic_name, 1, topic_len, bag->file) != topic_len) return 0;
    uint32_t msg_len;
    if (fread(&msg_len, sizeof(uint32_t), 1, bag->file) != 1) return 0;
    if (fread(msg->data, 1, msg_len, bag->file) != msg_len) return 0;
    msg->length = (int)msg_len;
    bag->index++;
    (void)topic_name;
    return 1;
}

void mr_bag_reader_close(mr_bag_reader *bag)
{
    if (bag->file) {
        fclose(bag->file);
        bag->file = NULL;
    }
}

void mr_tf_tree_init(mr_tf_tree *tree)
{
    memset(tree, 0, sizeof(mr_tf_tree));
}

int mr_tf_tree_add_transform(mr_tf_tree *tree, const char *parent,
                             const char *child, const double *trans,
                             const double *rot)
{
    if (tree->num_transforms >= MR_ROS_MAX_TOPICS) return -1;
    int idx = tree->num_transforms;
    strncpy(tree->transforms[idx].parent_frame, parent, 63);
    strncpy(tree->transforms[idx].child_frame, child, 63);
    memcpy(tree->transforms[idx].translation, trans, 3 * sizeof(double));
    memcpy(tree->transforms[idx].rotation, rot, 4 * sizeof(double));
    tree->transforms[idx].timestamp = (uint64_t)time(NULL);
    tree->num_transforms++;
    return idx;
}

int mr_tf_tree_lookup(const mr_tf_tree *tree, const char *parent,
                      const char *child, double *trans, double *rot)
{
    int i;
    for (i = 0; i < tree->num_transforms; i++) {
        if (strcmp(tree->transforms[i].parent_frame, parent) == 0
            && strcmp(tree->transforms[i].child_frame, child) == 0) {
            memcpy(trans, tree->transforms[i].translation,
                   3 * sizeof(double));
            memcpy(rot, tree->transforms[i].rotation,
                   4 * sizeof(double));
            return 1;
        }
    }
    return 0;
}

void mr_launch_file_parse(const char *xml_data, mr_launch_file *lf)
{
    memset(lf, 0, sizeof(mr_launch_file));
    const char *p = xml_data;
    while (*p) {
        if (strncmp(p, "<node ", 6) == 0) {
            int idx = lf->num_nodes;
            const char *name = strstr(p, "name=\"");
            if (name) {
                name += 6;
                int n = 0;
                while (name[n] && name[n] != '"' && n < 63) {
                    lf->nodes[idx].node_name[n] = name[n];
                    n++;
                }
            }
            const char *pkg = strstr(p, "pkg=\"");
            if (pkg) {
                pkg += 5;
                int n = 0;
                while (pkg[n] && pkg[n] != '"' && n < 63) {
                    lf->nodes[idx].pkg_name[n] = pkg[n];
                    n++;
                }
            }
            lf->num_nodes++;
        }
        p++;
    }
}

int mr_launch_file_spawn_all(const mr_launch_file *lf)
{
    int count = 0;
    int i;
    for (i = 0; i < lf->num_nodes; i++) {
        count++;
    }
    return count;
}
