# Libraries Memory (GEMINI.md)

## Shared Libraries Overview
The `lib` directory contains modular embedded drivers and logic for the RP2040 microcontrollers.

## Critical Modules
- **[config](file:///C:/Users/aman/Documents/Programming/X18-Float-Embedded/lib/config/):** System pins (`hw_config.h`) and software constants (`sw_config.h`). Maps dashboard parameters (`kp` -> Braking Time Constant, `ki` -> Braking Effort, `kd` -> Hover Deadband) and defines hybrid control limits and alpha constants.
- **[comms](file:///C:/Users/aman/Documents/Programming/X18-Float-Embedded/lib/comms/):** Packet layouts (`packets.h`). Telemetry and settings packages.
- **[fsm](file:///C:/Users/aman/Documents/Programming/X18-Float-Embedded/lib/fsm/):** Float FSM (`float_fsm.c`) managing mission loops, target arrivals, and data transmission. Enforces consecutive target hold requirements: resets `sample_index` and logs on drift. Explicitly initializes `sample_index` on arrivals (`0` for deep, `7` for shallow) and resets `ctrl_state` and `active_neutral_adc` on stage transitions to ensure a clean handoff.
- **[surface_fsm](file:///C:/Users/aman/Documents/Programming/X18-Float-Embedded/lib/surface_fsm/):** Surface station FSM (`surface_fsm.c`) decoding packets and syncing to serial.
- **[reflash](file:///C:/Users/aman/Documents/Programming/X18-Float-Embedded/lib/reflash/):** OTA host and target protocol handling high-speed LoRa binary transmission and critical SRAM reboot copy.

## State/Version Packet Format
- **Field:** `uint32_t fw_version` inside settings union.
- **Sync Pattern:** `[SYNC]` logs to serial terminal. Parsed by Streamlit dashboard.
- **Hold Packets:** Telemetry arrays log 7 sequential packets at 5-second intervals (0s, 5s, 10s, 15s, 20s, 25s, 30s) per target hold stage, for a total of 14 log records.
