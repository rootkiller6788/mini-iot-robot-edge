#include "scada_collect.h"
#include "opc_ua_sim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void simulate_sensor_values(scada_collector_t *collector, int step)
{
    double t = (double)step * 0.1;
    double temp = 25.0 + 5.0 * sin(t);
    double pressure = 100.0 + 10.0 * cos(t * 0.7);
    double flow = 50.0 + 20.0 * sin(t * 0.3);
    double level = 75.0 + 15.0 * cos(t * 0.5);
    double speed = 1500.0 + 100.0 * sin(t * 0.2);

    scada_write_tag_value(collector, 0, temp);
    scada_write_tag_value(collector, 1, pressure);
    scada_write_tag_value(collector, 2, flow);
    scada_write_tag_value(collector, 3, level);
    scada_write_tag_value(collector, 4, speed);
}

int main(void)
{
    printf("=== SCADA Data Collection Demo ===\n\n");

    scada_collector_t collector;
    scada_init_collector(&collector);
    collector.collecting = true;

    /* Configure tags */
    printf("--- Configuring Tags ---\n");
    scada_add_tag(&collector, "Tank.Temperature", SCADA_TAG_ANALOG, 1, 40001, 1000);
    scada_add_tag(&collector, "Tank.Pressure", SCADA_TAG_ANALOG, 1, 40002, 1000);
    scada_add_tag(&collector, "Tank.Flow", SCADA_TAG_ANALOG, 1, 40003, 500);
    scada_add_tag(&collector, "Tank.Level", SCADA_TAG_ANALOG, 1, 40004, 1000);
    scada_add_tag(&collector, "Motor.Speed", SCADA_TAG_ANALOG, 1, 40005, 500);

    scada_tag_t *level_tag = scada_find_tag(&collector, "Tank.Level");
    scada_tag_t *temp_tag = scada_find_tag(&collector, "Tank.Temperature");
    scada_tag_t *speed_tag = scada_find_tag(&collector, "Motor.Speed");
    if (level_tag) scada_set_tag_deadband(level_tag, 1.0);
    if (temp_tag) scada_set_tag_deadband(temp_tag, 0.5);

    printf("Added %u tags\n", collector.num_tags);

    /* Configure alarms */
    printf("\n--- Configuring Alarms ---\n");
    if (temp_tag) {
        uint32_t temp_idx = (uint32_t)(temp_tag - collector.tags);
        scada_add_alarm(&collector, "Temp_Hi", temp_idx, SCADA_ALARM_HI, 28.0, 0.5);
        scada_add_alarm(&collector, "Temp_HiHi", temp_idx, SCADA_ALARM_HIHI, 32.0, 0.5);
        scada_add_alarm(&collector, "Temp_Lo", temp_idx, SCADA_ALARM_LO, 15.0, 0.5);
    }
    if (level_tag) {
        uint32_t level_idx = (uint32_t)(level_tag - collector.tags);
        scada_add_alarm(&collector, "Level_LoLo", level_idx, SCADA_ALARM_LOLO, 55.0, 1.0);
        scada_add_alarm(&collector, "Level_Hi", level_idx, SCADA_ALARM_HI, 92.0, 1.0);
    }
    printf("Added %u alarms\n", collector.num_alarms);

    /* Simulate data collection over time */
    printf("\n--- Simulating Data Collection (20 cycles) ---\n");
    for (int step = 0; step < 20; step++) {
        uint64_t now_ms = (uint64_t)(step * 500);

        simulate_sensor_values(&collector, step);
        scada_poll_all_tags(&collector, now_ms);
        scada_check_alarms(&collector, now_ms);

        if (step % 5 == 0) {
            printf("Cycle %2d | ", step);
            for (uint32_t i = 0; i < collector.num_tags && i < 3; i++) {
                double val;
                scada_read_tag(&collector, i, &val);
                printf("%s=%.1f  ", collector.tags[i].name, val);
            }
            printf("\n");
        }
    }

    /* Print active alarms */
    printf("\n--- Active Alarms ---\n");
    uint32_t active_ids[16];
    int active_count = scada_get_active_alarms(&collector, active_ids, 16);
    if (active_count > 0) {
        for (int i = 0; i < active_count; i++) {
            scada_alarm_t *alarm = &collector.alarms[active_ids[i]];
            printf("  [%u] %s: %s (limit=%.1f, tag[%u]=%.2f)\n",
                   active_ids[i], alarm->name,
                   scada_alarm_level_string(alarm->level),
                   alarm->limit, alarm->tag_index,
                   collector.tags[alarm->tag_index].current_value);
        }
    } else {
        printf("  No active alarms\n");
    }

    /* Query history */
    printf("\n--- History Query ---\n");
    scada_history_point_t results[50];
    int hist_count = scada_query_history(&collector, 0, 0, UINT64_MAX, results, 50);
    printf("History points for tag 0: %d\n", hist_count);

    /* Daily report */
    printf("\n");
    scada_daily_report_t report;
    scada_generate_daily_report(&collector, &report);
    scada_print_report(&report);

    /* OPC UA integration demo */
    printf("\n--- OPC UA Address Space ---\n");
    opcua_address_space_t as;
    opcua_init_address_space(&as);

    uint32_t tank_id = opcua_add_object_node(&as, "Tank1", 1);
    opcua_add_variable_node(&as, "Temperature", tank_id,
                            OPCUA_TYPE_DOUBLE, OPCUA_ACCESS_RW);
    opcua_add_variable_node(&as, "Pressure", tank_id,
                            OPCUA_TYPE_DOUBLE, OPCUA_ACCESS_RW);
    opcua_add_variable_node(&as, "Level", tank_id,
                            OPCUA_TYPE_DOUBLE, OPCUA_ACCESS_RW);
    opcua_add_variable_node(&as, "Flow", tank_id,
                            OPCUA_TYPE_DOUBLE, OPCUA_ACCESS_RW);
    opcua_add_variable_node(&as, "Alarm_Active", tank_id,
                            OPCUA_TYPE_BOOLEAN, OPCUA_ACCESS_READ);

    printf("Created OPC UA address space with %u nodes\n", as.num_nodes);

    opcua_node_id_t browse_results[16];
    int browse_count = opcua_browse_node(&as, tank_id, browse_results, 16);
    printf("Tank1 children: %d\n", browse_count);
    for (int i = 0; i < browse_count; i++) {
        opcua_node_t *node = opcua_find_node(&as, browse_results[i].identifier);
        if (node) {
            printf("  - %s (id=%u, class=%s)\n",
                   node->display_name, node->identifier,
                   opcua_node_class_string(node->node_class));
        }
    }

    /* Set values via OPC UA */
    opcua_node_t *temp_node = opcua_find_node_by_name(&as, "Temperature");
    if (temp_node) {
        double temp_val;
        scada_read_tag(&collector, 0, &temp_val);
        opcua_set_double_value(&as, temp_node->identifier, temp_val);
        printf("OPC UA set Temperature = %.2f\n", temp_val);
    }

    /* Create subscription */
    uint32_t sub_id = opcua_create_subscription(&as, 500.0);
    printf("Created subscription %u (interval=500ms)\n", sub_id);

    if (temp_node) {
        uint32_t mi_id = opcua_add_monitored_item(&as, sub_id,
                                                   temp_node->identifier, 250.0);
        printf("Added monitored item %u for Temperature\n", mi_id);
    }

    printf("\n=== Demo Complete ===\n");
    return 0;
}
