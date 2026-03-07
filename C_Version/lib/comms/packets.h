#ifndef PACKETS_H
#define PACKETS_H

#include <stdint.h>

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
    CMD_SET_DURATION = 0x0C // NEW: Command to set underwater time
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
            uint16_t profile_duration_s; // Added to sync payload
        } settings; 
        uint8_t raw[24]; // Expanded to 24 bytes for future expansion
    } payload;
} packet_t;

#endif // PACKETS_H