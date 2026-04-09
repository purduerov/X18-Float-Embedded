# Float Unit (Underwater) Architecture

This document describes the software architecture for the underwater float, including its state machine and main loop execution flow.

---

## 1. Float FSM (State Transitions)

The Float FSM manages the mission lifecycle, from waiting for surface commands to executing the dive profile and surfacing to transfer data.

```mermaid
stateDiagram-v2
    [*] --> FLOAT_IDLE
    
    FLOAT_IDLE --> FLOAT_PRE_DIVE : Received CMD_BEGIN_PROFILE
    FLOAT_IDLE --> FLOAT_TEST_CALIBRATE : Received CMD_ENTER_TEST
    
    FLOAT_PRE_DIVE --> FLOAT_PROFILING : Pre-dive TX Finished
    
    state FLOAT_PROFILING {
        [*] --> Sampling
        Sampling --> Sampling : 1s Interval
    }
    
    FLOAT_PROFILING --> FLOAT_PROFILE_DONE : Duration Reached
    
    FLOAT_PROFILE_DONE --> FLOAT_DUMPING_DATA : Received CMD_SEND_DATA
    FLOAT_PROFILE_DONE --> FLOAT_PROFILE_DONE : Broadcast DONE_PROFILE (3s)
    
    FLOAT_DUMPING_DATA --> FLOAT_IDLE : All Data Sent (CMD_DATA_DONE)
    FLOAT_DUMPING_DATA --> FLOAT_DUMPING_DATA : Wait for ACKs / Retransmit
    
    FLOAT_TEST_CALIBRATE --> FLOAT_TEST_CALIBRATE : Broadcast Live Telemetry (1s)
    
    %% Global Reset
    FLOAT_PRE_DIVE --> FLOAT_IDLE : Received CMD_RESET_FSM
    FLOAT_PROFILING --> FLOAT_IDLE : Received CMD_RESET_FSM
    FLOAT_PROFILE_DONE --> FLOAT_IDLE : Received CMD_RESET_FSM
    FLOAT_DUMPING_DATA --> FLOAT_IDLE : Received CMD_RESET_FSM
    FLOAT_TEST_CALIBRATE --> FLOAT_IDLE : Received CMD_RESET_FSM
```

---

## 2. Float Program Flow (Main Loop)

The main loop in `src/float_main.c` executes periodic tasks like PID control and safety checks.

```mermaid
flowchart TD
    Start([Start]) --> Init[Initialize Hardware & Flash Storage]
    Init --> ReadSettings[Read PID & Mission Settings from Flash]
    ReadSettings --> Loop[Main Loop]
    
    subgraph LoopSection [Main Execution Loop]
        Loop --> TimerPID{PID Timer: 100ms?}
        TimerPID -- Yes --> ReadSensor[Read Depth Sensor]
        ReadSensor --> CalcPID[Calculate Target Actuator Position]
        CalcPID --> MoveAct[Command Actuator Movement]
        MoveAct --> SafetyCheck[Safety: Stall & Timeout Detection]
        SafetyCheck --> FSMUpdate[Update FSM & Log Data]
        
        TimerPID -- No --> FSMUpdate
        
        FSMUpdate --> RadioIRQ{Radio Interrupt?}
        RadioIRQ -- Yes --> ProcessPacket[Process Packet / Event]
        ProcessPacket --> Sleep[Sleep 1ms]
        RadioIRQ -- No --> Sleep
        Sleep --> Loop
    end
```

---

## 3. Implementation Details

- **Safety Overrides**: The float implements non-blocking actuator control with a 1-second stall detection and 8-second timeout to protect the hardware.
- **Persistent Storage**: All mission parameters (PID constants, actuator bounds, etc.) are saved to the internal flash using LittleFS.
- **Checksum Verification**: Every incoming packet is validated using a CRC-8/XOR checksum defined in `packets.h`.
- **Forced Resets**: The `CMD_RESET_FSM` command allows the operator to manually override the state machine and return it to `IDLE`.
