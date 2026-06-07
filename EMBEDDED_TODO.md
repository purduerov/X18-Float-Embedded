# Embedded Systems Architecture Improvements

This document lists future technical improvements for the X18-Float embedded firmware to increase safety, determinism, and reliability during aquatic operations.

---

## 1. Timing Jitter & CPU Scheduling
*   **Problem:** The main loop in `float_main.c` uses software timing comparisons (`if (now - last_time >= interval)`) coupled with a blocking `sleep_ms(1)`. Loop execution jitter from serial printing, I2C reads, or LoRa transmissions shifts the execution of the 10Hz Depth PID and 50Hz Actuator control loops.
*   **Improvement:** 
    *   Transition the control loops to **Hardware Alarm repeating timers** using the Pico SDK (`add_repeating_timer_ms`).
    *   Run the PID control and Actuator loops in interrupt-driven contexts or dedicate Core 1 to time-critical loops.

## 2. Flash Storage Safety (Settings Corruption)
*   **Problem:** `storage_save()` erases and overwrites the exact same 4KB sector at `FLASH_TARGET_OFFSET` every time settings (PID, bounds, neutral baseline) are changed. 
    *   **Wear:** No wear leveling.
    *   **Power Loss:** If power is cut during the sector erase phase, all settings are lost, and the float reverts to uncalibrated defaults.
    *   **Data Integrity:** There is no checksum (e.g., CRC) validation of stored settings; magic number validation alone cannot detect partial write corruptions.
*   **Improvement:**
    *   **CRC Verification:** Add a CRC-16 or CRC-32 checksum to `float_settings_t` and validate on boot.
    *   **Double Buffering (Ping-Pong):** Use two distinct sectors. Store settings with an incrementing sequence number. Always write to the oldest sector. On boot, load the sector with the highest version that passes the CRC check.

## 3. SCL Clock-Toggle I2C Bus Recovery
*   **Problem:** The current recovery logic performs a software controller reset (`hw_deinit_i2c` / `hw_init_i2c`). However, if an I2C slave device hangs mid-read and pulls the SDA line low, resetting the microcontroller's I2C peripheral will not free the bus because SCL remains high.
*   **Improvement:**
    *   Implement **Clock Clearing / SCL Toggling**: Before initializing the I2C controller, configure SCL as a GPIO output. Toggle SCL up to 9 times. This clocks out any stuck bits in the slave state machine, forcing it to release SDA. Once SDA goes high, configure the pins back to I2C mode.

## 4. Flash Streaming Telemetry
*   **Problem:** Telemetry samples are buffered in massive static arrays (`recorded_depths[1000]`, etc.) in RAM. If the float loses power, runs out of battery, or crashes, all mission telemetry is lost.
*   **Improvement:**
    *   Use a circular logging partition in Flash memory (e.g., using LittleFS).
    *   Write telemetry records directly to Flash in real-time or in small page buffers. This allows logging infinite profiles and guarantees data preservation on power loss.

## 5. Wireless Packet Robustness
*   **Problem:** `packet_t` uses a single-byte XOR checksum. XOR checksums are weak and cannot detect common multi-bit transmission errors (e.g., matching flipped bits on adjacent fields).
*   **Improvement:**
    *   Replace the XOR checksum with a **CRC-8-CCITT** or **CRC-16** calculated over the preceding 51 bytes of the packet.
