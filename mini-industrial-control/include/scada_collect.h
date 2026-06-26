#ifndef SCADA_COLLECT_H
#define SCADA_COLLECT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>

#define SCADA_MAX_TAGS           256
#define SCADA_MAX_HISTORY_POINTS 10000
#define SCADA_MAX_ALARMS         128
#define SCADA_MAX_TAG_NAME_LEN   48
#define SCADA_MAX_TAG_UNITS_LEN  16
#define SCADA_DEFAULT_DEADBAND   0.5
#define SCADA_DEFAULT_SCAN_MS    1000

typedef enum {
    SCADA_TAG_DISCRETE = 0,
    SCADA_TAG_ANALOG   = 1,
    SCADA_TAG_COUNTER  = 2,
    SCADA_TAG_STRING   = 3
} scada_tag_type_t;

typedef enum {
    SCADA_QUALITY_GOOD            = 0x00,
    SCADA_QUALITY_BAD             = 0x01,
    SCADA_QUALITY_UNCERTAIN       = 0x02,
    SCADA_QUALITY_COMM_FAILURE    = 0x04,
    SCADA_QUALITY_OVERRANGE       = 0x08,
    SCADA_QUALITY_MANUAL_FORCED   = 0x10
} scada_quality_t;

typedef enum {
    SCADA_ALARM_NONE   = 0,
    SCADA_ALARM_LOLO   = 1,
    SCADA_ALARM_LO     = 2,
    SCADA_ALARM_HI     = 3,
    SCADA_ALARM_HIHI   = 4,
    SCADA_ALARM_ROC    = 5,
    SCADA_ALARM_DEVIATION = 6
} scada_alarm_level_t;

typedef enum {
    SCADA_ALARM_STATE_NORMAL     = 0,
    SCADA_ALARM_STATE_ACTIVE     = 1,
    SCADA_ALARM_STATE_ACKED      = 2,
    SCADA_ALARM_STATE_RETURNED   = 3,
    SCADA_ALARM_STATE_SHELVED    = 4
} scada_alarm_state_t;

typedef struct {
    char             name[SCADA_MAX_TAG_NAME_LEN];
    char             description[SCADA_MAX_TAG_NAME_LEN];
    char             units[SCADA_MAX_TAG_UNITS_LEN];
    scada_tag_type_t type;
    uint16_t         device_address;
    uint16_t         register_address;
    uint32_t         scan_rate_ms;
    uint32_t         last_scan_ms;
    double           deadband;
    double           current_value;
    double           prev_value;
    uint8_t          quality;
    bool             enabled;
    bool             archiving;
    double           min_value;
    double           max_value;
    double           scale_factor;
    double           offset;
} scada_tag_t;

typedef struct {
    uint64_t     timestamp_ms;
    double       value;
    uint8_t      quality;
    uint32_t     tag_index;
} scada_history_point_t;

typedef struct {
    char                name[SCADA_MAX_TAG_NAME_LEN];
    scada_alarm_level_t level;
    double              limit;
    double              hysteresis;
    scada_alarm_state_t state;
    uint64_t            activation_time_ms;
    uint64_t            ack_time_ms;
    uint64_t            return_time_ms;
    bool                enabled;
    uint32_t            tag_index;
    char                message[SCADA_MAX_TAG_NAME_LEN];
} scada_alarm_t;

typedef struct {
    double daily_total;
    double daily_average;
    double daily_min;
    double daily_max;
    double hourly_rate;
    double efficiency;
    double downtime_minutes;
    double runtime_minutes;
    double oee;
    uint32_t good_parts;
    uint32_t bad_parts;
    char    report_date[20];
} scada_daily_report_t;

typedef struct {
    scada_tag_t            tags[SCADA_MAX_TAGS];
    uint32_t               num_tags;
    scada_history_point_t  history[SCADA_MAX_HISTORY_POINTS];
    uint32_t               num_history_points;
    uint32_t               history_write_index;
    double                 history_interval_s;
    scada_alarm_t          alarms[SCADA_MAX_ALARMS];
    uint32_t               num_alarms;
    bool                   collecting;
    uint64_t               start_time_ms;
    uint64_t               collection_interval_ms;
} scada_collector_t;

void scada_init_collector(scada_collector_t *collector);

uint32_t scada_add_tag(scada_collector_t *collector, const char *name,
                       scada_tag_type_t type, uint16_t device_addr,
                       uint16_t reg_addr, uint32_t scan_rate_ms);
bool     scada_remove_tag(scada_collector_t *collector, uint32_t tag_index);
scada_tag_t* scada_find_tag(scada_collector_t *collector, const char *name);

void scada_set_tag_scale(scada_tag_t *tag, double scale, double offset);
void scada_set_tag_deadband(scada_tag_t *tag, double deadband);

bool scada_read_tag(scada_collector_t *collector, uint32_t tag_index, double *value);
bool scada_write_tag_value(scada_collector_t *collector, uint32_t tag_index, double value);

void scada_poll_tag(scada_collector_t *collector, uint32_t tag_index, uint64_t current_time_ms);
void scada_poll_all_tags(scada_collector_t *collector, uint64_t current_time_ms);

bool scada_store_history_point(scada_collector_t *collector, uint32_t tag_index,
                               double value, uint8_t quality, uint64_t timestamp);
int  scada_query_history(const scada_collector_t *collector, uint32_t tag_index,
                         uint64_t start_time_ms, uint64_t end_time_ms,
                         scada_history_point_t *results, int max_results);

uint32_t scada_add_alarm(scada_collector_t *collector, const char *name,
                         uint32_t tag_index, scada_alarm_level_t level,
                         double limit, double hysteresis);
bool     scada_remove_alarm(scada_collector_t *collector, uint32_t alarm_index);

void scada_check_alarms(scada_collector_t *collector, uint64_t current_time_ms);
bool scada_acknowledge_alarm(scada_collector_t *collector, uint32_t alarm_index,
                             uint64_t ack_time_ms);
bool scada_shelve_alarm(scada_collector_t *collector, uint32_t alarm_index);

int scada_get_active_alarms(const scada_collector_t *collector,
                            uint32_t *alarm_indices, int max_results);

void scada_generate_daily_report(const scada_collector_t *collector,
                                 scada_daily_report_t *report);
void scada_print_report(const scada_daily_report_t *report);

const char* scada_alarm_level_string(scada_alarm_level_t level);
const char* scada_quality_string(uint8_t quality);

#endif /* SCADA_COLLECT_H */
