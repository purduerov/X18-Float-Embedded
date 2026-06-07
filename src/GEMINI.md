# Source Directory Memory (GEMINI.md)

## Firmware Targets
The `src` directory contains core target source entries for compilation:

- **[float_main.c](file:///C:/Users/aman/Documents/Programming/X18-Float-Embedded/src/float_main.c):** Float device firmware. Initializes status LEDs, depth sensor, actuator, and FSM. Runs a 10Hz outer PID depth-control loop and 50Hz inner actuator control loop. Prints compiler date and time (`__DATE__`, `__TIME__`) on boot to local USB CDC.
- **[surface_main.c](file:///C:/Users/aman/Documents/Programming/X18-Float-Embedded/src/surface_main.c):** Surface gateway station. Bridges PC Streamlit dashboard command characters to LoRa command packets. Intercepts `'S'` character to enter host-side OTA reflash stream.
