#include "safety_plc.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void safety_init(safety_controller_t *sc)
{
    memset(sc, 0, sizeof(safety_controller_t));
    sc->system_healthy = true;
    sc->comm_protocol = SAFETY_FAILSAFE_IO;
}

uint16_t safety_add_dual_channel(safety_controller_t *sc)
{
    if (!sc || sc->num_channels >= SAFETY_MAX_CHANNELS) return 0;

    safety_dual_channel_t *ch = &sc->channels[sc->num_channels];
    memset(ch, 0, sizeof(safety_dual_channel_t));
    ch->channel_id = sc->num_channels + 1;
    ch->healthy = true;
    sc->num_channels++;
    return ch->channel_id;
}

bool safety_remove_channel(safety_controller_t *sc, uint16_t channel_id)
{
    if (!sc || channel_id == 0) return false;

    for (uint16_t i = 0; i < sc->num_channels; i++) {
        if (sc->channels[i].channel_id == channel_id) {
            for (uint16_t j = i; j < sc->num_channels - 1; j++) {
                sc->channels[j] = sc->channels[j + 1];
            }
            sc->num_channels--;
            return true;
        }
    }
    return false;
}

void safety_update_channel_inputs(safety_controller_t *sc, uint16_t channel_id,
                                  bool input_a, bool input_b, uint32_t current_time_ms)
{
    if (!sc) return;

    for (uint16_t i = 0; i < sc->num_channels; i++) {
        safety_dual_channel_t *ch = &sc->channels[i];
        if (ch->channel_id == channel_id) {
            ch->prev_input_a = ch->input_a;
            ch->prev_input_b = ch->input_b;
            ch->input_a = input_a;
            ch->input_b = input_b;

            if (input_a != input_b) {
                if (ch->discrepancy_timer_ms == 0) {
                    ch->discrepancy_timer_ms = current_time_ms;
                }
            } else {
                ch->discrepancy_timer_ms = 0;
            }
            ch->last_change_tick = current_time_ms;
        }
    }
}

bool safety_is_channel_healthy(const safety_controller_t *sc, uint16_t channel_id)
{
    if (!sc) return false;

    for (uint16_t i = 0; i < sc->num_channels; i++) {
        if (sc->channels[i].channel_id == channel_id) {
            return sc->channels[i].healthy;
        }
    }
    return false;
}

bool safety_check_discrepancy(safety_controller_t *sc, uint16_t channel_id,
                              uint32_t current_time_ms)
{
    if (!sc) return false;

    for (uint16_t i = 0; i < sc->num_channels; i++) {
        safety_dual_channel_t *ch = &sc->channels[i];
        if (ch->channel_id == channel_id) {
            if (ch->discrepancy_timer_ms > 0) {
                uint32_t elapsed = current_time_ms - ch->discrepancy_timer_ms;
                if (elapsed > SAFETY_DISCREPANCY_MS) {
                    ch->discrepancy_alarm = true;
                    ch->healthy = false;
                    return true;
                }
            }
        }
    }
    return false;
}

uint16_t safety_add_function(safety_controller_t *sc, const char *name,
                             safety_function_type_t type, safety_sil_level_t sil)
{
    if (!sc || !name || sc->num_functions >= SAFETY_MAX_FUNCTIONS) return 0;

    safety_function_t *sf = &sc->functions[sc->num_functions];
    memset(sf, 0, sizeof(safety_function_t));
    strncpy(sf->name, name, SAFETY_MAX_NAME_LEN - 1);
    sf->name[SAFETY_MAX_NAME_LEN - 1] = '\0';
    sf->type = type;
    sf->sil_level = sil;
    sf->active = false;
    sf->tripped = false;
    sf->healthy = true;
    sf->reset_required = 0;

    sc->num_functions++;
    return sc->num_functions;
}

bool safety_remove_function(safety_controller_t *sc, uint16_t func_id)
{
    if (!sc || func_id == 0 || func_id > sc->num_functions) return false;

    for (uint16_t i = func_id - 1; i < sc->num_functions - 1; i++) {
        sc->functions[i] = sc->functions[i + 1];
    }
    sc->num_functions--;
    return true;
}

void safety_estop_trigger(safety_controller_t *sc, uint64_t time_ms)
{
    if (!sc) return;
    sc->estop_active = true;
    sc->total_trip_count++;

    for (uint16_t i = 0; i < sc->num_functions; i++) {
        if (sc->functions[i].type == SAFETY_FUNC_ESTOP) {
            sc->functions[i].tripped = true;
            sc->functions[i].last_trip_time_ms = time_ms;
        }
    }

    for (uint16_t i = 0; i < sc->num_safe_outputs; i++) {
        sc->safe_outputs[i] = false;
    }
}

void safety_estop_release(safety_controller_t *sc)
{
    if (!sc) return;
    sc->estop_active = false;

    for (uint16_t i = 0; i < sc->num_functions; i++) {
        if (sc->functions[i].type == SAFETY_FUNC_ESTOP) {
            sc->functions[i].active = true;
        }
    }
}

bool safety_estop_is_active(const safety_controller_t *sc)
{
    return sc ? sc->estop_active : false;
}

void safety_door_interlock(safety_controller_t *sc, bool door_closed, uint64_t time_ms)
{
    if (!sc) return;

    for (uint16_t i = 0; i < sc->num_functions; i++) {
        if (sc->functions[i].type == SAFETY_FUNC_DOOR_INTERLOCK) {
            if (!door_closed) {
                sc->functions[i].tripped = true;
                sc->functions[i].last_trip_time_ms = time_ms;
                sc->total_trip_count++;
            } else {
                sc->functions[i].active = true;
            }
        }
    }
}

bool safety_door_is_closed(const safety_controller_t *sc)
{
    if (!sc) return true;

    for (uint16_t i = 0; i < sc->num_functions; i++) {
        if (sc->functions[i].type == SAFETY_FUNC_DOOR_INTERLOCK) {
            return !sc->functions[i].tripped;
        }
    }
    return true;
}

void safety_light_curtain(safety_controller_t *sc, bool clear, uint64_t time_ms)
{
    if (!sc) return;

    for (uint16_t i = 0; i < sc->num_functions; i++) {
        if (sc->functions[i].type == SAFETY_FUNC_LIGHT_CURTAIN) {
            if (!clear) {
                sc->functions[i].tripped = true;
                sc->functions[i].last_trip_time_ms = time_ms;
                sc->total_trip_count++;
            } else {
                sc->functions[i].active = true;
            }
        }
    }
}

bool safety_light_curtain_is_clear(const safety_controller_t *sc)
{
    if (!sc) return true;

    for (uint16_t i = 0; i < sc->num_functions; i++) {
        if (sc->functions[i].type == SAFETY_FUNC_LIGHT_CURTAIN) {
            return !sc->functions[i].tripped;
        }
    }
    return true;
}

void safety_two_hand_init(safety_two_hand_t *th, uint32_t max_time_ms)
{
    if (!th) return;
    memset(th, 0, sizeof(safety_two_hand_t));
    th->max_time_between_ms = max_time_ms;
}

void safety_two_hand_update(safety_two_hand_t *th, bool left, bool right,
                            uint32_t current_time_ms)
{
    if (!th) return;

    th->prev_left = th->button_left;
    th->prev_right = th->button_right;
    th->button_left = left;
    th->button_right = right;

    bool left_rising = left && !th->prev_left;
    bool right_rising = right && !th->prev_right;

    if (left_rising && right_rising) {
        th->valid_sequence = true;
        th->output = true;
        th->timer_ms = current_time_ms;
    } else if (left_rising) {
        th->timer_ms = current_time_ms;
        th->valid_sequence = true;
    } else if (right_rising) {
        if (th->valid_sequence) {
            uint32_t elapsed = current_time_ms - th->timer_ms;
            if (elapsed <= th->max_time_between_ms) {
                th->output = true;
            }
        }
    }

    if (!left || !right) {
        th->output = false;
        th->valid_sequence = false;
    }
}

bool safety_two_hand_output(const safety_two_hand_t *th)
{
    return th ? th->output : false;
}

void safety_set_output(safety_controller_t *sc, uint16_t output_id, bool state)
{
    if (!sc || output_id >= SAFETY_MAX_SAFE_OUTPUTS) return;
    sc->safe_outputs[output_id] = state;
}

bool safety_get_output(const safety_controller_t *sc, uint16_t output_id)
{
    if (!sc || output_id >= SAFETY_MAX_SAFE_OUTPUTS) return false;
    return sc->safe_outputs[output_id];
}

void safety_evaluate(safety_controller_t *sc, uint64_t current_time_ms)
{
    if (!sc) return;

    for (uint16_t i = 0; i < sc->num_functions; i++) {
        safety_function_t *sf = &sc->functions[i];
        if (sf->tripped) {
            for (uint16_t j = 0; j < sc->num_safe_outputs; j++) {
                sc->safe_outputs[j] = false;
            }
        }
    }

    (void)current_time_ms;
}

bool safety_system_healthy(const safety_controller_t *sc)
{
    if (!sc) return false;

    for (uint16_t i = 0; i < sc->num_channels; i++) {
        if (!sc->channels[i].healthy) return false;
    }
    for (uint16_t i = 0; i < sc->num_functions; i++) {
        if (!sc->functions[i].healthy) return false;
    }
    return sc->system_healthy;
}

bool safety_sil_verify(safety_sil_level_t required_sil, uint32_t channel_mttfd_hours)
{
    switch (required_sil) {
    case SAFETY_SIL1: return channel_mttfd_hours >= 1000;
    case SAFETY_SIL2: return channel_mttfd_hours >= 5000;
    case SAFETY_SIL3: return channel_mttfd_hours >= 20000;
    case SAFETY_SIL4: return channel_mttfd_hours >= 50000;
    default:          return false;
    }
}

double safety_pfd_calculate(safety_sil_level_t sil)
{
    switch (sil) {
    case SAFETY_SIL1: return 1e-2;
    case SAFETY_SIL2: return 1e-3;
    case SAFETY_SIL3: return 1e-4;
    case SAFETY_SIL4: return 1e-5;
    default:          return 1.0;
    }
}

const char* safety_sil_string(safety_sil_level_t sil)
{
    switch (sil) {
    case SAFETY_SIL1: return "SIL1";
    case SAFETY_SIL2: return "SIL2";
    case SAFETY_SIL3: return "SIL3";
    case SAFETY_SIL4: return "SIL4";
    default:          return "Unknown";
    }
}

const char* safety_function_string(safety_function_type_t type)
{
    switch (type) {
    case SAFETY_FUNC_ESTOP:          return "E-Stop";
    case SAFETY_FUNC_DOOR_INTERLOCK: return "Door Interlock";
    case SAFETY_FUNC_LIGHT_CURTAIN:  return "Light Curtain";
    case SAFETY_FUNC_TWO_HAND:       return "Two-Hand Control";
    case SAFETY_FUNC_ZERO_SPEED:     return "Zero Speed";
    case SAFETY_FUNC_OVERSPEED:      return "Overspeed";
    case SAFETY_FUNC_SAFE_TORQUE:    return "Safe Torque Off";
    case SAFETY_FUNC_SAFE_POSITION:  return "Safe Position";
    default:                         return "Unknown";
    }
}

void safety_reset_function(safety_controller_t *sc, uint16_t func_id, uint64_t time_ms)
{
    if (!sc || func_id == 0 || func_id > sc->num_functions) return;

    safety_function_t *sf = &sc->functions[func_id - 1];
    sf->tripped = false;
    sf->reset_required = 0;
    sf->last_reset_time_ms = time_ms;
}

void safety_reset_all_functions(safety_controller_t *sc, uint64_t time_ms)
{
    if (!sc) return;

    for (uint16_t i = 0; i < sc->num_functions; i++) {
        sc->functions[i].tripped = false;
        sc->functions[i].reset_required = 0;
        sc->functions[i].last_reset_time_ms = time_ms;
    }
}

uint32_t safety_get_total_faults(const safety_controller_t *sc)
{
    return sc ? sc->total_fault_count : 0;
}

uint32_t safety_get_total_trips(const safety_controller_t *sc)
{
    return sc ? sc->total_trip_count : 0;
}
