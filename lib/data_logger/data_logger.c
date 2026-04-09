#include "data_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SensorReading_t *data_buffer = NULL;
static size_t data_count = 0;
static size_t data_capacity = 0;

#define INITIAL_CAPACITY 16

void data_logger_init(void) {
    data_buffer = NULL;
    data_count = 0;
    data_capacity = 0;
}

void data_logger_reset(void) {
    if (data_buffer) {
        free(data_buffer);
        data_buffer = NULL;
    }
    data_count = 0;
    data_capacity = 0;
}

void data_logger_add_sample(uint16_t co, uint32_t time, float depth) {
    if (data_count >= data_capacity) {
        size_t new_capacity = (data_capacity == 0) ? INITIAL_CAPACITY : data_capacity * 2;
        SensorReading_t *new_buffer = (SensorReading_t *)realloc(data_buffer, new_capacity * sizeof(SensorReading_t));
        
        if (new_buffer) {
            data_buffer = new_buffer;
            data_capacity = new_capacity;
        } else {
            printf("[LOGGER] ERROR: Memory allocation failed!\n");
            return;
        }
    }

    data_buffer[data_count].company_number = co;
    data_buffer[data_count].time_ms = time;
    data_buffer[data_count].depth_m = depth;
    data_count++;
}

void data_logger_dump_csv(void) {
    printf("\n--- START DATA DUMP ---\n");
    printf("CompanyNumber,Time(ms),Depth(m)\n");
    for (size_t i = 0; i < data_count; i++) {
        printf("%u,%lu,%.2f\n", 
               data_buffer[i].company_number,
               data_buffer[i].time_ms,
               data_buffer[i].depth_m);
    }
    printf("--- END DATA DUMP ---\n");
}

size_t data_logger_get_count(void) {
    return data_count;
}
