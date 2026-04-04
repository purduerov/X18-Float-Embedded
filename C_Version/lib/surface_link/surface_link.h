#ifndef SURFACE_LINK_H
#define SURFACE_LINK_H

#include "pico/stdlib.h"
#include <stddef.h>

/**
 * Command Handler Function Pointer
 * @param params: String containing arguments after the command character
 */
typedef void (*surface_cmd_handler_t)(const char *params);

/**
 * Surface Command Table Entry
 */
typedef struct {
    char cmd_char;
    surface_cmd_handler_t handler;
    const char *description;
} surface_command_t;

/**
 * Initialize the surface link
 * @param commands: Array of command entries
 * @param num_commands: Number of commands in the array
 */
void surface_link_init(const surface_command_t *commands, size_t num_commands);

/**
 * Process a single character from serial
 */
void surface_link_handle_char(char c);

/**
 * Process serial input and dispatch commands
 */
void surface_link_update(void);

#endif // SURFACE_LINK_H
