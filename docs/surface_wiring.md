# Surface Station - Physical Wiring Map

This document defines the physical wiring connections between the Raspberry Pi Pico and the SX1276 LoRa Radio for the Surface Station.

## Pin Connections

| Pico Pin (GPIO) | Pico Physical Pin | Radio Pin | Function / Description |
| :---: | :---: | :---: | :--- |
| **GP16** | 21 | **MISO** | SPI0 RX / Master In Slave Out |
| **GP17** | 22 | **CS / NSS** | SPI0 CSn / Chip Select |
| **GP18** | 24 | **SCK** | SPI0 SCK / Clock |
| **GP19** | 25 | **MOSI** | SPI0 TX / Master Out Slave In |
| **GP20** | 26 | **RST** | Reset (Active LOW) |
| **GP2** | 4 | **DIO0 / IRQ** | Hardware Interrupt (Active HIGH) |
| **GP8** | 11 | **EN** | Power Regulator Enable (High = ON) |
| **3V3(OUT)** | 36 | **VCC** | 3.3V Power |
| **GND** | 3, 8, 13, 18, 23, 28, 33, or 38 | **GND** | Ground Reference |
