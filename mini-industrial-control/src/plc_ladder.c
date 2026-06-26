#include "plc_ladder.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void plc_init(plc_program_t *plc)
{
    plc->num_rungs = 0;
    plc->scan_counter = 0;
    plc->current_phase = PLC_SCAN_INPUTS;
    plc->scan_time_us = 0;
    plc->last_scan_tick = 0;
    plc->running = false;
    plc->fault = false;
    plc->fault_code = 0;
    memset(plc->rungs, 0, sizeof(plc->rungs));
}

void plc_reset(plc_program_t *plc)
{
    plc->num_rungs = 0;
    plc->scan_counter = 0;
    plc->fault = false;
    plc->fault_code = 0;
    memset(plc->rungs, 0, sizeof(plc->rungs));
}

plc_rung_t* plc_add_rung(plc_program_t *plc)
{
    if (plc->num_rungs >= PLC_MAX_RUNGS) return NULL;
    plc_rung_t *rung = &plc->rungs[plc->num_rungs];
    memset(rung, 0, sizeof(plc_rung_t));
    rung->power_flow = false;
    plc->num_rungs++;
    return rung;
}

bool plc_remove_rung(plc_program_t *plc, uint16_t index)
{
    if (index >= plc->num_rungs) return false;
    for (uint16_t i = index; i < plc->num_rungs - 1; i++) {
        plc->rungs[i] = plc->rungs[i + 1];
    }
    plc->num_rungs--;
    return true;
}

bool plc_add_contact(plc_rung_t *rung, const char *name,
                     plc_contact_type_t type, uint16_t address)
{
    if (rung->num_contacts >= PLC_MAX_CONTACTS) return false;
    plc_contact_t *c = &rung->contacts[rung->num_contacts];
    strncpy(c->name, name, PLC_MAX_NAME_LEN - 1);
    c->name[PLC_MAX_NAME_LEN - 1] = '\0';
    c->type = type;
    c->address = address;
    c->state = false;
    rung->num_contacts++;
    return true;
}

bool plc_add_coil(plc_rung_t *rung, const char *name,
                  plc_coil_type_t type, uint16_t address)
{
    if (rung->num_coils >= PLC_MAX_COILS) return false;
    plc_coil_t *c = &rung->coils[rung->num_coils];
    strncpy(c->name, name, PLC_MAX_NAME_LEN - 1);
    c->name[PLC_MAX_NAME_LEN - 1] = '\0';
    c->type = type;
    c->address = address;
    c->state = false;
    c->prev_state = false;
    rung->num_coils++;
    return true;
}

bool plc_add_timer(plc_rung_t *rung, const char *name,
                   plc_timer_type_t type, uint32_t preset_ms)
{
    if (rung->num_timers >= PLC_MAX_TIMERS) return false;
    plc_timer_t *t = &rung->timers[rung->num_timers];
    strncpy(t->name, name, PLC_MAX_NAME_LEN - 1);
    t->name[PLC_MAX_NAME_LEN - 1] = '\0';
    t->type = type;
    t->preset_ms = preset_ms;
    t->accumulated_ms = 0;
    t->timing = false;
    t->done = false;
    t->input_state = false;
    t->last_tick = 0;
    rung->num_timers++;
    return true;
}

bool plc_add_counter(plc_rung_t *rung, const char *name,
                     plc_counter_type_t type, uint16_t preset)
{
    if (rung->num_counters >= PLC_MAX_COUNTERS) return false;
    plc_counter_t *c = &rung->counters[rung->num_counters];
    strncpy(c->name, name, PLC_MAX_NAME_LEN - 1);
    c->name[PLC_MAX_NAME_LEN - 1] = '\0';
    c->type = type;
    c->preset = preset;
    c->accumulated = 0;
    c->done = false;
    c->overflow = false;
    c->underflow = false;
    c->count_up_input = false;
    c->count_down_input = false;
    c->prev_cu = false;
    c->prev_cd = false;
    rung->num_counters++;
    return true;
}

bool plc_read_input(plc_program_t *plc, uint16_t address)
{
    (void)address;
    for (uint16_t i = 0; i < plc->num_rungs; i++) {
        plc_rung_t *rung = &plc->rungs[i];
        for (uint8_t j = 0; j < rung->num_contacts; j++) {
            if (rung->contacts[j].address == address) {
                return rung->contacts[j].state;
            }
        }
    }
    return false;
}

void plc_write_input(plc_program_t *plc, uint16_t address, bool value)
{
    for (uint16_t i = 0; i < plc->num_rungs; i++) {
        plc_rung_t *rung = &plc->rungs[i];
        for (uint8_t j = 0; j < rung->num_contacts; j++) {
            if (rung->contacts[j].address == address) {
                rung->contacts[j].state = value;
            }
        }
    }
}

bool plc_read_output(plc_program_t *plc, uint16_t address)
{
    for (uint16_t i = 0; i < plc->num_rungs; i++) {
        plc_rung_t *rung = &plc->rungs[i];
        for (uint8_t j = 0; j < rung->num_coils; j++) {
            if (rung->coils[j].address == address) {
                return rung->coils[j].state;
            }
        }
    }
    return false;
}

void plc_write_output(plc_program_t *plc, uint16_t address, bool value)
{
    for (uint16_t i = 0; i < plc->num_rungs; i++) {
        plc_rung_t *rung = &plc->rungs[i];
        for (uint8_t j = 0; j < rung->num_coils; j++) {
            if (rung->coils[j].address == address) {
                rung->coils[j].state = value;
            }
        }
    }
}

bool plc_evaluate_contact(plc_contact_t *contact)
{
    switch (contact->type) {
    case PLC_CONTACT_NO:
        return contact->state;
    case PLC_CONTACT_NC:
        return !contact->state;
    default:
        return false;
    }
}

void plc_update_timer(plc_timer_t *timer)
{
    bool input_on = timer->input_state;

    switch (timer->type) {
    case PLC_TIMER_TON:
        if (input_on) {
            if (!timer->timing) {
                timer->timing = true;
                timer->accumulated_ms = 0;
            }
            timer->accumulated_ms += PLC_SCAN_PERIOD_MS;
            if (timer->accumulated_ms >= timer->preset_ms) {
                timer->accumulated_ms = timer->preset_ms;
                timer->done = true;
            }
        } else {
            timer->timing = false;
            timer->accumulated_ms = 0;
            timer->done = false;
        }
        break;
    case PLC_TIMER_TOF:
        if (!input_on) {
            if (!timer->timing) {
                timer->timing = true;
                timer->accumulated_ms = 0;
            }
            timer->accumulated_ms += PLC_SCAN_PERIOD_MS;
            if (timer->accumulated_ms >= timer->preset_ms) {
                timer->accumulated_ms = timer->preset_ms;
                timer->done = false;
            }
        } else {
            timer->timing = false;
            timer->accumulated_ms = 0;
            timer->done = true;
        }
        break;
    case PLC_TIMER_TP:
        if (input_on && !timer->timing) {
            timer->timing = true;
            timer->accumulated_ms = 0;
            timer->done = true;
        }
        if (timer->timing) {
            timer->accumulated_ms += PLC_SCAN_PERIOD_MS;
            if (timer->accumulated_ms >= timer->preset_ms) {
                timer->accumulated_ms = timer->preset_ms;
                timer->done = false;
                timer->timing = false;
            }
        }
        if (!input_on) {
            timer->timing = false;
        }
        break;
    }
}

void plc_update_counter(plc_counter_t *counter)
{
    bool cu_rising = counter->count_up_input && !counter->prev_cu;
    bool cd_rising = counter->count_down_input && !counter->prev_cd;

    counter->prev_cu = counter->count_up_input;
    counter->prev_cd = counter->count_down_input;

    switch (counter->type) {
    case PLC_COUNTER_CTU:
        if (cu_rising && !counter->overflow) {
            if (counter->accumulated < UINT16_MAX) {
                counter->accumulated++;
            } else {
                counter->overflow = true;
            }
        }
        if (counter->accumulated >= counter->preset) {
            counter->done = true;
        } else {
            counter->done = false;
        }
        break;
    case PLC_COUNTER_CTD:
        if (cd_rising && !counter->underflow) {
            if (counter->accumulated > 0) {
                counter->accumulated--;
            } else {
                counter->underflow = true;
            }
        }
        if (counter->accumulated == 0) {
            counter->done = true;
        } else {
            counter->done = false;
        }
        break;
    case PLC_COUNTER_CTUD:
        if (cu_rising && !counter->overflow) {
            if (counter->accumulated < UINT16_MAX) {
                counter->accumulated++;
            } else {
                counter->overflow = true;
            }
        }
        if (cd_rising && !counter->underflow) {
            if (counter->accumulated > 0) {
                counter->accumulated--;
            } else {
                counter->underflow = true;
            }
        }
        if (counter->accumulated >= counter->preset) {
            counter->done = true;
        } else {
            counter->done = false;
        }
        break;
    }
}

bool plc_evaluate_rung(plc_rung_t *rung)
{
    bool power = true;

    for (uint8_t i = 0; i < rung->num_contacts; i++) {
        power = power && plc_evaluate_contact(&rung->contacts[i]);
    }

    rung->power_flow = power;

    for (uint8_t i = 0; i < rung->num_timers; i++) {
        rung->timers[i].input_state = power;
        plc_update_timer(&rung->timers[i]);
    }

    for (uint8_t i = 0; i < rung->num_counters; i++) {
        plc_update_counter(&rung->counters[i]);
    }

    return power;
}

void plc_update_outputs(plc_program_t *plc)
{
    for (uint16_t i = 0; i < plc->num_rungs; i++) {
        plc_rung_t *rung = &plc->rungs[i];
        for (uint8_t j = 0; j < rung->num_coils; j++) {
            plc_coil_t *coil = &rung->coils[j];
            coil->prev_state = coil->state;
            switch (coil->type) {
            case PLC_COIL_OUTPUT:
                coil->state = rung->power_flow;
                break;
            case PLC_COIL_INTERNAL:
                coil->state = rung->power_flow;
                break;
            case PLC_COIL_LATCH_SET:
                if (rung->power_flow) coil->state = true;
                break;
            case PLC_COIL_LATCH_RESET:
                if (rung->power_flow) coil->state = false;
                break;
            }
        }
    }
}

void plc_scan_cycle(plc_program_t *plc)
{
    if (plc->fault) return;

    plc->current_phase = PLC_SCAN_INPUTS;
    plc->current_phase = PLC_SCAN_PROGRAM;

    for (uint16_t i = 0; i < plc->num_rungs; i++) {
        plc_evaluate_rung(&plc->rungs[i]);
    }

    plc->current_phase = PLC_SCAN_OUTPUTS;
    plc_update_outputs(plc);

    plc->current_phase = PLC_SCAN_HOUSEKEEPING;
    plc->scan_counter++;
}

uint32_t plc_get_scan_time_us(const plc_program_t *plc)
{
    return plc->scan_time_us;
}

uint16_t plc_get_scan_count(const plc_program_t *plc)
{
    return plc->scan_counter;
}

bool plc_is_fault(const plc_program_t *plc)
{
    return plc->fault;
}

uint16_t plc_get_fault_code(const plc_program_t *plc)
{
    return plc->fault_code;
}

void plc_clear_fault(plc_program_t *plc)
{
    plc->fault = false;
    plc->fault_code = 0;
}

void plc_timer_reset(plc_timer_t *timer)
{
    timer->accumulated_ms = 0;
    timer->timing = false;
    timer->done = false;
}

uint32_t plc_timer_elapsed_ms(const plc_timer_t *timer)
{
    return timer->accumulated_ms;
}

uint32_t plc_timer_remaining_ms(const plc_timer_t *timer)
{
    if (timer->accumulated_ms >= timer->preset_ms) return 0;
    return timer->preset_ms - timer->accumulated_ms;
}

void plc_counter_reset(plc_counter_t *counter)
{
    counter->accumulated = 0;
    counter->done = false;
    counter->overflow = false;
    counter->underflow = false;
}

uint16_t plc_counter_value(const plc_counter_t *counter)
{
    return counter->accumulated;
}

bool plc_counter_done(const plc_counter_t *counter)
{
    return counter->done;
}
