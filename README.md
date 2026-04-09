# X18-Float-Embedded

Autonomous underwater float project designed for oceanographic research and environmental monitoring.

## 📂 Project Structure

- **`src/`**: Main firmware entry points (C/C++).
- **`lib/`**: Embedded drivers and modular libraries.
- **`front_end/`**: Mission Control Dashboard (Python/Streamlit).
- **`.gemini/`**: Gemini CLI configuration and hardware maps.

## 🏗️ Software Architecture

Detailed documentation for the system components:
- **[Float Unit (Underwater)](docs/float_architecture.md)**: Main loop flow, state machine, and safety mechanisms.
- **[Surface Station](docs/surface_architecture.md)**: Dashboard link, recovery FSM, and data logging.
- **[Communication Protocol](docs/communication_protocol.md)**: Detailed packet structures, command codes, and reliability ARQ.
- **[Hardware & Pinouts](docs/hardware_specifications.md)**: GPIO mapping, I2C/SPI peripherals, and actuator control logic.
- **[Hardware Map (Internal)](.gemini/hardware_map.md)**: **CRITICAL** Wiring reference (restricted).

## ✨ Key Features

- **Autonomous Depth Control:** Real-time PID-based buoyancy management.
- **Mission FSM:** Robust mission lifecycle management (Ascent, Descent, Drift).
- **One-Click Calibration:** "Zero Depth" feature to calibrate pressure sensors via dashboard.
- **Wireless Telemetry:** SX127x LoRa/FSK radio communication.
- **Mission Control Dashboard:** Real-time visualization and remote tuning.

## 🚀 Quick Start

### Embedded Firmware
1. Install [PlatformIO](https://platformio.org/).
2. **User Handled:** Build (`pio run`) and Flash (`pio run --target upload`). 

### Mission Control Dashboard
1. Navigate to `front_end/`.
2. Install dependencies: `pip install streamlit pyserial pandas plotly`
3. Run: `streamlit run main.py`
