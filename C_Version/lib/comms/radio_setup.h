#ifndef RADIO_SETUP_H
#define RADIO_SETUP_H

#include "radiolib_hal_pico.h"
#include "radiolib_sx1276.h"

// Expose the LoRa object and hardware abstraction layer so main files can use them
extern RadioLibHal_t *hal;
extern RadioLibModule_t radioModule;
extern RadioLibSX127x_t lora;

// Pass the interrupt function from the main file into the setup
bool radio_setup_init(void (*interrupt_callback)(void));

#endif // RADIO_SETUP_H