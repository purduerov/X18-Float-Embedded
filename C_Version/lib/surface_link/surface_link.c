#include "surface_link.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static const surface_command_t *cmd_table = NULL;
static size_t cmd_count = 0;

static char input_line[64];
static int input_pos = 0;

void surface_link_init(const surface_command_t *commands, size_t num_commands) {
    cmd_table = commands;
    cmd_count = num_commands;
    input_pos = 0;
}

static void dispatch_command(const char *line) {
    if (!line || strlen(line) == 0) return;

    char cmd_char = line[0];
    const char *params = line + 1;

    // Skip leading whitespace in params
    while (*params && isspace((unsigned char)*params)) {
        params++;
    }

    for (size_t i = 0; i < cmd_count; i++) {
        if (tolower((unsigned char)cmd_char) == tolower((unsigned char)cmd_table[i].cmd_char)) {
            if (cmd_table[i].handler) {
                cmd_table[i].handler(params);
            }
            return;
        }
    }

    printf("\n[SERIAL] Unknown command: %c\n", cmd_char);
}

void surface_link_handle_char(char c) {
    if (c == '\n' || c == '\r') {
        if (input_pos > 0) {
            input_line[input_pos] = '\0';
            printf("\n[SERIAL] Received: %s\n", input_line);
            dispatch_command(input_line);
            input_pos = 0;
        }
    } else if (c == '\b' || c == 127) {
        if (input_pos > 0) {
            input_pos--;
        }
    } else if (c >= 32 && c <= 126) {
        if (input_pos < sizeof(input_line) - 1) {
            input_line[input_pos++] = c;
        }
    }
}

void surface_link_update(void) {
    int c;
    while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        surface_link_handle_char((char)c);
    }
}
