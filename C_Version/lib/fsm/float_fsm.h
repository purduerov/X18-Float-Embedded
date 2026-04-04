#ifndef FLOAT_FSM_H
#define FLOAT_FSM_H

#include "pico/stdlib.h"
#include "packets.h"
#include "storage.h"
#include "radio_setup.h"
#include "ms5837.h"

// --- State Definitions ---
typedef enum
{
    FLOAT_IDLE,
    FLOAT_PRE_DIVE,      
    FLOAT_PROFILING,
    FLOAT_PROFILE_DONE,
    FLOAT_DUMPING_DATA
} FloatState_t;

// --- FSM Structure to hold all runtime context ---
typedef struct {
    FloatState_t state;
    bool currently_transmitting;
    uint32_t profile_start_time;
    uint32_t last_tx_time;
    uint32_t last_sample_time;
    uint32_t last_debug_print;
    uint16_t current_seq_num;
    uint16_t sample_index;
    uint16_t actuator_target;
    uint16_t current_actuator_pos;
    MS5837_t *depth_sensor;
} float_fsm_t;

// --- Public API ---
void float_fsm_init(float_fsm_t *fsm, MS5837_t *sensor);
void float_fsm_update(float_fsm_t *fsm);
void float_fsm_process_event(float_fsm_t *fsm);

#endif // FLOAT_FSM_H