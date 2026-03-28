# Implementation Plan: PID Depth Control with Actuator Integration

## Phase 1: Depth PID Controller Implementation
This phase implements the high-level mapping from depth error to target actuator position using TDD.

- [ ] Task: Define Depth PID structure and initialization
    - [ ] Create `lib/pid/depth_pid.h` defining `DepthPID` wrapper.
    - [ ] Implement `depth_pid_init` and `depth_pid_set_target`.
- [ ] Task: Implement Depth-to-Actuator mapping and unit tests (TDD)
    - [ ] Write unit tests for `depth_pid_calculate_target_pos` (mapping depth error to a 0-4095 potentiometer range).
    - [ ] Implement the calculation logic in `lib/pid/depth_pid.c`.
- [ ] Task: Conductor - User Manual Verification 'Phase 1: Depth PID Controller Implementation' (Protocol in workflow.md)

## Phase 2: Actuator Library Enhancement
This phase migrates the successful control logic from `test_actuator.c` into the core `lib/actuator` library.

- [ ] Task: Integrate VREF PWM and ADC Filtering into `lib/actuator`
    - [ ] Update `Actuator` struct to include VREF PWM slice/channel.
    - [ ] Implement `actuator_set_torque` (using PWM) and `get_filtered_pos` in `lib/actuator/actuator.c`.
- [ ] Task: Implement robust `actuator_move_to_position` (TDD)
    - [ ] Write tests for the hysteresis logic (DEADZONE_ENTER/EXIT).
    - [ ] Implement the move-to-position logic (Inner PID or thresholding) as seen in `test_actuator.c`.
- [ ] Task: Conductor - User Manual Verification 'Phase 2: Actuator Library Enhancement' (Protocol in workflow.md)

## Phase 3: Smart Data Persistence and Batch Logging
This phase implements the storage strategies to protect the flash while maintaining high-resolution telemetry.

- [ ] Task: Implement PID Constant Persistence (TDD)
    - [ ] Write tests for `storage_save_pid_params` and `storage_load_pid_params`.
    - [ ] Implement persistence to LittleFS (only writing on change).
- [ ] Task: Implement 10Hz RAM Batch Logging (TDD)
    - [ ] Create a `lib/storage/telemetry_buffer.c` for RAM-based logging.
    - [ ] Implement the 60s/buffer-full flush mechanism to LittleFS.
- [ ] Task: Conductor - User Manual Verification 'Phase 3: Smart Data Persistence and Batch Logging' (Protocol in workflow.md)

## Phase 4: Full System Integration
This phase ties the Depth PID, Actuator control, and Storage together into a unified task.

- [ ] Task: Implement `depth_control_task`
    - [ ] Combine depth reading, PID calculation, actuator movement, and telemetry buffering into a single periodic function (10Hz).
- [ ] Task: Integration Test with Mock Sensors (TDD)
    - [ ] Write a test runner that simulates depth changes and verifies actuator response and logging behavior.
- [ ] Task: Conductor - User Manual Verification 'Phase 4: Full System Integration' (Protocol in workflow.md)
