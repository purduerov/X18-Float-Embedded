#ifndef PACKETS_H
#define PACKETS_H

#include <stdint.h>
#include <stddef.h>

#define MAX_PAYLOAD_SIZE 32

typedef enum
{
    CMD_NONE = 0x00,
    CMD_SEND_DATA = 0x01,
    CMD_DATA_TRANSMISSION = 0x02,
    CMD_SET_PID = 0x03,
    CMD_BEGIN_PROFILE = 0x04,
    CMD_DONE_PROFILE = 0x05,
    CMD_DATA_DONE = 0x06,
    CMD_ACK = 0x07,
    CMD_PREDIVE_READY = 0x08,
    CMD_SET_COMPANY = 0x09,
    CMD_REQ_SETTINGS = 0x0A, 
    CMD_REP_SETTINGS = 0x0B,
    CMD_SET_DURATION = 0x0C,
    CMD_ZERO_DEPTH = 0x0D,
    CMD_SET_ACTUATOR = 0x0E,
    CMD_RESET_FSM = 0x0F,
    CMD_SET_ACT_BOUNDS = 0x10,
    CMD_ENTER_TEST = 0x11,
    CMD_REP_TEST_DATA = 0x12,
    CMD_BOOTLOADER = 0x13,
    CMD_SET_DEEP_TARGET = 0x14,
    CMD_SET_NEUTRAL_ADC = 0x15,
    CMD_SET_TOLERANCE = 0x16,
    CMD_SET_SHALLOW_TARGET = 0x17,
    CMD_SET_NUM_PROFILES = 0x18
} PacketCommand_t;

typedef struct __attribute__((packed))
{
    uint8_t command;
    uint16_t seq_num;
    union __attribute__((packed)) {
        struct __attribute__((packed)) {
            uint16_t company_number;
            uint32_t time_ms;
            float depth_m;
            float pressure_kpa; // Added for MATE 2026 compliance
            uint16_t actuator_pos;
            uint16_t target_actuator_pos;
        } telemetry;         
        struct __attribute__((packed)) {
            float kp;
            float ki;
            float kd;
            float deep_target_m;
            float shallow_target_m;
            float depth_offset;
            uint16_t company_number;
            uint16_t profile_duration_s; 
            uint16_t num_profiles;
            uint16_t actuator_target;
            uint16_t current_actuator_pos;
            uint16_t act_min;
            uint16_t act_max;
            uint16_t neutral_buoyancy_adc;
            float arrival_band_m;
            float live_depth;
            uint32_t fw_version;
        } settings; 
        struct __attribute__((packed)) {
            float live_depth;
            uint16_t live_adc;
        } test_data;
        uint8_t raw[MAX_PAYLOAD_SIZE];     
    } payload;
    uint32_t checksum; // CRC32 of all preceding bytes (0xEDB88320 polynomial)
} packet_t;

// CRC32 helper (standard 0xEDB88320 polynomial, same as crc32_software() used elsewhere)
static inline uint32_t packet_calculate_checksum(const packet_t *pkt) {
    const uint8_t *data = (const uint8_t *)pkt;
    uint32_t crc = 0xFFFFFFFFU;
    // Calculate over all bytes EXCEPT the checksum field itself (last 4 bytes)
    for (size_t i = 0; i < sizeof(packet_t) - sizeof(uint32_t); i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1U) {
                crc = (crc >> 1) ^ 0xEDB88320U;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc ^ 0xFFFFFFFFU;
}

#endif // PACKETS_H