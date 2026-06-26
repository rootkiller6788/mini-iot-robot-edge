#ifndef PLC_LADDER_H
#define PLC_LADDER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PLC_MAX_RUNGS           128
#define PLC_MAX_CONTACTS        16
#define PLC_MAX_COILS           16
#define PLC_MAX_TIMERS          32
#define PLC_MAX_COUNTERS        32
#define PLC_MAX_NAME_LEN        32
#define PLC_SCAN_PERIOD_MS      10

typedef enum {
    PLC_CONTACT_NO = 0,
    PLC_CONTACT_NC = 1
} plc_contact_type_t;

typedef enum {
    PLC_COIL_OUTPUT      = 0,
    PLC_COIL_INTERNAL    = 1,
    PLC_COIL_LATCH_SET   = 2,
    PLC_COIL_LATCH_RESET = 3
} plc_coil_type_t;

typedef enum {
    PLC_TIMER_TON = 0,
    PLC_TIMER_TOF = 1,
    PLC_TIMER_TP  = 2
} plc_timer_type_t;

typedef enum {
    PLC_COUNTER_CTU = 0,
    PLC_COUNTER_CTD = 1,
    PLC_COUNTER_CTUD = 2
} plc_counter_type_t;

typedef enum {
    PLC_SCAN_INPUTS   = 0,
    PLC_SCAN_PROGRAM  = 1,
    PLC_SCAN_OUTPUTS  = 2,
    PLC_SCAN_HOUSEKEEPING = 3
} plc_scan_phase_t;

typedef struct {
    char              name[PLC_MAX_NAME_LEN];
    plc_contact_type_t type;
    uint16_t          address;
    bool              state;
} plc_contact_t;

typedef struct {
    char              name[PLC_MAX_NAME_LEN];
    plc_coil_type_t   type;
    uint16_t          address;
    bool              state;
    bool              prev_state;
} plc_coil_t;

typedef struct {
    char              name[PLC_MAX_NAME_LEN];
    plc_timer_type_t  type;
    uint32_t          preset_ms;
    uint32_t          accumulated_ms;
    bool              timing;
    bool              done;
    bool              input_state;
    uint32_t          last_tick;
} plc_timer_t;

typedef struct {
    char                name[PLC_MAX_NAME_LEN];
    plc_counter_type_t   type;
    uint16_t             preset;
    uint16_t             accumulated;
    bool                 done;
    bool                 overflow;
    bool                 underflow;
    bool                 count_up_input;
    bool                 count_down_input;
    bool                 prev_cu;
    bool                 prev_cd;
} plc_counter_t;

typedef struct {
    plc_contact_t  contacts[PLC_MAX_CONTACTS];
    uint8_t        num_contacts;
    plc_coil_t     coils[PLC_MAX_COILS];
    uint8_t        num_coils;
    plc_timer_t    timers[PLC_MAX_TIMERS];
    uint8_t        num_timers;
    plc_counter_t  counters[PLC_MAX_COUNTERS];
    uint8_t        num_counters;
    bool           power_flow;
} plc_rung_t;

typedef struct {
    plc_rung_t      rungs[PLC_MAX_RUNGS];
    uint16_t        num_rungs;
    uint16_t        scan_counter;
    plc_scan_phase_t current_phase;
    uint32_t        scan_time_us;
    uint32_t        last_scan_tick;
    bool            running;
    bool            fault;
    uint16_t        fault_code;
} plc_program_t;

void        plc_init(plc_program_t *plc);
void        plc_reset(plc_program_t *plc);
plc_rung_t* plc_add_rung(plc_program_t *plc);
bool        plc_remove_rung(plc_program_t *plc, uint16_t index);

bool plc_add_contact(plc_rung_t *rung, const char *name,
                     plc_contact_type_t type, uint16_t address);
bool plc_add_coil(plc_rung_t *rung, const char *name,
                  plc_coil_type_t type, uint16_t address);
bool plc_add_timer(plc_rung_t *rung, const char *name,
                   plc_timer_type_t type, uint32_t preset_ms);
bool plc_add_counter(plc_rung_t *rung, const char *name,
                     plc_counter_type_t type, uint16_t preset);

bool plc_read_input(plc_program_t *plc, uint16_t address);
void plc_write_input(plc_program_t *plc, uint16_t address, bool value);
bool plc_read_output(plc_program_t *plc, uint16_t address);
void plc_write_output(plc_program_t *plc, uint16_t address, bool value);

void plc_scan_cycle(plc_program_t *plc);
bool plc_evaluate_rung(plc_rung_t *rung);
bool plc_evaluate_contact(plc_contact_t *contact);
void plc_update_timer(plc_timer_t *timer);
void plc_update_counter(plc_counter_t *counter);
void plc_update_outputs(plc_program_t *plc);

uint32_t plc_get_scan_time_us(const plc_program_t *plc);
uint16_t plc_get_scan_count(const plc_program_t *plc);
bool     plc_is_fault(const plc_program_t *plc);
uint16_t plc_get_fault_code(const plc_program_t *plc);
void     plc_clear_fault(plc_program_t *plc);

void     plc_timer_reset(plc_timer_t *timer);
uint32_t plc_timer_elapsed_ms(const plc_timer_t *timer);
uint32_t plc_timer_remaining_ms(const plc_timer_t *timer);

void     plc_counter_reset(plc_counter_t *counter);
uint16_t plc_counter_value(const plc_counter_t *counter);
bool     plc_counter_done(const plc_counter_t *counter);

#endif /* PLC_LADDER_H */
