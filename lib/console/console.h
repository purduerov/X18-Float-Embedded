#ifndef CONSOLE_H
#define CONSOLE_H

#include "pico/stdlib.h"
#include <stddef.h>

/**
 * Command Handler Function Pointer
 * @param params: String containing arguments after the command character
 */
typedef void (*console_cmd_handler_t)(const char *params);

/**
 * Console Command Table Entry
 */
typedef struct {
    char cmd_char;
    console_cmd_handler_t handler;
    const char *description;
} console_command_t;

/**
 * Initialize the console interface
 * @param commands: Array of command entries
 * @param num_commands: Number of commands in the array
 */
void console_init(const console_command_t *commands, size_t num_commands);

/**
 * Process a single character from serial (non-blocking)
 */
void console_handle_char(char c);

/**
 * Process any pending serial characters (non-blocking)
 */
void console_update(void);

#endif // CONSOLE_H
