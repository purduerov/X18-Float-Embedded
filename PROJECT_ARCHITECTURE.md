# X18-Float Project Architecture

This document provides a high-level overview of the X18-Float system, including the embedded firmware (C/C++) and the Mission Control Dashboard (Python/Streamlit).

## System Components

### 1. Embedded Firmware (C/C++)
- **Microcontroller:** Raspberry Pi Pico (RP2040).
- **Boot Sequence:** Unified hardware initialization via `system_init()` in `lib/config/`.
- **Mission Logic:** Managed by a Finite State Machine (FSM) in `lib/fsm/`.
- **Depth Control:** Autonomous PID control in `lib/depth_pid/` using MS5837 pressure sensor feedback.
- **Sensors:** I2C-based MS5837 (Depth) and BN085 (IMU).
- **Communication:**
  - **Surface/Radio:** SX127x (via RadioLib) for remote telemetry.
  - **Console (`lib/console/`):** Table-driven serial command parser for dashboard interaction.
- **Storage:** Data logged to internal flash using LittleFS (`lib/storage/`). Settings (PID, offsets, bounds) are stored in persistent flash.

### 2. Mission Control Dashboard (Python/Streamlit)
- **Role:** Surface-side interface for real-time monitoring and configuration.
- **Location:** `front_end/main.py`.
- **Connection:** Communicates with the float (directly or via surface station) via a USB/Serial link.
- **Core Functions:**
  - Real-time telemetry visualization (Depth vs Time).
  - **Actuator Control:** Manual override and bounds configuration.
  - Mission parameter tuning (PID gains, Mission Duration, Team ID).
  - Data download from the float's internal storage.

## Communication Protocol (Serial & Radio)

### Outbound (Float/Surface -> Dashboard)
- **Telemetry:** `ID,Time_ms,Depth_m` (e.g., `67,12345,0.45`)
- **Settings Sync:** `[SYNC]` with keys: `P`, `I`, `D`, `Co#`, `Time`, `Off`, `ADC`, `ActMin`, `ActMax`.
  - *Example:* `[SYNC] P=3.20 I=4.50 D=38.40 Co#=67 Time=40 Off=0.000 ADC=2048 ActMin=126 ActMax=3900`

### Inbound (Dashboard -> Surface/Float)
- **Command 'p'**: Begin Dive Profile.
- **Command '?'**: Request current settings (Sync).
- **Command 'z'**: Zero Depth offset.
- **Command 'a <pos>'**: Set manual actuator position (0-4095).
- **Command 'b <min> <max>'**: Set actuator safety bounds.
- **Command 's <p> <i> <d>'**: Update PID gains.

## Development & Maintenance

### Firmware (C/C++)
- **Build System:** PlatformIO with target-specific environments (`env:float`, `env:surface`).
- **Main Files:** 
  - `src/float_main.c`: Primary autonomous firmware.
  - `src/surface_main.c`: Surface station gateway.

### Front End (Python)
- **Workflow:** Use `streamlit run front_end/main.py` to launch the dashboard.
