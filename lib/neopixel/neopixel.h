#ifndef NEOPIXEL_H
#define NEOPIXEL_H

#include "hardware/pio.h"
#include "pico/stdlib.h"
#include <stdint.h>


// Standard Colors (GRB format for WS2812)
#define COLOR_BLACK 0x000000
#define COLOR_WHITE 0xFFFFFF
#define COLOR_RED 0x00FF00
#define COLOR_GREEN 0xFF0000
#define COLOR_BLUE 0x0000FF
#define COLOR_YELLOW 0xFFFF00
#define COLOR_CYAN 0xFF00FF
#define COLOR_MAGENTA 0x00FFFF

/**
 * @brief Initialize the NeoPixel on the specified pin using PIO.
 *
 * @param pin GPIO pin connected to the NeoPixel (GPIO 16 for Feather RP2040)
 */
void neopixel_init(uint pin, uint pwr);

/**
 * @brief Set the color of the single on-board NeoPixel.
 *
 * @param color 24-bit color in GRB format (use macros provided)
 */
void neopixel_set_color(uint32_t color);

/**
 * @brief Set the color with separate R, G, B components (0-255).
 */
void neopixel_set_rgb(uint8_t r, uint8_t g, uint8_t b);

#endif // NEOPIXEL_H
