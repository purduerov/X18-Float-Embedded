#include "neopixel.h"
#include "ws2812.pio.h"

static PIO pio_instance = pio0;
static uint sm_instance = 0;

void neopixel_init(uint pin, uint pwr) {
  gpio_init(pwr);
  gpio_set_dir(pwr, GPIO_OUT);
  gpio_put(pwr, 1);
  uint offset = pio_add_program(pio_instance, &ws2812_program);
  ws2812_program_init(pio_instance, sm_instance, offset, pin, 800000, false);
}

void neopixel_set_color(uint32_t color) {
  // WS2812 expects bits to be shifted such that the 24-bit GRB data is in the
  // MSB
  pio_sm_put_blocking(pio_instance, sm_instance, color << 8u);
}

void neopixel_set_rgb(uint8_t r, uint8_t g, uint8_t b) {
  // Convert to GRB format
  uint32_t color = ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b;
  neopixel_set_color(color);
}
