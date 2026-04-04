#ifndef REFLASH_PROTOCOL_H
#define REFLASH_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#define REFLASH_CHUNK_SIZE 220
#define SLOT_0_OFFSET 0x00000000
#define SLOT_1_OFFSET 0x00100000 // 1MB Offset
#define SLOT_SIZE     0x00100000 // 1MB

typedef enum {
    REFLASH_MSG_START = 0x01,
    REFLASH_MSG_DATA  = 0x02,
    REFLASH_MSG_ACK   = 0x03,
    REFLASH_MSG_NACK  = 0x04,
    REFLASH_MSG_DONE  = 0x05
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
