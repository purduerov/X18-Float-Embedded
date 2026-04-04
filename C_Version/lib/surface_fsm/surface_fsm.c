#include "surface_fsm.h"
#include "radio_setup.h"
#include "data_logger.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

static const char *SurfaceStateNames[] = {"IDLE", "WAITING_PROFILE", "DOWNLOADING"};

void surface_fsm_init(surface_fsm_t *fsm) {
    fsm->state = SURFACE_IDLE;
    fsm->currently_transmitting = false;
    fsm->expected_seq_num = 1;
}

const char* surface_fsm_get_state_name(surface_fsm_t *fsm) {
    if (fsm->state >= 0 && fsm->state < 3) {
        return SurfaceStateNames[fsm->state];
    }
    return "UNKNOWN";
}

bool surface_fsm_is_transmitting(surface_fsm_t *fsm) {
    return fsm->currently_transmitting;
}

static void send_packet(surface_fsm_t *fsm, packet_t *pkt) {
    sleep_ms(100);
    pkt->checksum = packet_calculate_checksum(pkt);
    fsm->currently_transmitting = true;
    radio_start_transmit((uint8_t *)pkt, sizeof(packet_t));
}

// --- Dashboard Command API Implementation ---

void surface_fsm_cmd_begin_profile(surface_fsm_t *fsm) {
    if (fsm->state == SURFACE_IDLE && !fsm->currently_transmitting) {
        printf(">> Commanding BEGIN_PROFILE...\n");
        packet_t tx_pkt = {.command = CMD_BEGIN_PROFILE, .seq_num = 0};
        send_packet(fsm, &tx_pkt);
        fsm->state = SURFACE_WAITING_PROFILE;
    }
}

void surface_fsm_cmd_set_pid(surface_fsm_t *fsm, float p, float i, float d) {
    if (fsm->state == SURFACE_IDLE && !fsm->currently_transmitting) {
        printf(">> Sending PID: P=%.2f, I=%.2f, D=%.2f\n", p, i, d);
        packet_t tx_pkt = {.command = CMD_SET_PID, .seq_num = 0};
        tx_pkt.payload.settings.kp = p;
        tx_pkt.payload.settings.ki = i;
        tx_pkt.payload.settings.kd = d;
        send_packet(fsm, &tx_pkt);
    }
}

void surface_fsm_cmd_set_company(surface_fsm_t *fsm, uint16_t id) {
    if (fsm->state == SURFACE_IDLE && !fsm->currently_transmitting) {
        printf(">> Sending Company ID Update: %u\n", id);
        packet_t tx_pkt = {.command = CMD_SET_COMPANY, .seq_num = 0};
        tx_pkt.payload.telemetry.company_number = id;
        send_packet(fsm, &tx_pkt);
    }
}

void surface_fsm_cmd_set_duration(surface_fsm_t *fsm, uint16_t seconds) {
    if (fsm->state == SURFACE_IDLE && !fsm->currently_transmitting) {
        printf(">> Sending Duration Update: %u sec\n", seconds);
        packet_t tx_pkt = {.command = CMD_SET_DURATION, .seq_num = 0};
        tx_pkt.payload.settings.profile_duration_s = seconds;
        send_packet(fsm, &tx_pkt);
    }
}

void surface_fsm_cmd_zero_depth(surface_fsm_t *fsm) {
    if (fsm->state == SURFACE_IDLE && !fsm->currently_transmitting) {
        printf(">> Sending ZERO_DEPTH command via radio...\n");
        packet_t tx_pkt = {.command = CMD_ZERO_DEPTH, .seq_num = 0};
        send_packet(fsm, &tx_pkt);
    }
}

void surface_fsm_cmd_set_actuator(surface_fsm_t *fsm, uint16_t position) {
    if (fsm->state == SURFACE_IDLE && !fsm->currently_transmitting) {
        printf(">> Sending SET_ACTUATOR (%u) command via radio...\n", position);
        packet_t tx_pkt = {.command = CMD_SET_ACTUATOR, .seq_num = 0};
        tx_pkt.payload.settings.actuator_target = position;
        send_packet(fsm, &tx_pkt);
    }
}

void surface_fsm_cmd_set_act_bounds(surface_fsm_t *fsm, uint16_t min_val, uint16_t max_val) {
    if (fsm->state == SURFACE_IDLE && !fsm->currently_transmitting) {
        printf(">> Sending Actuator Bounds: Min=%u, Max=%u\n", min_val, max_val);
        packet_t tx_pkt = {.command = CMD_SET_ACT_BOUNDS, .seq_num = 0};
        tx_pkt.payload.settings.act_min = min_val;
        tx_pkt.payload.settings.act_max = max_val;
        send_packet(fsm, &tx_pkt);
    }
}

void surface_fsm_cmd_sync(surface_fsm_t *fsm) {
    if (fsm->state == SURFACE_IDLE && !fsm->currently_transmitting) {
        printf(">> Requesting current float settings...\n");
        packet_t tx_pkt = {.command = CMD_REQ_SETTINGS, .seq_num = 0};
        send_packet(fsm, &tx_pkt);
    }
}

void surface_fsm_cmd_reset(surface_fsm_t *fsm) {
    printf(">> FORCING FSM RESET TO IDLE...\n");
    fsm->state = SURFACE_IDLE;
    fsm->currently_transmitting = false;
    fsm->expected_seq_num = 1;
    
    packet_t tx_pkt = {.command = CMD_RESET_FSM, .seq_num = 0};
    send_packet(fsm, &tx_pkt);
    // radio_start_receive() will be called when transmission finishes in process_event
}

// --- Radio Event Processor ---

void surface_fsm_process_event(surface_fsm_t *fsm) {
    if (fsm->currently_transmitting) {
        radio_finish_transmit();
        fsm->currently_transmitting = false;
        radio_start_receive();
        return;
    }

    uint8_t buffer[256];
    int16_t len = radio_read_data(buffer, sizeof(buffer));

    if (len == sizeof(packet_t)) {
        packet_t rx_pkt;
        memcpy(&rx_pkt, buffer, sizeof(packet_t));

        if (rx_pkt.checksum != packet_calculate_checksum(&rx_pkt)) {
            printf("[RADIO] ERROR: Packet Checksum Mismatch! Ignoring.\n");
        } else {
            // State Machine Response Logic
            if (fsm->state == SURFACE_IDLE) {
                if (rx_pkt.command == CMD_REP_SETTINGS) {
                    printf("\n[SYNC] P=%.2f, I=%.2f, D=%.2f, Co#=%u, Time=%u, ADC=%u, ActMin=%u, ActMax=%u\n",
                        rx_pkt.payload.settings.kp, rx_pkt.payload.settings.ki,
                        rx_pkt.payload.settings.kd,
                        rx_pkt.payload.settings.company_number,
                        rx_pkt.payload.settings.profile_duration_s,
                        rx_pkt.payload.settings.current_actuator_pos,
                        rx_pkt.payload.settings.act_min,
                        rx_pkt.payload.settings.act_max);
                }
            } else if (fsm->state == SURFACE_WAITING_PROFILE) {
                if (rx_pkt.command == CMD_DATA_TRANSMISSION && rx_pkt.seq_num == 0) {
                    printf(">> PRE-DIVE Packet Logged: Co# %u | Time %lu ms | Depth %.2f m\n",
                           rx_pkt.payload.telemetry.company_number,
                           rx_pkt.payload.telemetry.time_ms,
                           rx_pkt.payload.telemetry.depth_m);
                } else if (rx_pkt.command == CMD_DONE_PROFILE) {
                    printf(">> Float finished profile! Sending SEND_DATA command...\n");
                    data_logger_reset();
                    packet_t tx_pkt = {.command = CMD_SEND_DATA, .seq_num = 0};
                    send_packet(fsm, &tx_pkt);
                    fsm->state = SURFACE_DOWNLOADING;
                    fsm->expected_seq_num = 1;
                }
            } else if (fsm->state == SURFACE_DOWNLOADING) {
                if (rx_pkt.command == CMD_DONE_PROFILE) {
                    printf(">> Float missed SEND_DATA. Re-sending...\n");
                    packet_t tx_pkt = {.command = CMD_SEND_DATA, .seq_num = 0};
                    send_packet(fsm, &tx_pkt);
                } else if (rx_pkt.command == CMD_DATA_TRANSMISSION) {
                    if (rx_pkt.seq_num == fsm->expected_seq_num) {
                        data_logger_add_sample(rx_pkt.payload.telemetry.company_number,
                                             rx_pkt.payload.telemetry.time_ms,
                                             rx_pkt.payload.telemetry.depth_m);

                        printf(">> Stored Data #%d: Co# %u | Time %lu ms | Depth %.2f m\n",
                               rx_pkt.seq_num,
                               rx_pkt.payload.telemetry.company_number,
                               rx_pkt.payload.telemetry.time_ms,
                               rx_pkt.payload.telemetry.depth_m);
                        fsm->expected_seq_num++;
                    } else {
                        printf(">> Received duplicate/old packet #%d. Re-sending ACK.\n", rx_pkt.seq_num);
                    }

                    packet_t ack_pkt = {.command = CMD_ACK, .seq_num = rx_pkt.seq_num};
                    send_packet(fsm, &ack_pkt);
                } else if (rx_pkt.command == CMD_DATA_DONE) {
                    data_logger_dump_csv();
                    printf(">> Download Complete! Total Packets Received: %u\n", (unsigned int)data_logger_get_count());
                    fsm->state = SURFACE_IDLE;
                }
            }
        }
    }

    if (!fsm->currently_transmitting) {
        radio_start_receive();
    }
}
