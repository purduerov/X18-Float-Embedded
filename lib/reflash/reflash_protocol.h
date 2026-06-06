#ifndef REFLASH_PROTOCOL_H
#define REFLASH_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#define REFLASH_CHUNK_SIZE 220
#define SLOT_0_OFFSET 0x00000000
#define SLOT_1_OFFSET 0x00100000 // 1MB Offset
#define SLOT_SIZE     0x00100000 // 1MB

#define REFLASH_BW_NORMAL 125.0f
#define REFLASH_BW_FAST   500.0f

typedef enum {
    REFLASH_MSG_START = 0xF1,
    REFLASH_MSG_DATA  = 0xF2,
    REFLASH_MSG_ACK   = 0xF3,
    REFLASH_MSG_NACK  = 0xF4,
    REFLASH_MSG_DONE  = 0xF5
} reflash_msg_type_t;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint32_t total_size;
    uint32_t master_crc;
} reflash_start_msg_t;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint32_t seq_num;
    uint8_t data[REFLASH_CHUNK_SIZE];
    uint32_t crc;
} reflash_data_msg_t;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint32_t seq_num;
} reflash_ack_msg_t;

#endif // REFLASH_PROTOCOL_H
