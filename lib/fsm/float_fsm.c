#include "float_fsm.h"
#include "hw_config.h"
#include "sw_config.h"
#include "neopixel.h"
#include "pico/bootrom.h"
#include "reflash_target.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// --- Internal Data Buffers ---
static float recorded_depths[MAX_RECORDED_SAMPLES];
static uint32_t recorded_times[MAX_RECORDED_SAMPLES];
static uint16_t recorded_adcs[MAX_RECORDED_SAMPLES];
static uint16_t recorded_target_adcs[MAX_RECORDED_SAMPLES];

const char *FloatStateNames[] = {"IDLE",         "PRE_DIVE",
                                 "PROFILING",    "PROFILE_DONE",
                                 "DUMPING_DATA", "TEST_CALIBRATE"};

static void update_status_led(FloatState_t state) {
  switch (state) {
  case FLOAT_IDLE:
    neopixel_set_color(COLOR_GREEN);
    break;
  case FLOAT_PRE_DIVE:
    neopixel_set_color(COLOR_YELLOW);
    break;
  case FLOAT_PROFILING:
    neopixel_set_color(COLOR_BLUE);
    break;
  case FLOAT_PROFILE_DONE:
    neopixel_set_color(COLOR_CYAN);
    break;
  case FLOAT_DUMPING_DATA:
    neopixel_set_color(COLOR_MAGENTA);
    break;
  case FLOAT_TEST_CALIBRATE:
    neopixel_set_color(COLOR_WHITE);
    break;
  default:
    neopixel_set_color(COLOR_RED);
    break;
  }
}

void float_fsm_init(float_fsm_t *fsm, MS5837_t *sensor) {
  memset(fsm, 0, sizeof(float_fsm_t));
  fsm->state = FLOAT_IDLE;
  fsm->mission_stage = STAGE_DEEP;
  fsm->current_profile = 0;
  fsm->depth_sensor = sensor;
  fsm->actuator_target = DEFAULT_ACTUATOR_POS;
  fsm->last_debug_print = to_ms_since_boot(get_absolute_time());
  update_status_led(fsm->state);
  radio_start_receive();
}

void float_fsm_process_event(float_fsm_t *fsm) {
  // This is called when the radio signals an operation is done (TX finished or
  // RX arrived)
  uint8_t buffer[256];

  if (fsm->currently_transmitting) {
    radio_finish_transmit();
    fsm->currently_transmitting = false;

    if (fsm->state == FLOAT_PRE_DIVE) {
      printf(">> Pre-dive packet sent. Starting mission sequencer (Radio "
             "SILENT)...\n");
      fsm->state = FLOAT_PROFILING;
      fsm->mission_stage = STAGE_DEEP;
      fsm->current_profile = 1;
      fsm->target_depth_reached = false;
      
      float_settings_t settings;
      storage_get_settings(&settings);
      fsm->actuator_target = settings.neutral_buoyancy_adc; // Sync target for logging
      
      update_status_led(fsm->state);
      fsm->profile_start_time = to_ms_since_boot(get_absolute_time());
      if (fsm->profile_start_time == 0)
        fsm->profile_start_time = 1; // Prevent 0
      fsm->last_sample_time = 0;     // Trigger immediate first sample
      fsm->sample_index = 0;

      // Initialize Stall Detection
      fsm->last_stall_check_time = fsm->profile_start_time;
      fsm->stall_reference_depth = fsm->current_depth;

      // Initialize Adaptive Neutral Learning & Control State
      fsm->hover_accumulated_adc = 0;
      fsm->hover_sample_count = 0;
      fsm->ctrl_state = 0; // CTRL_TRANSIT
      fsm->filtered_velocity = 0.0f;
      fsm->last_depth = fsm->current_depth;
      fsm->last_nudge_time = fsm->profile_start_time;
      fsm->active_neutral_adc = settings.neutral_buoyancy_adc;
    }
    radio_start_receive();
  } else {
    int16_t len = radio_read_data(buffer, sizeof(buffer));
    if (len > 0) {
      // Give the OTA Reflasher the first chance at the packet
      if (reflash_target_process_packet(radio_get_instance(), buffer, len)) {
        // Packet consumed by OTA reflasher, do nothing more
      } else if (len == sizeof(packet_t)) {
        packet_t rx_pkt;
        memcpy(&rx_pkt, buffer, sizeof(packet_t));

        // Verify checksum before processing
        if (rx_pkt.checksum != packet_calculate_checksum(&rx_pkt)) {
          printf("[RADIO] ERROR: Packet Checksum Mismatch! Ignoring.\n");
        } else {
          float_settings_t settings;
          storage_get_settings(&settings);

          if (rx_pkt.command == CMD_RESET_FSM) {
            printf(">> Radio CMD: Resetting FSM to IDLE...\n");
            fsm->state = FLOAT_IDLE;
            fsm->current_profile = 0;
            update_status_led(fsm->state);
            fsm->currently_transmitting = false;
          } else if (fsm->state == FLOAT_IDLE ||
                     fsm->state == FLOAT_TEST_CALIBRATE) {
            if (rx_pkt.command == CMD_BEGIN_PROFILE &&
                fsm->state == FLOAT_IDLE) {
              printf(">> Received BEGIN_PROFILE. Triggering Pre-Dive "
                     "Transmission...\n");
              fsm->state = FLOAT_PRE_DIVE;
              update_status_led(fsm->state);
            } else if (rx_pkt.command == CMD_ENTER_TEST &&
                       fsm->state == FLOAT_IDLE) {
              printf(
                  ">> Received ENTER_TEST. Starting live telemetry dump...\n");
              fsm->state = FLOAT_TEST_CALIBRATE;
              update_status_led(fsm->state);
              fsm->last_tx_time = 0; // Trigger immediate transmit
            } else if (rx_pkt.command == CMD_SET_PID) {
              settings.kp = rx_pkt.payload.settings.kp;
              settings.ki = rx_pkt.payload.settings.ki;
              settings.kd = rx_pkt.payload.settings.kd;
              printf(">> PID Updated: P=%.2f, I=%.2f, D=%.2f. Saving to "
                     "Flash...\n",
                     settings.kp, settings.ki, settings.kd);
              storage_set_settings(&settings);
              storage_save();
            } else if (rx_pkt.command == CMD_SET_COMPANY) {
              settings.company_number = rx_pkt.payload.telemetry.company_number;
              printf(">> Company ID Updated: %u. Saving to Flash...\n",
                     settings.company_number);
              storage_set_settings(&settings);
              storage_save();
            } else if (rx_pkt.command == CMD_SET_DURATION) {
              settings.profile_duration_s =
                  rx_pkt.payload.settings.profile_duration_s;
              printf(">> Profile Duration Updated: %u seconds. Saving to "
                     "Flash...\n",
                     settings.profile_duration_s);
              storage_set_settings(&settings);
              storage_save();
            } else if (rx_pkt.command == CMD_SET_DEEP_TARGET) {
              settings.deep_target_m = rx_pkt.payload.settings.deep_target_m;
              printf(">> Deep Target Updated: %.2f m. Saving to Flash...\n",
                     settings.deep_target_m);
              storage_set_settings(&settings);
              storage_save();
            } else if (rx_pkt.command == CMD_SET_SHALLOW_TARGET) {
              settings.shallow_target_m =
                  rx_pkt.payload.settings.shallow_target_m;
              printf(">> Shallow Target Updated: %.2f m. Saving to Flash...\n",
                     settings.shallow_target_m);
              storage_set_settings(&settings);
              storage_save();
            } else if (rx_pkt.command == CMD_SET_NUM_PROFILES) {
              settings.num_profiles = rx_pkt.payload.settings.num_profiles;
              printf(">> Number of Profiles Updated: %u. Saving to Flash...\n",
                     settings.num_profiles);
              storage_set_settings(&settings);
              storage_save();
            } else if (rx_pkt.command == CMD_SET_TOLERANCE) {
              settings.arrival_band_m = rx_pkt.payload.settings.arrival_band_m;
              printf(
                  ">> Arrival Tolerance Updated: %.2f m. Saving to Flash...\n",
                  settings.arrival_band_m);
              storage_set_settings(&settings);
              storage_save();
            } else if (rx_pkt.command == CMD_ZERO_DEPTH) {
              ms5837_read(fsm->depth_sensor);
              settings.depth_offset = ms5837_get_depth(fsm->depth_sensor);
              printf(">> Depth Zeroed at: %.3f m. Saving to Flash...\n",
                     settings.depth_offset);
              storage_set_settings(&settings);
              storage_save();
            } else if (rx_pkt.command == CMD_SET_ACTUATOR) {
              fsm->actuator_target = rx_pkt.payload.settings.actuator_target;
              fsm->manual_move_pending = true;
              printf(">> Radio CMD: Set Actuator Target to %u\n",
                     fsm->actuator_target);
            } else if (rx_pkt.command == CMD_SET_ACT_BOUNDS) {
              settings.act_min = rx_pkt.payload.settings.act_min;
              settings.act_max = rx_pkt.payload.settings.act_max;
              printf(">> Actuator Bounds Updated: Min=%u, Max=%u. Saving to "
                     "Flash...\n",
                     settings.act_min, settings.act_max);
              storage_set_settings(&settings);
              storage_save();
            } else if (rx_pkt.command == CMD_SET_NEUTRAL_ADC) {
              settings.neutral_buoyancy_adc =
                  rx_pkt.payload.settings.neutral_buoyancy_adc;
              printf(">> Neutral ADC Updated: %u. Saving to Flash...\n",
                     settings.neutral_buoyancy_adc);
              storage_set_settings(&settings);
              storage_save();
            } else if (rx_pkt.command == CMD_REQ_SETTINGS) {
              printf(">> Received REQ_SETTINGS. Transmitting Flash config back "
                     "to surface...\n");
              packet_t tx_pkt = {.command = CMD_REP_SETTINGS, .seq_num = 0};
              tx_pkt.payload.settings.kp = settings.kp;
              tx_pkt.payload.settings.ki = settings.ki;
              tx_pkt.payload.settings.kd = settings.kd;
              tx_pkt.payload.settings.deep_target_m = settings.deep_target_m;
              tx_pkt.payload.settings.shallow_target_m =
                  settings.shallow_target_m;
              tx_pkt.payload.settings.num_profiles = settings.num_profiles;
              tx_pkt.payload.settings.depth_offset = settings.depth_offset;
              tx_pkt.payload.settings.company_number = settings.company_number;
              tx_pkt.payload.settings.profile_duration_s =
                  settings.profile_duration_s;
              tx_pkt.payload.settings.actuator_target = fsm->actuator_target;
              tx_pkt.payload.settings.current_actuator_pos =
                  fsm->current_actuator_pos;
              tx_pkt.payload.settings.act_min = settings.act_min;
              tx_pkt.payload.settings.act_max = settings.act_max;
              tx_pkt.payload.settings.neutral_buoyancy_adc =
                  settings.neutral_buoyancy_adc;
              tx_pkt.payload.settings.arrival_band_m = settings.arrival_band_m;
              tx_pkt.payload.settings.live_depth = fsm->current_depth;
              tx_pkt.payload.settings.fw_version = FIRMWARE_VERSION;
              tx_pkt.checksum = packet_calculate_checksum(&tx_pkt);
              fsm->currently_transmitting = true;
              radio_start_transmit((uint8_t *)&tx_pkt, sizeof(packet_t));
            }
          } else if (fsm->state == FLOAT_PROFILE_DONE) {
            if (rx_pkt.command == CMD_SEND_DATA) {
              printf(">> Received SEND_DATA command. Starting data dump for "
                     "scoring...\n");
              fsm->state = FLOAT_DUMPING_DATA;
              update_status_led(fsm->state);
              fsm->current_seq_num = 1;
              fsm->last_tx_time = 0;
            }
          } else if (fsm->state == FLOAT_DUMPING_DATA) {
            if (rx_pkt.command == CMD_ACK &&
                rx_pkt.seq_num == fsm->current_seq_num) {
              printf(">> Received ACK for packet %d.\n", fsm->current_seq_num);
              fsm->current_seq_num++;

              if (fsm->current_seq_num > fsm->sample_index) {
                printf(">> All data sent. Sending DATA_DONE...\n");
                packet_t done_pkt = {.command = CMD_DATA_DONE, .seq_num = 0};
                done_pkt.checksum = packet_calculate_checksum(&done_pkt);
                fsm->currently_transmitting = true;
                radio_start_transmit((uint8_t *)&done_pkt, sizeof(packet_t));
                fsm->state = FLOAT_IDLE;
                update_status_led(fsm->state);
              } else {
                fsm->last_tx_time = 0;
              }
            }
          }
        }
      }
    }
    if (!fsm->currently_transmitting) {
      radio_start_receive();
    }
  }
}

void float_fsm_update(float_fsm_t *fsm) {
  uint32_t now = to_ms_since_boot(get_absolute_time());
  float_settings_t settings;
  storage_get_settings(&settings);

  // Debug Printing
  if (now - fsm->last_debug_print >= 2000) {
    if (!reflash_target_is_in_progress()) {
      printf("[DEBUG] State: %s | Transmitting: %d | ADC: %u | Depth: %.3f m\n",
             FloatStateNames[fsm->state], fsm->currently_transmitting,
             fsm->current_actuator_pos, fsm->current_depth);
    }
    fsm->last_debug_print = now;
  }

  // State Logic
  if (fsm->state == FLOAT_PRE_DIVE && !fsm->currently_transmitting) {
    printf(">> Sending Pre-Dive Data Packet...\n");
    packet_t tx_pkt = {.command = CMD_DATA_TRANSMISSION, .seq_num = 0};
    tx_pkt.payload.telemetry.company_number = settings.company_number;
    tx_pkt.payload.telemetry.time_ms = now;

    // Use cached depth from main loop
    tx_pkt.payload.telemetry.depth_m = fsm->current_depth;

    tx_pkt.payload.telemetry.actuator_pos = fsm->current_actuator_pos;
    tx_pkt.payload.telemetry.target_actuator_pos = fsm->actuator_target;
    tx_pkt.checksum = packet_calculate_checksum(&tx_pkt);
    fsm->currently_transmitting = true;
    radio_start_transmit((uint8_t *)&tx_pkt, sizeof(packet_t));
  } else if (fsm->state == FLOAT_PROFILING) {
    float target_m = 0.0f;
    const char *stage_name = "UNKNOWN";

    if (fsm->mission_stage == STAGE_DEEP) {
      target_m = settings.deep_target_m;
      stage_name = "DEEP";
    } else if (fsm->mission_stage == STAGE_SHALLOW) {
      target_m = settings.shallow_target_m;
      stage_name = "SHALLOW";
    } else if (fsm->mission_stage == STAGE_EXITING) {
      target_m = -0.5f; // Command full buoyancy
      stage_name = "EXITING";
    }

    // Depth Arrival & Consecutive Hold Logic
    float depth_error = fsm->current_depth - target_m;
    if (depth_error < 0)
      depth_error = -depth_error;

    if (!fsm->target_depth_reached) {
      bool arrived = false;
      if (fsm->mission_stage == STAGE_EXITING) {
        if (fsm->current_depth <= 0.05f)
          arrived = true;
      } else {
        if (depth_error <= settings.arrival_band_m)
          arrived = true;
      }

      if (arrived) {
        if (fsm->mission_stage == STAGE_EXITING) {
          printf(">> MISSION: Surface reached. Profile Done.\n");
          fsm->state = FLOAT_PROFILE_DONE;
          fsm->actuator_target = settings.act_max;
          update_status_led(fsm->state);
        } else {
          printf(">> MISSION: Arrived at %s (%.2fm). Starting %u sec hold...\n",
                  stage_name, target_m, settings.profile_duration_s);
          fsm->target_depth_reached = true;
          fsm->hover_accumulated_adc = 0;
          fsm->hover_sample_count = 0;
          fsm->profile_start_time = now;
          if (fsm->profile_start_time == 0)
            fsm->profile_start_time = 1;
          
          if (fsm->mission_stage == STAGE_DEEP) {
            fsm->sample_index = (fsm->current_profile - 1) * 14;
          } else if (fsm->mission_stage == STAGE_SHALLOW) {
            fsm->sample_index = (fsm->current_profile - 1) * 14 + 7;
          }
        }
      }
    } else {
      // Reset timer if we drift out of band (Consecutive Hold requirement)
      if (fsm->mission_stage != STAGE_EXITING &&
          depth_error > settings.arrival_band_m) {
        printf("!! MISSION: Drifted out of band! Resetting %s timer and logs.\n",
               stage_name);
        fsm->target_depth_reached = false;
        fsm->hover_accumulated_adc = 0;
        fsm->hover_sample_count = 0;
        fsm->profile_start_time = now;
        
        // Reset sequential packet logging
        if (fsm->mission_stage == STAGE_DEEP) {
          fsm->sample_index = (fsm->current_profile - 1) * 14;
        } else if (fsm->mission_stage == STAGE_SHALLOW) {
          fsm->sample_index = (fsm->current_profile - 1) * 14 + 7;
        }
      }
    }

    // Sampling Loop (5s interval, hold phase only)
    if (fsm->target_depth_reached && fsm->profile_start_time > 0) {
      uint32_t elapsed_ms = now - fsm->profile_start_time;
      uint32_t stage_start_idx = 0;
      if (fsm->mission_stage == STAGE_DEEP) {
        stage_start_idx = (fsm->current_profile - 1) * 14;
      } else if (fsm->mission_stage == STAGE_SHALLOW) {
        stage_start_idx = (fsm->current_profile - 1) * 14 + 7;
      }
      
      uint32_t stage_sample_idx = fsm->sample_index - stage_start_idx;
      uint32_t expected_elapsed_ms = stage_sample_idx * 5000;
      
      if (elapsed_ms >= expected_elapsed_ms && stage_sample_idx < 7 && fsm->sample_index < MAX_RECORDED_SAMPLES) {
        recorded_times[fsm->sample_index] = expected_elapsed_ms; 
        recorded_depths[fsm->sample_index] = fsm->current_depth;
        recorded_adcs[fsm->sample_index] = fsm->current_actuator_pos;
        recorded_target_adcs[fsm->sample_index] = fsm->actuator_target;
        
        // Adaptive Neutral Learning: Accumulate actuator position while holding close to target
        float err_val = fsm->current_depth - target_m;
        if (err_val < 0) err_val = -err_val;
        if (err_val <= 0.15f) { 
          fsm->hover_accumulated_adc += fsm->current_actuator_pos;
          fsm->hover_sample_count++;
        }
        
        printf(">> Hold Sample %u/7 [%s]: Time %lu s | Depth %.2f m | ADC %u | TargetADC %u\n",
               stage_sample_idx + 1,
               stage_name, expected_elapsed_ms / 1000,
               recorded_depths[fsm->sample_index], recorded_adcs[fsm->sample_index],
               recorded_target_adcs[fsm->sample_index]);
               
        fsm->sample_index++;
        fsm->last_sample_time = now;
      }
    }

    // --- Stall Detection (Early Abort) ---
    // If we are supposed to be moving (diving/rising) but depth hasn't changed...
    if (!fsm->target_depth_reached && (now - fsm->last_stall_check_time >= STALL_CHECK_DURATION_MS)) {
        float depth_change = fabsf(fsm->current_depth - fsm->stall_reference_depth);
        if (depth_change < STALL_DEPTH_THRESHOLD_M) {
            printf("!! [STALL] No depth change detected (%.3fm). Aborting mission...\n", depth_change);
            fsm->mission_stage = STAGE_EXITING;
            fsm->target_depth_reached = false;
            fsm->actuator_target = settings.act_max;
            update_status_led(fsm->state);
        }
        fsm->last_stall_check_time = now;
        fsm->stall_reference_depth = fsm->current_depth;
    }

    // Mission Progression Logic
    if (fsm->profile_start_time > 0) {
      uint32_t elapsed = now - fsm->profile_start_time;
      bool buffer_full = (fsm->sample_index >= MAX_RECORDED_SAMPLES);
      bool stage_complete =
          (fsm->target_depth_reached &&
           (elapsed >= (uint32_t)settings.profile_duration_s * 1000));
      bool safety_timeout =
          (!fsm->target_depth_reached &&
           (elapsed >= (uint32_t)(settings.profile_duration_s +
                                  PROFILING_SAFETY_TIMEOUT_S) *
                           1000));

            if (stage_complete) {
                // Adaptive Neutral Learning: Compute and update learned neutral point
                if (fsm->hover_sample_count >= 3) {
                    uint16_t learned_neutral = fsm->hover_accumulated_adc / fsm->hover_sample_count;
                    if (learned_neutral >= 1300 && learned_neutral <= 2500) {
                        float_settings_t live_settings;
                        storage_get_settings(&live_settings);
                        live_settings.neutral_buoyancy_adc = learned_neutral;
                        storage_set_settings(&live_settings);
                        printf(">> [ADAPTIVE] Hold complete. Learned neutral buoyancy ADC: %u (updated in RAM)\n", learned_neutral);
                    }
                }

                if (fsm->mission_stage == STAGE_DEEP) {
                    // Check if shallow stage is disabled (e.g. 0.0m)
                    if (settings.shallow_target_m <= 0.05f) {
                        printf(">> MISSION: Deep stage %u/%u done. Shallow disabled.\n", fsm->current_profile, settings.num_profiles);
                        if (fsm->current_profile >= settings.num_profiles) {
                            printf(">> MISSION: Moving to EXITING stage.\n");
                            fsm->mission_stage = STAGE_EXITING;
                            fsm->target_depth_reached = false;
                            fsm->actuator_target = settings.act_max; 
                            fsm->ctrl_state = 0;
                        } else {
                            fsm->current_profile++;
                            fsm->mission_stage = STAGE_DEEP;
                            fsm->target_depth_reached = false;
                            fsm->profile_start_time = now;
                            fsm->ctrl_state = 0;
                            fsm->active_neutral_adc = settings.neutral_buoyancy_adc;
                        }
                    } else {
                        printf(">> MISSION: Deep stage %u/%u done. Moving to SHALLOW.\n", fsm->current_profile, settings.num_profiles);
                        fsm->mission_stage = STAGE_SHALLOW;
                        fsm->target_depth_reached = false;
                        fsm->profile_start_time = now;
                        fsm->ctrl_state = 0;
                        fsm->active_neutral_adc = settings.neutral_buoyancy_adc;
                    }
                } else {
                    printf(">> MISSION: Shallow stage %u/%u done.\n", fsm->current_profile, settings.num_profiles);
                    if (fsm->current_profile >= settings.num_profiles) {
                        printf(">> MISSION: Moving to EXITING stage.\n");
                        fsm->mission_stage = STAGE_EXITING;
                        fsm->target_depth_reached = false;
                        fsm->actuator_target = settings.act_max;
                        fsm->ctrl_state = 0;
                    } else {
                        fsm->current_profile++;
                        fsm->mission_stage = STAGE_DEEP;
                        fsm->target_depth_reached = false;
                        fsm->profile_start_time = now;
                        fsm->ctrl_state = 0;
                        fsm->active_neutral_adc = settings.neutral_buoyancy_adc;
                    }
                }
                update_status_led(fsm->state);
            } else if (safety_timeout || buffer_full) {
                if (safety_timeout) printf("!! [SAFETY] Mission Timeout in %s. Surfacing...\n", stage_name);
                else printf(">> [INFO] Data Buffer Full. Surfacing...\n");
                printf(">> MISSION: Moving to EXITING stage.\n");
                fsm->mission_stage = STAGE_EXITING;
                fsm->target_depth_reached = false;
                fsm->actuator_target = settings.act_max;
                fsm->ctrl_state = 0;
                update_status_led(fsm->state);
            }
    }
  } else if (fsm->state == FLOAT_PROFILE_DONE && !fsm->currently_transmitting) {
    if (now - fsm->last_tx_time >= RADIO_DONE_BROADCAST_MS) {
      printf(">> Broadcasting DONE_PROFILE (Waiting for Recovery / SEND_DATA "
             "CMD)...\n");
      packet_t tx_pkt = {.command = CMD_DONE_PROFILE, .seq_num = 0};
      tx_pkt.checksum = packet_calculate_checksum(&tx_pkt);
      fsm->currently_transmitting = true;
      radio_start_transmit((uint8_t *)&tx_pkt, sizeof(packet_t));
      fsm->last_tx_time = now;
    }
  } else if (fsm->state == FLOAT_DUMPING_DATA && !fsm->currently_transmitting) {
    if (now - fsm->last_tx_time >= RADIO_DATA_RETRANSMIT_MS) {
      printf(">> Sending/Retransmitting Data Packet %d...\n",
             fsm->current_seq_num);
      packet_t tx_pkt = {.command = CMD_DATA_TRANSMISSION,
                         .seq_num = fsm->current_seq_num};
      tx_pkt.payload.telemetry.company_number = settings.company_number;
      tx_pkt.payload.telemetry.time_ms =
          recorded_times[fsm->current_seq_num - 1];
      tx_pkt.payload.telemetry.depth_m =
          recorded_depths[fsm->current_seq_num - 1];
      tx_pkt.payload.telemetry.actuator_pos =
          recorded_adcs[fsm->current_seq_num - 1];
      tx_pkt.payload.telemetry.target_actuator_pos =
          recorded_target_adcs[fsm->current_seq_num - 1];
      tx_pkt.checksum = packet_calculate_checksum(&tx_pkt);
      fsm->currently_transmitting = true;
      radio_start_transmit((uint8_t *)&tx_pkt, sizeof(packet_t));
      fsm->last_tx_time = now;
    }
  } else if (fsm->state == FLOAT_TEST_CALIBRATE &&
             !fsm->currently_transmitting) {
    if (now - fsm->last_tx_time >= RADIO_TEST_TX_MS) {
      packet_t tx_pkt = {.command = CMD_REP_TEST_DATA, .seq_num = 0};
      tx_pkt.payload.test_data.live_depth = fsm->current_depth;
      tx_pkt.payload.test_data.live_adc = fsm->current_actuator_pos;
      tx_pkt.checksum = packet_calculate_checksum(&tx_pkt);

      fsm->currently_transmitting = true;
      radio_start_transmit((uint8_t *)&tx_pkt, sizeof(packet_t));
      fsm->last_tx_time = now;
    }
  }
}
