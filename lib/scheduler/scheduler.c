#include "scheduler.h"

void scheduler_init(Scheduler *s) {
    s->task_count = 0;
}

void scheduler_add_task(Scheduler *s, task_ptr func, uint32_t rate_ms) {
    if (s->task_count < MAX_TASKS) {
        s->tasks[s->task_count].func = func;
        s->tasks[s->task_count].rate_ms = rate_ms;
        s->tasks[s->task_count].last_run = 0;
        s->task_count++;
    }
}

void scheduler_tick(Scheduler *s) {
    // Get the current time in milliseconds since the Pico started
    uint32_t now = to_ms_since_boot(get_absolute_time());

    for (uint8_t i = 0; i < s->task_count; i++) {
        // Check if enough time has passed since the last run
        if (now - s->tasks[i].last_run >= s->tasks[i].rate_ms) {
            s->tasks[i].func();
            s->tasks[i].last_run = now;
        }
    }
}