# X18-Float-Embedded

Autonomous underwater float project designed for oceanographic research and environmental monitoring.

## Project Structure

- **`src/`**: Main firmware entry points (C/C++).
  - `float_main.c`: Primary autonomous float firmware.
  - `surface_main.c`: Modular surface station firmware.
  - `calibrated_float_main.c`: Baseline firmware with direct serial calibration support.
- **`lib/`**: Embedded drivers and modular libraries.
  - `fsm/`: Float-side Mission Finite State Machine.
  - `surface_fsm/`: Surface-side Mission Control State Machine.
  - `surface_link/`: Serial command parsing library.
  - `data_logger/`: Telemetry memory management and CSV export.
  - `pid/`, `MS5837/`, `BN085/`, `RadioLib/`, `storage/`: Hardware drivers and control modules.
- **`front_end/`**: Mission Control Dashboard (Python/Streamlit).
- **`conductor/`**: Project documentation, tech stack, and workflow guides.
- **`.gemini/`**: Gemini CLI configuration and hardware maps.

## Key Features

- **Autonomous Depth Control:** Real-time PID-based buoyancy management.
- **Mission FSM:** Robust mission lifecycle management (Ascent, Descent, Drift).
- **One-Click Calibration:** "Zero Depth" feature to calibrate pressure sensors at the surface via dashboard or serial.
- **Onboard Logging:** Persistent data storage using LittleFS.
- **Wireless Telemetry:** SX127x LoRa/FSK radio communication.
- **Mission Control Dashboard:** Real-time telemetry visualization, remote PID tuning, and data management.

## Quick Start

### Embedded Firmware
1. Install [PlatformIO](https://platformio.org/).
2. Build: `pio run`
3. Flash: `pio run --target upload`

### Mission Control Dashboard
1. Install Python 3.8+.
2. Navigate to `front_end/`.
3. Install dependencies: `pip install streamlit pyserial pandas plotly`
4. Run: `streamlit run main.py`

## Documentation

- **[PROJECT_ARCHITECTURE.md](PROJECT_ARCHITECTURE.md)**: High-level system overview.
- **[front_end/README.md](front_end/README.md)**: Mission Control Dashboard details.
- **[conductor/index.md](conductor/index.md)**: Comprehensive project documentation index.
- **[.gemini/hardware_map.md](.gemini/hardware_map.md)**: Hardware pinout and map.
