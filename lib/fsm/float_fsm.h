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
    FLOAT_DUMPING_DATA,
    FLOAT_TEST_CALIBRATE
} FloatState_t;

typedef enum {
    STAGE_DEEP,
    STAGE_SHALLOW,
    STAGE_EXITING
} MissionStage_t;

// --- FSM Structure to hold all runtime context ---
typedef struct {
    FloatState_t state;
    MissionStage_t mission_stage;
    uint16_t current_profile;
    bool currently_transmitting;
    uint32_t profile_start_time;
    uint32_t last_tx_time;
    uint32_t last_sample_time;
    uint32_t last_debug_print;
    uint16_t current_seq_num;
    uint16_t sample_index;
    uint16_t actuator_target;
    uint16_t current_actuator_pos;
    bool manual_move_pending;
    bool target_depth_reached;
    float current_depth;
    
    // --- Stall Detection (Early Abort) ---
    uint32_t last_stall_check_time;
    float stall_reference_depth;

    // --- Adaptive Neutral Learning ---
    uint32_t hover_accumulated_adc;
    uint32_t hover_sample_count;

    MS5837_t *depth_sensor;
} float_fsm_t;

// --- Public API ---
void float_fsm_init(float_fsm_t *fsm, MS5837_t *sensor);
void float_fsm_update(float_fsm_t *fsm);
void float_fsm_process_event(float_fsm_t *fsm);

#endif // FLOAT_FSM_H