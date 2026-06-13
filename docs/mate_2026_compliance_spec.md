# MATE 2026 Explorer Class - Compliance Specification

This document maps the **MATE 2026 EXPLORER Class** competition rules for Task 4 ("MATE Floats Under the Ice") directly to the **X18-Float** firmware architecture and configurations.

---

## 1. FSM Profile Sequence & Ice Avoidance

* **MATE Requirement:** Complete exactly **two identical vertical profiles** back-to-back:
  `[SURFACE] ➔ [DESCEND] ➔ [DEEP HOLD] ➔ [ASCEND] ➔ [SHALLOW HOLD] ➔ [RECOVERY]`
* **Ice Avoidance Penalty:** Breaking the water surface (depth $\le 0.0$m) or contacting the physical ice sheet during ascent/hold triggers a **-5 point penalty** per profile.
* **Firmware Implementation:**
  * Controlled by the sequencer in `lib/fsm/float_fsm.c` and `src/float_main.c`.
  * **Surface/Ice Avoidance:** The controller uses a **biased target of 0.55m** (instead of nominal 0.40m) and asymmetric deadband bounds (**0.65m** deep limit to nudge up / **0.42m** shallow limit to nudge down) during the shallow stage to guarantee the float remains safely submerged below the ice/surface.

---

## 2. Target Depths & Window Comparators

* **MATE Target Limits:**
  * **Deep Hold:** Target **2.50m** within a $\pm$0.33m band (**2.17m to 2.83m**).
  * **Shallow Hold:** Target **0.40m** within a $\pm$0.33m band (**0.07m to 0.73m**).
* **Firmware Constants (`lib/config/sw_config.h`):**
  * `ARRIVAL_BAND_M` is set to `0.33f`.
  * `deep_target_m` is set to `2.5f`.
  * `shallow_target_m` is set to `0.4f`.

---

## 3. The 30-Second Hold & Sampling Clock

* **MATE Requirement:** 
  * Float must maintain depth within the target window for **30 consecutive seconds**.
  * **Drift Reset:** If the float drifts outside the target window ($\pm$0.33m) at *any* point, the timer must completely reset to zero.
  * **Logging Interval:** Telemetry must be logged at **5-second intervals** (exactly 7 points: `0s, 5s, 10s, 15s, 20s, 25s, 30s`).
* **Firmware Implementation:**
  * Implemented in `float_fsm_update()`.
  * **Drift Reset:** On error bounds breach, `fsm->target_depth_reached` is set to `false`, `profile_start_time` is updated, and the stage's log index is reset.
  * **Hold Packets:** Checks `elapsed_ms >= expected_elapsed_ms` (multiples of 5000ms) to log exactly 7 sequential packets per hold.

---

## 4. Telemetry Log Accumulation

* **MATE Requirement:** Post-dive telemetry graph must contain **at least 20 valid packets** from the vertical profiles.
* **Firmware Implementation:**
  * Resolved the overwriting bug in `float_fsm.c`.
  * **Accumulation:** `sample_index` is offset using `(current_profile - 1) * 14` (Deep starts at index `0` or `14`, Shallow starts at index `7` or `21`).
  * **Total Logged Packets:** Logs exactly **28 packets** across two full profiles, meeting the 20-packet floor without overwriting.

---

## 5. Packet Formatting Definitions & Gaps

* **MATE Requirement:** Payload must transmit:
  1. Company ID
  2. Timestamp Data
  3. Pressure Data: Displayed in Pascals (Pa) or kilopascals (kPa).
  4. Depth Data: Displayed in meters (m) or centimeters (cm).
* **Firmware Packet Struct (`lib/comms/packets.h`):**
  ```c
  struct __attribute__((packed)) {
      uint16_t company_number;
      uint32_t time_ms;
      float depth_m;
      uint16_t actuator_pos;
      uint16_t target_actuator_pos;
  } telemetry;
  ```
* > [!WARNING]
  > **COMPLIANCE GAP IDENTIFIED:**
  > The current `telemetry` structure contains depth (`depth_m`) but **lacks a pressure field** (Pa or kPa). The struct must be refactored to include a pressure calculation field (calculated from `ms5837_get_pressure()`) to be fully compliant.

---

## 6. Wireless Transmission Constraints

* **Pre-Dive (Shore):** Float wirelessly transmits at least one data packet before dive (handled by `FLOAT_PRE_DIVE` state).
* **Submerged:** No tether or wire; float logs blindly to local memory arrays.
* **Post-Recovery (Deck):** Float autonomously transmits all 28 logged packets to the shore station upon receiving a recovery command (`CMD_SEND_DATA` sent automatically by the surface station on profile done).

---

## 7. Safety Timeout Buffer

* **MATE Requirement:** Sufficient safety buffer to prevent premature aborts on slow transits.
* **Firmware Constant (`lib/config/sw_config.h`):**
  * `PROFILING_SAFETY_TIMEOUT_S` is set to **60 seconds** to allow slow buoyancy engine transit times.
