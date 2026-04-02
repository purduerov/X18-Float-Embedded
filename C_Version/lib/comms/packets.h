#ifndef PACKETS_H
#define PACKETS_H

#include <stdint.h>
#include <stddef.h>

#define MAX_PAYLOAD_SIZE 24

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
    CMD_SET_ACTUATOR = 0x0E
} PacketCommand_t;

typedef struct __attribute__((packed))
{
    uint8_t command;
    uint16_t seq_num;
    union {
        struct {
            uint16_t company_number;
            uint32_t time_ms;
            float depth_m;
        } telemetry;         
        struct {
            float kp;
            float ki;
            float kd;
            uint16_t company_number;
            uint16_t profile_duration_s; 
            uint16_t actuator_target;
        } settings; 
        uint8_t raw[MAX_PAYLOAD_SIZE];     
    } payload;
    uint8_t checksum; // XOR checksum of all preceding bytes
} packet_t;

// Helper to calculate a simple XOR checksum for the packet
static inline uint8_t packet_calculate_checksum(const packet_t *pkt) {
    const uint8_t *data = (const uint8_t *)pkt;
    uint8_t checksum = 0;
    // Calculate over all bytes EXCEPT the checksum field itself (last byte)
    for (size_t i = 0; i < sizeof(packet_t) - 1; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

#endif // PACKETS_H