# Summary of Abandoned Changes (2026-04-28)

We were attempting to implement a **Buoyancy PID & Hold** feature. This summary is for re-implementation reference.

## 1. Neutral Buoyancy ADC Baseline
- **Concept:** Anchor the PID loop on a specific "Neutral" ADC value rather than nudging the current position.
- **Math:** `Target_Position = Neutral_ADC + PID_Output`.
- **Direction:** `current_depth - target_depth`. Negative output = Retract (Sink). Positive output = Expand (Rise).
- **Storage:** Added `neutral_buoyancy_adc` to `float_settings_t` and `packets.h`.

## 2. Depth Arrival Timer
- **Concept:** Don't start the `profile_duration_s` countdown until the float is actually at depth.
- **Logic:** Arrival band was set to `+/- 0.33m`.
- **FSM:** Added `target_depth_reached` flag to `float_fsm_t`.

## 3. Sensor Recovery Mechanism
- **Concept:** Detect I2C failures and reset the sensor/bus.
- **Tier 1:** 5 failures -> `ms5837_begin` (Soft reset).
- **Tier 2:** 10 failures -> `i2c_deinit` + `i2c_init` (Hard bus reset).
- **Fail-safe:** 50 failures -> `FLOAT_PROFILE_DONE` (Force surface).

## 4. Known Issues Encountered
- **Loop Starvation:** Calling `ms5837_read` inside the high-frequency FSM loop caused a total hang because it's a blocking 40ms call.
- **Unresponsiveness:** High I2C timeouts (50ms) and aggressive recovery attempts were starving the `console_update()` task.

## Re-Implementation Strategy
- Ensure `ms5837_read` is ONLY called in the timed 10Hz loop.
- Use a cached `current_depth` value in the FSM struct for logic checks.
- Keep I2C timeouts low (5ms) to prevent blocking during sensor loss.
- Decouple the "Neutral ADC" update from the "PID Gain" update in the UI to prevent packet overflow.