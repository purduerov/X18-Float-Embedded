# Libraries Memory (GEMINI.md)

## Shared Libraries Overview
The `lib` directory contains modular embedded drivers and logic for the RP2040 microcontrollers.

## Critical Modules
- **[config](file:///C:/Users/aman/Documents/Programming/X18-Float-Embedded/lib/config/):** System pins (`hw_config.h`) and software constants (`sw_config.h`). Includes version definition `FIRMWARE_VERSION`.
- **[comms](file:///C:/Users/aman/Documents/Programming/X18-Float-Embedded/lib/comms/):** Packet layouts (`packets.h`). Telemetry and settings packages.
- **[fsm](file:///C:/Users/aman/Documents/Programming/X18-Float-Embedded/lib/fsm/):** Float FSM (`float_fsm.c`) managing mission loops and settings sync replies.
- **[surface_fsm](file:///C:/Users/aman/Documents/Programming/X18-Float-Embedded/lib/surface_fsm/):** Surface station FSM (`surface_fsm.c`) decoding packets and syncing to serial.
- **[reflash](file:///C:/Users/aman/Documents/Programming/X18-Float-Embedded/lib/reflash/):** OTA host and target protocol handling high-speed LoRa binary transmission and critical SRAM reboot copy.

## State/Version Packet Format
- **Field:** `uint32_t fw_version` inside settings union.
- **Sync Pattern:** `[SYNC]` logs to serial terminal. Parsed by Streamlit dashboard.
