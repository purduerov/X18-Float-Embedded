# Specification: PID Depth Control with Actuator Integration

## Overview
Implement a full PID control loop to maintain a target depth by controlling the float's buoyancy via a custom actuator. This track will integrate the MS5837 depth sensor for feedback, utilize the existing radio-based PID tuning mechanism, and implement a smart logging strategy to minimize flash wear.

## Functional Requirements
- **Feedback Loop**: Continuously read depth from the MS5837 sensor at a 10Hz frequency.
- **PID Algorithm**: Implement a standard PID controller (`pid.c/h`) using existing constants (Kp, Ki, Kd) received via radio.
- **Actuator Mapping**: Use **Position Control** to map the PID output (calculated error compensation) to a target position for the buoyancy actuator.
- **Safety Limits**:
    - **Soft Limits**: Scale the PID output to stay within the safe operating range of the actuator.
    - **Physical Limits**: Detect and handle physical limits (e.g., stalls or limit switches) as demonstrated in `src/test_actuator.c`.
- **Mission Integration**: Implement the PID loop as a **General Task** that can be invoked across different mission phases (e.g., during the 'Drift' state).
- **Data Persistence**:
    - **PID Tuning**: Save updated PID constants (Kp, Ki, Kd) to LittleFS *only* upon receiving a change via radio.
    - **Mission Logging**: Log mission data (target depth, current depth, PID error, PID output components, actuator position) at a 10Hz rate.
    - **Smart Batching**: To minimize flash wear, mission data should be buffered in RAM and batch-written to LittleFS once every 60 seconds or when the buffer is full.

## Non-Functional Requirements
- **Flash Durability**: Ensure the number of write operations to LittleFS is minimized to prolong the lifespan of the internal flash.
- **Real-Time Performance**: The PID update loop (10Hz) and batch logging must not introduce jitter that affects control stability.

## Acceptance Criteria
- [ ] PID loop successfully maintains depth within a ±0.1m tolerance (under test conditions).
- [ ] Actuator never exceeds predefined soft or hard physical limits.
- [ ] Changing PID constants via radio updates the controller behavior in real-time and persists the new values to flash.
- [ ] Mission data is correctly logged to RAM at 10Hz and batch-written to LittleFS every 60 seconds.

## Out of Scope
- Implementation of the radio-based tuning mechanism (this is assumed to be existing).
- Fine-tuning of PID constants for specific water environments (this will be done in-field).
