# Front End Memory (GEMINI.md)

## Role & Overview
The `front_end` directory contains the Python/Streamlit-based Mission Control Dashboard. It connects to the Surface station serial port to command the Float and visualize real-time depth profiles and actuator logs.

## Module Structure
- **[main.py](file:///C:/Users/aman/Documents/Programming/X18-Float-Embedded/front_end/main.py):** Main dashboard entrypoint. Runs a live refresh loop at `REFRESH_RATE_S`.
- **[modules/hardware.py](file:///C:/Users/aman/Documents/Programming/X18-Float-Embedded/front_end/modules/hardware.py):** `HardwareManager` handles the background serial thread listening to the Surface station output, sending commands, and file loading logic.
- **[modules/ui_elements.py](file:///C:/Users/aman/Documents/Programming/X18-Float-Embedded/front_end/modules/ui_elements.py):** Contains rendering functions for sidebar controls, live metrics, plotting, console logs, and OTA reflash interface.
- **[modules/constants.py](file:///C:/Users/aman/Documents/Programming/X18-Float-Embedded/front_end/modules/constants.py):** Configuration limits and default gains.

## Reflash Operations (OTA)
- **Auto-Grab:** Uses `HardwareManager.load_local_firmware()` to read raw binary data from `.pio/build/float/firmware.bin`.
- **Confirmation:** Requires checking the "Confirm firmware flash to Float" checkbox before displaying the flash trigger.
- **Telemetry Sync:** Displays wireless firmware version `FW` extracted from serial sync packets.
- **Serial Thread Lock:** A mutex (`self.serial_lock`) in `modules/hardware.py` protects all serial port accesses. It locks serial reads during transmission of reflash packets to eliminate competition between the telemetry reading thread and the reflash thread, preventing packet fragmentation and CRC errors.
