# Design Spec: Actuator Stall Recovery and Retry

**Date:** 2026-04-11
**Topic:** Actuator Stall Recovery
**Status:** Approved

## 1. Problem Statement
The current actuator stall logic detects a stall and stops the motor, but the main control loop immediately attempts to restart it. This leads to a continuous loop of stall detections and serial flooding (every ~1s), especially when 12V power is missing.

## 2. Goals
- Prevent serial flooding during persistent stall conditions.
- Implement a single automatic retry after the first stall.
- Provide a clear "Hard Lock" state that requires manual intervention (new target) to clear.
- Ensure the main control loop respects the stall/lock state.

## 3. Architecture & State Management

### 3.1 Actuator Struct (`actuator.h`)
Add the following fields:
- `int retry_count`: Number of auto-retries attempted for the current move (0 or 1).
- `bool hard_locked`: Indicates the actuator has failed twice and is now in a safe "Lock" state.
- `uint32_t retry_timer`: (Optional) Timestamp for the auto-retry back-off period.

### 3.2 Constants (`hw_config.h`)
- `ACT_RETRY_BACKOFF_MS`: Time to wait before auto-retrying (default: 2000ms).
- `ACT_MAX_RETRIES`: Number of allowed retries (default: 1).

## 4. Implementation Details

### 4.1 `actuator_tick` (Library Logic)
- **Stall Detection:** When `t_now - act->last_pos_time > ACT_STALL_MS`:
    - If `retry_count < ACT_MAX_RETRIES`:
        - Set `stalled = true`.
        - Increment `retry_count`.
        - Stop motor pins.
        - Start `retry_timer`.
        - Print: `!! [ACTUATOR] STALL DETECTED. Auto-retry in 2s...`
    - Else (if `retry_count >= ACT_MAX_RETRIES`):
        - Set `hard_locked = true`.
        - Stop motor pins.
        - Print: `!! [ACTUATOR] HARD LOCK: Multiple stalls detected. Manual reset required.`
- **Auto-Retry:** If `stalled` and `!hard_locked` and `t_now - retry_timer > ACT_RETRY_BACKOFF_MS`:
    - Set `stalled = false`.
    - `actuator_move_to` will naturally resume movement in the next tick.

### 4.2 `actuator_move_to` (Reset Logic)
- When a **new** target is received (`act->move_target != new_position`):
    - Reset `stalled = false`.
    - Reset `hard_locked = false`.
    - Reset `retry_count = 0`.
    - Reset stall/timeout timers.

### 4.3 Main Loop (`float_main.c`)
- In the inner actuator control loop (50Hz):
    - Check `if (act.hard_locked)`:
        - If true, force `actuator_set_move_pins(&act, 0)` and `actuator_vref_set(0)`.
        - Skip PID calculation and pin override.
    - This ensures the main loop doesn't fight the library's safety lock.

## 5. Success Criteria
- [ ] No serial flooding when 12V is missing (only 2 messages per failed move).
- [ ] Actuator retries once after a stall.
- [ ] Actuator stops completely after the second stall.
- [ ] New serial/radio target resets the stall/lock state and allows movement.
