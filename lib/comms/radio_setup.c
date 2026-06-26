#include "radio_setup.h"
#include "hw_config.h"
#include "pico/stdlib.h"
#include "radiolib_hal_pico.h"
#include "radiolib_sx1276.h"
#include <stdio.h>
#include <string.h>

static RadioLibHal_t *hal;
static RadioLibModule_t radioModule;
static RadioLibSX127x_t lora;

bool radio_setup_init(void (*interrupt_callback)(void)) {
  // Power on the radio module
  gpio_init(PIN_EN);
  gpio_set_dir(PIN_EN, GPIO_OUT);
  gpio_put(PIN_EN, 1);
  sleep_ms(100);

  // Initialize hardware abstraction
  hal = RadioLib_Pico_Create(PIN_SPI_INST, PIN_SPI_SCK, PIN_SPI_MOSI, PIN_SPI_MISO,
                             SPI_BAUDRATE);
  memset(&radioModule, 0, sizeof(RadioLibModule_t));
  RadioLib_Module_Create(&radioModule, hal, PIN_CS, PIN_IRQ, PIN_RST,
                         RADIOLIB_NC);
  radioModule.enPin = PIN_EN;

  uint32_t gPins[] = {RADIOLIB_NC, 29, 6, 7, 10, 11};
  for (int i = 0; i < 6; i++) {
    radioModule.radioGPins[i] = RADIOLIB_NC;
  }

  RadioLib_SX127x_Create(&lora, &radioModule);

  printf("[RADIO] Initializing SX1276...\n");
  // Use centralized config from hw_config.h
  int16_t status = RadioLib_SX1276_Begin(&lora, RADIO_FREQ, RADIO_BW, RADIO_SF,
                                         RADIO_CR, RADIO_POWER);

  if (status != RADIOLIB_ERR_NONE) {
    printf("[RADIO] CRITICAL ERROR: Init failed, code %d\n", status);
    return false;
  }

  // Attach the interrupt routine provided by the main file
  RadioLib_SX127x_SetAction(&lora, interrupt_callback);
  printf("[RADIO] Init Success!\n");

  return true;
}

RadioLibSX127x_t *radio_get_instance(void) { return &lora; }

void radio_start_receive(void) { RadioLib_SX127x_StartReceive(&lora); }

void radio_start_transmit(const uint8_t *data, size_t len) {
  RadioLib_SX127x_StartTransmit(&lora, (uint8_t *)data, len);
}

void radio_finish_transmit(void) { RadioLib_SX127x_FinishTransmit(&lora); }

int16_t radio_read_data(uint8_t *buffer, size_t len) {
  return RadioLib_SX127x_ReadData(&lora, buffer, len);
}

float radio_get_rssi(void) { return RadioLib_SX127x_GetRSSI(&lora); }

float radio_get_snr(void) { return RadioLib_SX127x_GetSNR(&lora); }