# Hardware and Mission Alignment Specification

This document records the finalized hardware parameters, calibration bounds, and mission-sequencing logic for the **X18-Float** system.

---

## 1. Electrical & Power Configuration

* **Battery Chemistry:** Nickel-Metal Hydride (NiMH)
* **Battery Configuration:** 10x 1.2V AA cells in series (Nominal **12.0V** pack)
* **Motor Driver Supply:** 12.0V directly from the battery pack to the H-bridge inputs.
* **Logic Levels & Sensors:** 3.3V logic (RP2040)
* **Feedback Potentiometer:** Built-in actuator potentiometer. Must be powered from the **3.3V logic rail** (not the 12V battery pack) to prevent damage to the RP2040 ADC0 (GP26).

---

## 2. Actuator Calibration & Control Limits

* **Feedback Sensor:** 12-bit ADC (0 to 4095 range)
* **Retracted Safety Limit:** **120** ADC counts
* **Extended Safety Limit:** **3900** ADC counts
  > [!IMPORTANT]
  > Operating outside the 120–3900 range risks mechanical binding, motor stalling, and battery drain.
* **Actuator Position Deadzone:** Controlled via two-limit hysteresis in `lib/actuator/actuator.c`:
  * **Enter Deadzone (Stop):** Within **±20** counts of target position.
  * **Exit Deadzone (Move):** Exceeds **±50** counts of target position.
  * *Note: The legacy single-value `POS_TOL` parameter is obsolete.*

---

## 3. Communication & Peripheral Settings

* **I2C Bus Speed (i2c1):** **10 kHz** (Configured for extreme noise immunity underwater; overrides default 400 kHz specifications).
* **LoRa Radio Output Power:** **17 dBm** (Configured for max stable power using the SX1276 PA_BOOST pin to ensure underwater signal penetration).

---

## 4. Control Targets & Surface Avoidance

* **Deep Target Depth:** **2.5 meters** (MATE specification: 2.5m ± 0.33m).
* **Shallow Target Depth:** **0.40 meters** (MATE specification: 0.40m ± 0.33m).
* **Biased Control Target (Shallow):** **0.55 meters**
  * Configured as the effective control target to keep a safety buffer below the ice/surface.
* **Hover Deadbands (Shallow Stage):** Asymmetric limits to guarantee ice/surface avoidance:
  * **Drift Deep Limit:** **0.65m** (Nudges actuator up to rise).
  * **Drift Shallow Limit:** **0.42m** (Nudges actuator down to sink).

---

## 5. MATE 2026 FSM Profile Logging

To comply with the MATE 2026 EXPLORER requirements (at least 20 packets logged at 5-second intervals over two vertical profiles), the FSM logic in `lib/fsm/float_fsm.c` is updated to accumulate data across both runs:

| Profile | Stage | Sample Index Range | Timestamp Range | Target Depth |
| :---: | :---: | :---: | :---: | :---: |
| **Profile 1** | Deep | `0` to `6` | 0s to 30s | 2.5m |
| **Profile 1** | Shallow | `7` to `13` | 0s to 30s | 0.55m (biased) |
| **Profile 2** | Deep | `14` to `20` | 0s to 30s | 2.5m |
| **Profile 2** | Shallow | `21` to `27` | 0s to 30s | 0.55m (biased) |

* **Total Packets Logged:** **28 packets** (Clears the 20-packet minimum requirement; prevents Profile 2 from overwriting Profile 1's data in the RAM buffer).
