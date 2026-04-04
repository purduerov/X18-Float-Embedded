#include "float_fsm.h"
#include "reflash_target.h"
#include <stdio.h>
#include <string.h>

// --- Configuration ---
#define SAMPLE_INTERVAL_MS 1000
#define MAX_PACKETS 400

// --- Internal Data Buffers ---
static float recorded_depths[MAX_PACKETS];
static uint32_t recorded_times[MAX_PACKETS];

const char *FloatStateNames[] = {
    "IDLE", "PRE_DIVE", "PROFILING", "PROFILE_DONE", "DUMPING_DATA"};

void float_fsm_init(float_fsm_t *fsm, MS5837_t *sensor) {
    memset(fsm, 0, sizeof(float_fsm_t));
    fsm->state = FLOAT_IDLE;
    fsm->depth_sensor = sensor;
    fsm->actuator_target = 2000;
    fsm->last_debug_print = to_ms_since_boot(get_absolute_time());
    radio_start_receive();
}

void float_fsm_process_event(float_fsm_t *fsm) {
    // This is called when the radio signals an operation is done (TX finished or RX arrived)
    uint8_t buffer[256];

    if (fsm->currently_transmitting) {
        radio_finish_transmit();
        fsm->currently_transmitting = false;

        if (fsm->state == FLOAT_PRE_DIVE) {
            printf(">> Pre-dive packet sent. Starting dive profiles (Radio SILENT)...\n");
            fsm->state = FLOAT_PROFILING;
            fsm->profile_start_time = to_ms_since_boot(get_absolute_time());
            fsm->last_sample_time = fsm->profile_start_time;
            fsm->sample_index = 0;
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

                    if (fsm->state == FLOAT_IDLE) {
                        if (rx_pkt.command == CMD_BEGIN_PROFILE) {
                            printf(">> Received BEGIN_PROFILE. Triggering Pre-Dive Transmission...\n");
                            fsm->state = FLOAT_PRE_DIVE;
                        } else if (rx_pkt.command == CMD_SET_PID) {
                            settings.kp = rx_pkt.payload.settings.kp;
                            settings.ki = rx_pkt.payload.settings.ki;
                            settings.kd = rx_pkt.payload.settings.kd;
                            printf(">> PID Updated: P=%.2f, I=%.2f, D=%.2f. Saving to Flash...\n", 
                                   settings.kp, settings.ki, settings.kd);
                            storage_set_settings(&settings);
                            storage_save();
                        } else if (rx_pkt.command == CMD_SET_COMPANY) {
                            settings.company_number = rx_pkt.payload.telemetry.company_number;
                            printf(">> Company ID Updated: %u. Saving to Flash...\n", settings.company_number);
                            storage_set_settings(&settings);
                            storage_save();
                        } else if (rx_pkt.command == CMD_SET_DURATION) {
                            settings.profile_duration_s = rx_pkt.payload.settings.profile_duration_s;
                            printf(">> Profile Duration Updated: %u seconds. Saving to Flash...\n", settings.profile_duration_s);
                            storage_set_settings(&settings);
                            storage_save();
                        } else if (rx_pkt.command == CMD_ZERO_DEPTH) {
                            ms5837_read(fsm->depth_sensor);
                            settings.depth_offset = ms5837_get_depth(fsm->depth_sensor);
                            printf(">> Depth Zeroed at: %.3f m. Saving to Flash...\n", settings.depth_offset);
                            storage_set_settings(&settings);
                            storage_save();
                        } else if (rx_pkt.command == CMD_SET_ACTUATOR) {
                            fsm->actuator_target = rx_pkt.payload.settings.actuator_target;
                            printf(">> Radio CMD: Set Actuator Target to %u\n", fsm->actuator_target);
                        } else if (rx_pkt.command == CMD_RESET_FSM) {
                            printf(">> Radio CMD: Resetting FSM to IDLE...\n");
                            fsm->state = FLOAT_IDLE;
                            fsm->currently_transmitting = false;
                        } else if (rx_pkt.command == CMD_REQ_SETTINGS) {
                            printf(">> Received REQ_SETTINGS. Transmitting Flash config back to surface...\n");
                            packet_t tx_pkt = {.command = CMD_REP_SETTINGS, .seq_num = 0};
                            tx_pkt.payload.settings.kp = settings.kp;
                            tx_pkt.payload.settings.ki = settings.ki;
                            tx_pkt.payload.settings.kd = settings.kd;
                            tx_pkt.payload.settings.company_number = settings.company_number;
                            tx_pkt.payload.settings.profile_duration_s = settings.profile_duration_s;
                            tx_pkt.payload.settings.actuator_target = fsm->actuator_target;
                            tx_pkt.payload.settings.current_actuator_pos = fsm->current_actuator_pos;
                            tx_pkt.checksum = packet_calculate_checksum(&tx_pkt);
                            fsm->currently_transmitting = true;
                            radio_start_transmit((uint8_t *)&tx_pkt, sizeof(packet_t));
                        }
                    } else if (fsm->state == FLOAT_PROFILE_DONE) {
                        if (rx_pkt.command == CMD_SEND_DATA) {
                            printf(">> Received SEND_DATA command. Starting data dump for scoring...\n");
                            fsm->state = FLOAT_DUMPING_DATA;
                            fsm->current_seq_num = 1;
                            fsm->last_tx_time = 0;
                        }
                    } else if (fsm->state == FLOAT_DUMPING_DATA) {
                        if (rx_pkt.command == CMD_ACK && rx_pkt.seq_num == fsm->current_seq_num) {
                            printf(">> Received ACK for packet %d.\n", fsm->current_seq_num);
                            fsm->current_seq_num++;

                            if (fsm->current_seq_num > fsm->sample_index) {
                                printf(">> All data sent. Sending DATA_DONE...\n");
                                packet_t done_pkt = {.command = CMD_DATA_DONE, .seq_num = 0};
                                done_pkt.checksum = packet_calculate_checksum(&done_pkt);
                                fsm->currently_transmitting = true;
                                radio_start_transmit((uint8_t *)&done_pkt, sizeof(packet_t));
                                fsm->state = FLOAT_IDLE;
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
        printf("[DEBUG] State: %s | Transmitting: %d | ADC: %u\n",
               FloatStateNames[fsm->state], fsm->currently_transmitting, fsm->current_actuator_pos);
        fsm->last_debug_print = now;
    }

    // State Logic
    if (fsm->state == FLOAT_PRE_DIVE && !fsm->currently_transmitting) {
        printf(">> Sending Pre-Dive Data Packet...\n");
        packet_t tx_pkt = {.command = CMD_DATA_TRANSMISSION, .seq_num = 0};
        tx_pkt.payload.telemetry.company_number = settings.company_number;
        tx_pkt.payload.telemetry.time_ms = now;
        tx_pkt.payload.telemetry.depth_m = 0.0f;
        tx_pkt.checksum = packet_calculate_checksum(&tx_pkt);
        fsm->currently_transmitting = true;
        radio_start_transmit((uint8_t *)&tx_pkt, sizeof(packet_t));
    } else if (fsm->state == FLOAT_PROFILING) {
        if (now - fsm->last_sample_time >= SAMPLE_INTERVAL_MS && fsm->sample_index < MAX_PACKETS) {
            ms5837_read(fsm->depth_sensor);
            recorded_depths[fsm->sample_index] = ms5837_get_depth(fsm->depth_sensor) - settings.depth_offset;
            recorded_times[fsm->sample_index] = now;
            printf(">> Sample %u/%u: Time %lu ms | Depth %.2f m\n",
                   fsm->sample_index + 1, MAX_PACKETS, recorded_times[fsm->sample_index], recorded_depths[fsm->sample_index]);
            fsm->sample_index++;
            fsm->last_sample_time = now;
        }
        if (now - fsm->profile_start_time >= (settings.profile_duration_s * 1000)) {
            printf(">> Profile complete (%u sec). Surfacing...\n", settings.profile_duration_s);
            fsm->state = FLOAT_PROFILE_DONE;
        }
    } else if (fsm->state == FLOAT_PROFILE_DONE && !fsm->currently_transmitting) {
        if (now - fsm->last_tx_time >= 3000) {
            printf(">> Broadcasting DONE_PROFILE (Waiting for Recovery / SEND_DATA CMD)...\n");
            packet_t tx_pkt = {.command = CMD_DONE_PROFILE, .seq_num = 0};
            tx_pkt.checksum = packet_calculate_checksum(&tx_pkt);
            fsm->currently_transmitting = true;
            radio_start_transmit((uint8_t *)&tx_pkt, sizeof(packet_t));
            fsm->last_tx_time = now;
        }
    } else if (fsm->state == FLOAT_DUMPING_DATA && !fsm->currently_transmitting) {
        if (now - fsm->last_tx_time >= 2000) {
            printf(">> Sending/Retransmitting Data Packet %d...\n", fsm->current_seq_num);
            packet_t tx_pkt = {.command = CMD_DATA_TRANSMISSION, .seq_num = fsm->current_seq_num};
            tx_pkt.payload.telemetry.company_number = settings.company_number;
            tx_pkt.payload.telemetry.time_ms = recorded_times[fsm->current_seq_num - 1];
            tx_pkt.payload.telemetry.depth_m = recorded_depths[fsm->current_seq_num - 1];
            tx_pkt.checksum = packet_calculate_checksum(&tx_pkt);
            fsm->currently_transmitting = true;
            radio_start_transmit((uint8_t *)&tx_pkt, sizeof(packet_t));
            fsm->last_tx_time = now;
        }
    }
}
