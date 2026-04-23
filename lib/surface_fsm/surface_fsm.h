#ifndef SURFACE_FSM_H
#define SURFACE_FSM_H

#include "pico/stdlib.h"
#include "packets.h"
#include <stdbool.h>

/**
 * Surface Station States
 */
typedef enum {
  SURFACE_IDLE,
  SURFACE_WAITING_PROFILE,
  SURFACE_DOWNLOADING
} SurfaceState_t;

/**
 * Surface FSM Object
 */
typedef struct {
    SurfaceState_t state;
    bool currently_transmitting;
    uint16_t expected_seq_num;
} surface_fsm_t;

/**
 * Initialize the Surface FSM
 */
void surface_fsm_init(surface_fsm_t *fsm);

/**
 * Process a Radio event (Interrupt received)
 */
void surface_fsm_process_event(surface_fsm_t *fsm);

/**
 * Get human-readable state name
 */
const char* surface_fsm_get_state_name(surface_fsm_t *fsm);

/**
 * Check if the FSM is currently busy transmitting
 */
bool surface_fsm_is_transmitting(surface_fsm_t *fsm);

// --- Dashboard Command API ---

void surface_fsm_cmd_begin_profile(surface_fsm_t *fsm);
void surface_fsm_cmd_set_pid(surface_fsm_t *fsm, float p, float i, float d);
void surface_fsm_cmd_set_company(surface_fsm_t *fsm, uint16_t id);
void surface_fsm_cmd_set_duration(surface_fsm_t *fsm, uint16_t seconds);
void surface_fsm_cmd_set_target_depth(surface_fsm_t *fsm, float depth);
void surface_fsm_cmd_zero_depth(surface_fsm_t *fsm);
void surface_fsm_cmd_set_actuator(surface_fsm_t *fsm, uint16_t position);
void surface_fsm_cmd_set_act_bounds(surface_fsm_t *fsm, uint16_t min_val, uint16_t max_val);
void surface_fsm_cmd_sync(surface_fsm_t *fsm);
void surface_fsm_cmd_reset(surface_fsm_t *fsm);
void surface_fsm_cmd_test_mode(surface_fsm_t *fsm);

#endif // SURFACE_FSM_H
