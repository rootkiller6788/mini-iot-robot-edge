#ifndef SAFETY_PLC_H
#define SAFETY_PLC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define SAFETY_MAX_CHANNELS      64
#define SAFETY_MAX_FUNCTIONS     32
#define SAFETY_MAX_SAFE_OUTPUTS  32
#define SAFETY_MAX_NAME_LEN      32
#define SAFETY_DISCREPANCY_MS    100
#define SAFETY_TWO_HAND_MAX_MS   500
#define SAFETY_DEBOUNCE_MS       5

typedef enum {
    SAFETY_SIL1 = 1,
    SAFETY_SIL2 = 2,
    SAFETY_SIL3 = 3,
    SAFETY_SIL4 = 4
} safety_sil_level_t;

typedef enum {
    SAFETY_CHANNEL_SINGLE = 0,
    SAFETY_CHANNEL_DUAL   = 1,
    SAFETY_CHANNEL_TRIPLE = 2
} safety_channel_config_t;

typedef enum {
    SAFETY_FUNC_ESTOP          = 0,
    SAFETY_FUNC_DOOR_INTERLOCK = 1,
    SAFETY_FUNC_LIGHT_CURTAIN  = 2,
    SAFETY_FUNC_TWO_HAND       = 3,
    SAFETY_FUNC_ZERO_SPEED     = 4,
    SAFETY_FUNC_OVERSPEED      = 5,
    SAFETY_FUNC_SAFE_TORQUE    = 6,
    SAFETY_FUNC_SAFE_POSITION  = 7
} safety_function_type_t;

typedef enum {
    SAFETY_PROFISAFE   = 0,
    SAFETY_CIP_SAFETY  = 1,
    SAFETY_FAILSAFE_IO = 2,
    SAFETY_SAFE_ETHERCAT = 3
} safety_comm_protocol_t;

typedef struct {
    uint16_t  channel_id;
    bool      input_a;
    bool      input_b;
    bool      prev_input_a;
    bool      prev_input_b;
    uint32_t  discrepancy_timer_ms;
    bool      discrepancy_alarm;
    bool      healthy;
    uint32_t  last_change_tick;
} safety_dual_channel_t;

typedef struct {
    char                     name[SAFETY_MAX_NAME_LEN];
    safety_function_type_t   type;
    safety_sil_level_t       sil_level;
    bool                     active;
    bool                     tripped;
    bool                     healthy;
    uint32_t                 reset_required;
    uint32_t                 diagnostic_code;
    uint64_t                 last_trip_time_ms;
    uint64_t                 last_reset_time_ms;
} safety_function_t;

typedef struct {
    bool    button_left;
    bool    button_right;
    bool    prev_left;
    bool    prev_right;
    uint32_t max_time_between_ms;
    uint32_t timer_ms;
    bool    output;
    bool    valid_sequence;
} safety_two_hand_t;

typedef struct {
    safety_dual_channel_t  channels[SAFETY_MAX_CHANNELS];
    uint16_t               num_channels;
    safety_function_t      functions[SAFETY_MAX_FUNCTIONS];
    uint16_t               num_functions;
    bool                   safe_outputs[SAFETY_MAX_SAFE_OUTPUTS];
    uint16_t               num_safe_outputs;
    safety_comm_protocol_t comm_protocol;
    bool                   estop_active;
    bool                   maintenance_mode;
    uint32_t               uptime_ms;
    uint32_t               total_fault_count;
    uint32_t               total_trip_count;
    bool                   system_healthy;
} safety_controller_t;

void safety_init(safety_controller_t *sc);

uint16_t safety_add_dual_channel(safety_controller_t *sc);
bool     safety_remove_channel(safety_controller_t *sc, uint16_t channel_id);

void safety_update_channel_inputs(safety_controller_t *sc, uint16_t channel_id,
                                  bool input_a, bool input_b, uint32_t current_time_ms);
bool safety_is_channel_healthy(const safety_controller_t *sc, uint16_t channel_id);
bool safety_check_discrepancy(safety_controller_t *sc, uint16_t channel_id,
                              uint32_t current_time_ms);

uint16_t safety_add_function(safety_controller_t *sc, const char *name,
                             safety_function_type_t type, safety_sil_level_t sil);
bool     safety_remove_function(safety_controller_t *sc, uint16_t func_id);

void safety_estop_trigger(safety_controller_t *sc, uint64_t time_ms);
void safety_estop_release(safety_controller_t *sc);
bool safety_estop_is_active(const safety_controller_t *sc);

void safety_door_interlock(safety_controller_t *sc, bool door_closed, uint64_t time_ms);
bool safety_door_is_closed(const safety_controller_t *sc);

void safety_light_curtain(safety_controller_t *sc, bool clear, uint64_t time_ms);
bool safety_light_curtain_is_clear(const safety_controller_t *sc);

void safety_two_hand_init(safety_two_hand_t *th, uint32_t max_time_ms);
void safety_two_hand_update(safety_two_hand_t *th, bool left, bool right,
                            uint32_t current_time_ms);
bool safety_two_hand_output(const safety_two_hand_t *th);

void safety_set_output(safety_controller_t *sc, uint16_t output_id, bool state);
bool safety_get_output(const safety_controller_t *sc, uint16_t output_id);

void safety_evaluate(safety_controller_t *sc, uint64_t current_time_ms);
bool safety_system_healthy(const safety_controller_t *sc);

bool safety_sil_verify(safety_sil_level_t required_sil, uint32_t channel_mttfd_hours);
double safety_pfd_calculate(safety_sil_level_t sil);
const char* safety_sil_string(safety_sil_level_t sil);
const char* safety_function_string(safety_function_type_t type);

void safety_reset_function(safety_controller_t *sc, uint16_t func_id, uint64_t time_ms);
void safety_reset_all_functions(safety_controller_t *sc, uint64_t time_ms);

uint32_t safety_get_total_faults(const safety_controller_t *sc);
uint32_t safety_get_total_trips(const safety_controller_t *sc);

#endif /* SAFETY_PLC_H */
