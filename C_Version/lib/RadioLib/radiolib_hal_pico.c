#include "pico/stdlib.h"
#include "hardware/irq.h"
#include "pico/multicore.h"
#include <stdlib.h>
#include "radiolib_hal_pico.h"
#include "hardware/spi.h"

// Max GPIOs for interrupt tracking
#define PI_PICO_MAX_USER_GPIO (48)
#define RADIOLIB_NC (0xFFFFFFFF)

// Static storage for interrupt handling
static void (*picoHalUserCallbacks[PI_PICO_MAX_USER_GPIO])(void) = {0};
static uint32_t picoHalIrqEventMasks[PI_PICO_MAX_USER_GPIO] = {0};

// Internal interrupt dispatcher
static void picoInterruptHandler(uint gpio, uint32_t events) {
    if (gpio < PI_PICO_MAX_USER_GPIO && picoHalUserCallbacks[gpio]) {
        picoHalUserCallbacks[gpio]();
    }
}

// Private context for the RP2040 HAL
typedef struct {
    spi_inst_t *spi; 
    uint32_t sck;
    uint32_t mosi;
    uint32_t miso;
    uint32_t speed;
} PicoHalCtx_t;

// --- GPIO Implementations ---
void pico_pinMode(RadioLibHal_t *self, uint32_t pin, uint32_t mode)
{
    if (pin == RADIOLIB_NC) return;
    gpio_init(pin);
    gpio_set_dir(pin, mode);
}

void pico_digitalWrite(RadioLibHal_t *self, uint32_t pin, uint32_t value)
{
    if (pin == RADIOLIB_NC) return;
    gpio_put(pin, (bool)value);
}

uint32_t pico_digitalRead(RadioLibHal_t *self, uint32_t pin)
{
    if (pin == RADIOLIB_NC) return 0;
    return gpio_get(pin);
}

// --- Timing Implementations ---
void pico_delay(RadioLibHal_t *self, RadioLibTime_t ms) { sleep_ms(ms); }
void pico_delayMicroseconds(RadioLibHal_t *self, RadioLibTime_t us) { sleep_us(us); }
RadioLibTime_t pico_millis(RadioLibHal_t *self) { return to_ms_since_boot(get_absolute_time()); }
RadioLibTime_t pico_micros(RadioLibHal_t *self) { return to_us_since_boot(get_absolute_time()); }

// --- SPI Implementations ---
void pico_spiBegin(RadioLibHal_t* self) {
    PicoHalCtx_t* ctx = (PicoHalCtx_t*)self->user_data;
    spi_init(ctx->spi, ctx->speed);
    gpio_set_function(ctx->sck, GPIO_FUNC_SPI);
    gpio_set_function(ctx->mosi, GPIO_FUNC_SPI);
    gpio_set_function(ctx->miso, GPIO_FUNC_SPI);
}

void pico_spiTransfer(RadioLibHal_t* self, uint8_t* out, size_t len, uint8_t* in) {
    PicoHalCtx_t* ctx = (PicoHalCtx_t*)self->user_data;
    spi_write_read_blocking(ctx->spi, out, in, len);
}

void pico_spiNoOp(RadioLibHal_t* self) { (void)self; }

// --- Lifecycle ---
void pico_attachInterrupt(RadioLibHal_t *self, uint32_t pin, void (*cb)(void), uint32_t mode) {
    if (pin == RADIOLIB_NC || pin >= PI_PICO_MAX_USER_GPIO) return;
    
    picoHalUserCallbacks[pin] = cb;
    picoHalIrqEventMasks[pin] = mode;
    
    // Use the shared callback mechanism correctly
    gpio_set_irq_enabled_with_callback(pin, mode, true, &picoInterruptHandler);
}

void pico_detachInterrupt(RadioLibHal_t *self, uint32_t pin) {
    gpio_set_irq_enabled(pin, picoHalIrqEventMasks[pin], false);
}

void pico_yield(RadioLibHal_t *self) { (void)self; }

void pico_init(RadioLibHal_t *self) { self->spiBegin(self); }

RadioLibHal_t* RadioLib_Pico_Create(spi_inst_t* spi, uint32_t sck, uint32_t mosi, uint32_t miso, uint32_t speed) {
    RadioLibHal_t* hal = malloc(sizeof(RadioLibHal_t));
    PicoHalCtx_t* ctx = malloc(sizeof(PicoHalCtx_t));
    
    ctx->spi = spi; ctx->sck = sck; ctx->mosi = mosi; ctx->miso = miso; ctx->speed = speed;

    hal->gpioModeInput = GPIO_IN;
    hal->gpioModeOutput = GPIO_OUT;
    hal->gpioLevelLow = 0;
    hal->gpioLevelHigh = 1;
    hal->gpioInterruptRising = GPIO_IRQ_EDGE_RISE;
    hal->gpioInterruptFalling = GPIO_IRQ_EDGE_FALL;
    hal->init = pico_init;
    hal->pinMode = pico_pinMode;
    hal->digitalWrite = pico_digitalWrite;
    hal->digitalRead = pico_digitalRead;
    hal->attachInterrupt = pico_attachInterrupt;
    hal->detachInterrupt = pico_detachInterrupt;
    hal->delay = pico_delay;
    hal->delayMicroseconds = pico_delayMicroseconds;
    hal->millis = pico_millis;
    hal->micros = pico_micros;
    hal->yield = pico_yield;
    hal->spiBegin = pico_spiBegin;
    hal->spiBeginTransaction = pico_spiNoOp;
    hal->spiTransfer = pico_spiTransfer;
    hal->spiEndTransaction = pico_spiNoOp;
    hal->spiEnd = pico_spiNoOp;
    hal->user_data = ctx;

    return hal;
}