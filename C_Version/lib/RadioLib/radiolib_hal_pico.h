#ifndef RADIOLIB_HAL_H
#define RADIOLIB_HAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "hardware/spi.h" // Required for spi_inst_t

// Define the time type used for timestamps and delays
typedef uint32_t RadioLibTime_t;

// Forward declaration of the HAL structure
struct RadioLibHal;

/**
 * @brief Global function to get microseconds since start.
 */
RadioLibTime_t rlb_time_us(void);

/**
 * @brief Hardware abstraction library interface structure.
 */
typedef struct RadioLibHal {
    // Platform Constants
    uint32_t gpioModeInput;
    uint32_t gpioModeOutput;
    uint32_t gpioLevelLow;
    uint32_t gpioLevelHigh;
    uint32_t gpioInterruptRising;
    uint32_t gpioInterruptFalling;

    void (*init)(struct RadioLibHal* self);

    // GPIO
    void (*pinMode)(struct RadioLibHal* self, uint32_t pin, uint32_t mode);
    void (*digitalWrite)(struct RadioLibHal* self, uint32_t pin, uint32_t value);
    uint32_t (*digitalRead)(struct RadioLibHal* self, uint32_t pin);
    void (*attachInterrupt)(struct RadioLibHal* self, uint32_t pin, void (*cb)(void), uint32_t mode);
    void (*detachInterrupt)(struct RadioLibHal* self, uint32_t pin);

    // Timing
    void (*delay)(struct RadioLibHal* self, RadioLibTime_t ms);
    void (*delayMicroseconds)(struct RadioLibHal* self, RadioLibTime_t us);
    RadioLibTime_t (*millis)(struct RadioLibHal* self);
    RadioLibTime_t (*micros)(struct RadioLibHal* self);
    void (*yield)(struct RadioLibHal* self);

    // SPI
    void (*spiBegin)(struct RadioLibHal* self);
    void (*spiBeginTransaction)(struct RadioLibHal* self);
    void (*spiTransfer)(struct RadioLibHal* self, uint8_t* out, size_t len, uint8_t* in);
    void (*spiEndTransaction)(struct RadioLibHal* self);
    void (*spiEnd)(struct RadioLibHal* self);

    void* user_data;
} RadioLibHal_t;

// Constructor Function Prototype
RadioLibHal_t* RadioLib_Pico_Create(spi_inst_t* spi, uint32_t sck, uint32_t mosi, uint32_t miso, uint32_t speed);

#endif