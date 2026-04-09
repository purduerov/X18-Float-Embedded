#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "pico/stdlib.h"

// Define the maximum number of tasks allowed
#define MAX_TASKS 10

// Define a function pointer type for tasks
typedef void (*task_ptr)(void);

typedef struct {
    task_ptr func;      // Pointer to the function to run
    uint32_t rate_ms;   // Execution interval in milliseconds
    uint32_t last_run;  // System time of the last execution
} ScheduledTask;

typedef struct {
    ScheduledTask tasks[MAX_TASKS];
    uint8_t task_count;
} Scheduler;

void scheduler_init(Scheduler *s);
void scheduler_add_task(Scheduler *s, task_ptr func, uint32_t rate_ms);
void scheduler_tick(Scheduler *s);

#endif