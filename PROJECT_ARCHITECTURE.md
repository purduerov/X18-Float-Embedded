# X18-Float Project Architecture

This document provides a high-level overview of the X18-Float system, including the embedded firmware (C/C++) and the Mission Control Dashboard (Python/Streamlit).

## System Components

### 1. Embedded Firmware (C/C++)
- **Microcontroller:** Raspberry Pi Pico (RP2040).
- **Mission Logic:** Managed by a Finite State Machine (FSM) in `lib/fsm/`.
- **Depth Control:** Autonomous PID control in `lib/pid/` using MS5837 pressure sensor feedback.
- **Sensors:** I2C-based MS5837 (Depth) and BN085 (IMU).
- **Calibration:** Support for dynamic depth zeroing (setting surface pressure as 0.0m depth).
- **Communication:**
  - **Surface/Radio:** SX127x (via RadioLib) for remote telemetry.
  - **Surface Link (`lib/surface_link/`):** Modular serial command parser for dashboard interaction.
  - **Data Logger (`lib/data_logger/`):** Memory-managed storage for downloaded telemetry data.
- **Storage:** Data logged to internal flash using LittleFS (`lib/storage/`). Settings (PID, offsets) are stored in persistent flash.

### 2. Mission Control Dashboard (Python/Streamlit)
- **Role:** Surface-side interface for real-time monitoring and configuration.
- **Location:** `front_end/main.py`.
- **Connection:** Communicates with the float (directly or via surface station) via a USB/Serial link.
- **Core Functions:**
  - Real-time telemetry visualization (Depth vs Time).
  - **Depth Calibration:** One-click "Zero Depth" button to calibrate sensors at the surface.
  - Mission parameter tuning (PID gains, Mission Duration, Team ID).
  - Mission state monitoring (Idle, Pre-dive, Active, Complete).
  - Data download from the float's internal storage.

## Communication Protocol (Serial & Radio)

The float and dashboard communicate over Serial using a set of commands and data formats:

### Outbound (Float/Surface -> Dashboard)
- **Telemetry:** `ID,Time_ms,Depth_m` (e.g., `67,12345,0.45`)
- **Status Events:**
  - `PREDIVE_READY`: Float is ready to start a mission.
  - `START DATA DUMP` / `END DATA DUMP`: Framing for historical data retrieval.
  - `DATA_DONE`: Mission successfully completed.
- **Settings Sync:** `[SYNC]` followed by key-value pairs (e.g., `P=3.2 I=4.5 D=38.4 Co#=67 Time=40`).

### Inbound (Dashboard -> Surface/Float)
- **Command 'p'**: Trigger the mission profile (Begin Dive).
- **Command '?'**: Request current settings from the float.
- **Command 'z'**: **Zero Depth** - Record current pressure as surface baseline.
- **Command 'c <id>'**: Update Team/Company ID.
- **Command 't <seconds>'**: Update Mission Duration.
- **Command 's <p> <i> <d>'**: Update PID gains.

## Development & Maintenance

### Firmware (C/C++)
- **Build System:** PlatformIO.
- **Main Files:** 
  - `src/float_main.c`: Primary autonomous firmware with PID.
  - `src/surface_main.c`: Surface station firmware (refactored for modularity).
  - `src/calibrated_float_main.c`: Firmware based on the working baseline with added serial calibration support.

### Front End (Python)
- **Framework:** Streamlit.
- **Workflow:** Use `streamlit run front_end/main.py` to launch the dashboard.
- **Requirements:** See `front_end/README.md` for dependencies.
