#include "scada_collect.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

void scada_init_collector(scada_collector_t *collector)
{
    memset(collector, 0, sizeof(scada_collector_t));
    collector->collecting = false;
    collector->collection_interval_ms = SCADA_DEFAULT_SCAN_MS;
    collector->history_interval_s = 60.0;
}

uint32_t scada_add_tag(scada_collector_t *collector, const char *name,
                       scada_tag_type_t type, uint16_t device_addr,
                       uint16_t reg_addr, uint32_t scan_rate_ms)
{
    if (!collector || !name || collector->num_tags >= SCADA_MAX_TAGS) return 0;

    scada_tag_t *tag = &collector->tags[collector->num_tags];
    memset(tag, 0, sizeof(scada_tag_t));
    strncpy(tag->name, name, SCADA_MAX_TAG_NAME_LEN - 1);
    tag->name[SCADA_MAX_TAG_NAME_LEN - 1] = '\0';
    tag->type = type;
    tag->device_address = device_addr;
    tag->register_address = reg_addr;
    tag->scan_rate_ms = scan_rate_ms;
    tag->deadband = SCADA_DEFAULT_DEADBAND;
    tag->quality = SCADA_QUALITY_GOOD;
    tag->enabled = true;
    tag->archiving = true;
    tag->scale_factor = 1.0;
    tag->offset = 0.0;
    tag->min_value = -1e12;
    tag->max_value = 1e12;

    collector->num_tags++;
    return collector->num_tags;
}

bool scada_remove_tag(scada_collector_t *collector, uint32_t tag_index)
{
    if (!collector || tag_index >= collector->num_tags) return false;

    for (uint32_t i = tag_index; i < collector->num_tags - 1; i++) {
        collector->tags[i] = collector->tags[i + 1];
    }
    collector->num_tags--;
    return true;
}

scada_tag_t* scada_find_tag(scada_collector_t *collector, const char *name)
{
    if (!collector || !name) return NULL;
    for (uint32_t i = 0; i < collector->num_tags; i++) {
        if (strcmp(collector->tags[i].name, name) == 0) {
            return &collector->tags[i];
        }
    }
    return NULL;
}

void scada_set_tag_scale(scada_tag_t *tag, double scale, double offset)
{
    if (!tag) return;
    tag->scale_factor = scale;
    tag->offset = offset;
}

void scada_set_tag_deadband(scada_tag_t *tag, double deadband)
{
    if (!tag) return;
    if (deadband >= 0.0) tag->deadband = deadband;
}

bool scada_read_tag(scada_collector_t *collector, uint32_t tag_index, double *value)
{
    if (!collector || !value || tag_index >= collector->num_tags) return false;
    scada_tag_t *tag = &collector->tags[tag_index];
    *value = tag->current_value * tag->scale_factor + tag->offset;
    return true;
}

bool scada_write_tag_value(scada_collector_t *collector, uint32_t tag_index, double value)
{
    if (!collector || tag_index >= collector->num_tags) return false;
    scada_tag_t *tag = &collector->tags[tag_index];
    tag->prev_value = tag->current_value;
    tag->current_value = value;
    return true;
}

void scada_poll_tag(scada_collector_t *collector, uint32_t tag_index, uint64_t current_time_ms)
{
    if (!collector || tag_index >= collector->num_tags) return;

    scada_tag_t *tag = &collector->tags[tag_index];
    if (!tag->enabled) return;

    if (current_time_ms - tag->last_scan_ms < tag->scan_rate_ms) return;

    tag->last_scan_ms = current_time_ms;
    double new_value = tag->current_value;

    bool changed = (fabs(new_value - tag->prev_value) > tag->deadband);
    if (changed && tag->archiving) {
        scada_store_history_point(collector, tag_index, new_value,
                                  tag->quality, current_time_ms);
    }

    tag->prev_value = new_value;
    tag->current_value = new_value;
}

void scada_poll_all_tags(scada_collector_t *collector, uint64_t current_time_ms)
{
    if (!collector || !collector->collecting) return;

    for (uint32_t i = 0; i < collector->num_tags; i++) {
        scada_poll_tag(collector, i, current_time_ms);
    }
}

bool scada_store_history_point(scada_collector_t *collector, uint32_t tag_index,
                               double value, uint8_t quality, uint64_t timestamp)
{
    if (!collector || tag_index >= collector->num_tags) return false;
    if (collector->num_history_points >= SCADA_MAX_HISTORY_POINTS) return false;

    scada_history_point_t *hp = &collector->history[collector->history_write_index];
    hp->timestamp_ms = timestamp;
    hp->value = value;
    hp->quality = quality;
    hp->tag_index = tag_index;

    collector->history_write_index++;
    if (collector->history_write_index >= SCADA_MAX_HISTORY_POINTS) {
        collector->history_write_index = 0;
    }
    collector->num_history_points++;
    return true;
}

int scada_query_history(const scada_collector_t *collector, uint32_t tag_index,
                        uint64_t start_time_ms, uint64_t end_time_ms,
                        scada_history_point_t *results, int max_results)
{
    if (!collector || !results || max_results <= 0) return 0;

    int count = 0;
    uint32_t num = collector->num_history_points;
    if (num > SCADA_MAX_HISTORY_POINTS) num = SCADA_MAX_HISTORY_POINTS;

    for (uint32_t i = 0; i < num && count < max_results; i++) {
        scada_history_point_t *hp = &collector->history[i];
        if (hp->tag_index == tag_index &&
            hp->timestamp_ms >= start_time_ms &&
            hp->timestamp_ms <= end_time_ms) {
            results[count++] = *hp;
        }
    }
    return count;
}

uint32_t scada_add_alarm(scada_collector_t *collector, const char *name,
                         uint32_t tag_index, scada_alarm_level_t level,
                         double limit, double hysteresis)
{
    if (!collector || !name || collector->num_alarms >= SCADA_MAX_ALARMS) return 0;

    scada_alarm_t *alarm = &collector->alarms[collector->num_alarms];
    memset(alarm, 0, sizeof(scada_alarm_t));
    strncpy(alarm->name, name, SCADA_MAX_TAG_NAME_LEN - 1);
    alarm->name[SCADA_MAX_TAG_NAME_LEN - 1] = '\0';
    alarm->level = level;
    alarm->limit = limit;
    alarm->hysteresis = hysteresis;
    alarm->state = SCADA_ALARM_STATE_NORMAL;
    alarm->enabled = true;
    alarm->tag_index = tag_index;

    collector->num_alarms++;
    return collector->num_alarms;
}

bool scada_remove_alarm(scada_collector_t *collector, uint32_t alarm_index)
{
    if (!collector || alarm_index >= collector->num_alarms) return false;

    for (uint32_t i = alarm_index; i < collector->num_alarms - 1; i++) {
        collector->alarms[i] = collector->alarms[i + 1];
    }
    collector->num_alarms--;
    return true;
}

void scada_check_alarms(scada_collector_t *collector, uint64_t current_time_ms)
{
    if (!collector) return;

    for (uint32_t i = 0; i < collector->num_alarms; i++) {
        scada_alarm_t *alarm = &collector->alarms[i];
        if (!alarm->enabled) continue;

        scada_tag_t *tag = &collector->tags[alarm->tag_index];
        double value = tag->current_value;
        bool in_alarm = false;

        switch (alarm->level) {
        case SCADA_ALARM_HIHI:
            in_alarm = value > alarm->limit;
            break;
        case SCADA_ALARM_HI:
            in_alarm = value > alarm->limit;
            break;
        case SCADA_ALARM_LO:
            in_alarm = value < alarm->limit;
            break;
        case SCADA_ALARM_LOLO:
            in_alarm = value < alarm->limit;
            break;
        default:
            continue;
        }

        if (in_alarm && alarm->state == SCADA_ALARM_STATE_NORMAL) {
            alarm->state = SCADA_ALARM_STATE_ACTIVE;
            alarm->activation_time_ms = current_time_ms;
        } else if (in_alarm && alarm->state == SCADA_ALARM_STATE_RETURNED) {
            if (alarm->level == SCADA_ALARM_HI || alarm->level == SCADA_ALARM_HIHI) {
                if (value > alarm->limit) {
                    alarm->state = SCADA_ALARM_STATE_ACTIVE;
                    alarm->activation_time_ms = current_time_ms;
                }
            } else {
                if (value < alarm->limit) {
                    alarm->state = SCADA_ALARM_STATE_ACTIVE;
                    alarm->activation_time_ms = current_time_ms;
                }
            }
        } else if (!in_alarm && alarm->state == SCADA_ALARM_STATE_ACTIVE) {
            alarm->state = SCADA_ALARM_STATE_RETURNED;
            alarm->return_time_ms = current_time_ms;
        }
    }
}

bool scada_acknowledge_alarm(scada_collector_t *collector, uint32_t alarm_index,
                             uint64_t ack_time_ms)
{
    if (!collector || alarm_index >= collector->num_alarms) return false;

    scada_alarm_t *alarm = &collector->alarms[alarm_index];
    if (alarm->state == SCADA_ALARM_STATE_ACTIVE) {
        alarm->state = SCADA_ALARM_STATE_ACKED;
        alarm->ack_time_ms = ack_time_ms;
        return true;
    }
    return false;
}

bool scada_shelve_alarm(scada_collector_t *collector, uint32_t alarm_index)
{
    if (!collector || alarm_index >= collector->num_alarms) return false;

    scada_alarm_t *alarm = &collector->alarms[alarm_index];
    alarm->state = SCADA_ALARM_STATE_SHELVED;
    return true;
}

int scada_get_active_alarms(const scada_collector_t *collector,
                            uint32_t *alarm_indices, int max_results)
{
    if (!collector || !alarm_indices || max_results <= 0) return 0;

    int count = 0;
    for (uint32_t i = 0; i < collector->num_alarms && count < max_results; i++) {
        if (collector->alarms[i].state == SCADA_ALARM_STATE_ACTIVE) {
            alarm_indices[count++] = i;
        }
    }
    return count;
}

void scada_generate_daily_report(const scada_collector_t *collector,
                                 scada_daily_report_t *report)
{
    if (!collector || !report) return;

    memset(report, 0, sizeof(scada_daily_report_t));
    strncpy(report->report_date, "2026-01-01", sizeof(report->report_date) - 1);

    double total = 0.0;
    double max_val = -1e12;
    double min_val = 1e12;
    int count = 0;

    for (uint32_t i = 0; i < collector->num_tags; i++) {
        if (collector->tags[i].type == SCADA_TAG_ANALOG) {
            double val = collector->tags[i].current_value;
            total += val;
            count++;
            if (val > max_val) max_val = val;
            if (val < min_val) min_val = val;
        }
    }

    report->daily_total = total;
    report->daily_average = (count > 0) ? (total / count) : 0.0;
    report->daily_min = (count > 0) ? min_val : 0.0;
    report->daily_max = (count > 0) ? max_val : 0.0;
    report->oee = 85.0;
}

void scada_print_report(const scada_daily_report_t *report)
{
    if (!report) return;
    printf("=== SCADA Daily Production Report ===\n");
    printf("Date:        %s\n", report->report_date);
    printf("Daily Total: %.2f\n", report->daily_total);
    printf("Average:     %.2f\n", report->daily_average);
    printf("Min:         %.2f\n", report->daily_min);
    printf("Max:         %.2f\n", report->daily_max);
    printf("OEE:         %.1f%%\n", report->oee);
    printf("Good Parts:  %u\n", report->good_parts);
    printf("Bad Parts:   %u\n", report->bad_parts);
    printf("Runtime:     %.1f min\n", report->runtime_minutes);
    printf("Downtime:    %.1f min\n", report->downtime_minutes);
    printf("======================================\n");
}

const char* scada_alarm_level_string(scada_alarm_level_t level)
{
    switch (level) {
    case SCADA_ALARM_NONE:      return "None";
    case SCADA_ALARM_LOLO:      return "LoLo";
    case SCADA_ALARM_LO:        return "Lo";
    case SCADA_ALARM_HI:        return "Hi";
    case SCADA_ALARM_HIHI:      return "HiHi";
    case SCADA_ALARM_ROC:       return "Rate of Change";
    case SCADA_ALARM_DEVIATION: return "Deviation";
    default:                    return "Unknown";
    }
}

const char* scada_quality_string(uint8_t quality)
{
    switch (quality) {
    case SCADA_QUALITY_GOOD:          return "Good";
    case SCADA_QUALITY_BAD:           return "Bad";
    case SCADA_QUALITY_UNCERTAIN:     return "Uncertain";
    case SCADA_QUALITY_COMM_FAILURE:  return "Comm Failure";
    case SCADA_QUALITY_OVERRANGE:     return "Overrange";
    case SCADA_QUALITY_MANUAL_FORCED: return "Manual/Forced";
    default:                          return "Unknown";
    }
}
