# MATE 2026 Mission Sequencer Plan

## Goal
Implement a multi-stage mission sequencer that handles multiple identical vertical profiles (Deep -> Shallow) as required by the 2026 competition, with fully adjustable parameters via the dashboard. It must also support running just 1 profile and skipping the shallow depth if desired (e.g., if shallow depth is set to 0.0 or the same as deep depth).

## 1. Configurable Settings (`float_settings_t` & `packets.h`)
- Add/update settings to allow full customization from the dashboard:
  - `deep_target_m` (e.g., 2.5m)
  - `shallow_target_m` (e.g., 0.4m. If set to 0.0, skip the shallow stage.)
  - `num_profiles` (e.g., 1 or 2)
  - `profile_duration_s` (hold time per depth, e.g., 30s)
  - `arrival_band_m` (adjustable tolerance, already exists)
- Add new `CMD_SET_*` packets to sync these from the Surface Station to the Float.

## 2. FSM Refactor (`float_fsm.h`)
- Add `mission_stage` and `current_profile` to `float_fsm_t`.
- Define dynamic stages:
  - `STAGE_DEEP`
  - `STAGE_SHALLOW`
- Update `FloatState_t` to use a unified `FLOAT_PROFILING` state that relies on the internal `mission_stage` to dictate the target.

## 3. Dynamic Targets (`float_fsm.c`)
- Update `float_fsm_update` to:
  - Select target depth (`deep_target_m` or `shallow_target_m`) based on current `mission_stage`.
  - Handle transition from Deep to Shallow only after `profile_duration_s` consecutive hold.
  - If `shallow_target_m` is 0.0 (or disabled), skip the `STAGE_SHALLOW` phase and immediately increment `current_profile`.
  - After Shallow hold completes (if enabled), increment `current_profile`.
  - If `current_profile >= num_profiles`, transition to `FLOAT_PROFILE_DONE`.
  - Reset `target_depth_reached` on stage transition.

## 4. Consecutive Hold Logic
- Ensure `profile_start_time` is reset if the float drifts outside `arrival_band_m` during a stage.
- Add a small debounce to the "drift" to prevent jitter from resetting the timer unnecessarily.

## 5. UI/Dashboard Support
- Update dashboard to include inputs for:
  - Deep Target (m)
  - Shallow Target (m) (note: set to 0 to disable)
  - Number of Profiles
- Keep inputs for Time (Hold Duration) and Arrival Tolerance.
- Update the "Active Config" and Telemetry display to show current "Mission Stage" (e.g., "Profile 1: Deep").

## 6. Safety Fail-safes
- Implement a global mission timer. If the entire sequence takes more than 10-15 minutes, force surface (`FLOAT_PROFILE_DONE`).
- Surface immediately if battery voltage is low (if monitored) or if too many sensor failures occur.
