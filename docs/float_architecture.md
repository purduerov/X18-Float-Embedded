# Float Unit (Underwater) Architecture

This document describes the software architecture for the underwater float, including its program flow and state machine logic.

---

## 1. Float Program Flow (Main Loop)

The float firmware uses a unified boot sequence followed by a multi-rate control loop.

```mermaid
flowchart TD
    Start([Start]) --> Init[Unified system_init]
    Init --> ReadSettings[Read Flash Config]
    ReadSettings --> Loop[Main Loop]
    
    subgraph LoopSection [Main Execution Loop]
        Loop --> TimerPID{PID Timer:<br/>100ms?}
        TimerPID -- Yes --> ReadSensor[Read Depth Sensor]
        ReadSensor --> CalcPID[Calculate Depth PID]
        CalcPID --> MoveAct[Command Actuator]
        MoveAct --> FSMUpdate[Update FSM &<br/>Log Data]
        
        TimerPID -- No --> TimerAct{Act Timer:<br/>20ms?}
        TimerAct -- Yes --> TickAct[Actuator Tick]
        TickAct --> FSMUpdate
        TimerAct -- No --> FSMUpdate
        
        FSMUpdate --> RadioIRQ{Radio<br/>Event?}
        RadioIRQ -- Yes --> ProcessPacket[Process Packet]
        ProcessPacket --> Sleep[Sleep 1ms]
        RadioIRQ -- No --> Sleep
        Sleep --> Loop
    end
```

### Control Rates
- **Depth PID (10Hz)**: Calculates the required buoyancy adjustment based on target depth.
- **Actuator Loop (50Hz)**: Handles non-blocking motor control, including stall detection and soft-start.
- **State Machine (ASAP)**: Processes incoming radio commands and transitions through mission phases.

---

## 2. Float FSM (State Transitions)

The Float FSM manages the mission lifecycle, from waiting for surface commands to executing the dive profile and surfacing to transfer data.

```mermaid
stateDiagram-v2
    [*] --> FLOAT_IDLE
    
    FLOAT_IDLE --> FLOAT_PRE_DIVE : Received\nCMD_BEGIN_PROFILE
    FLOAT_IDLE --> FLOAT_TEST_CALIBRATE : Received\nCMD_ENTER_TEST
    
    FLOAT_PRE_DIVE --> FLOAT_PROFILING : Pre-dive TX\nFinished
    
    state FLOAT_PROFILING {
        [*] --> Sampling
        Sampling --> Sampling : 1s Interval
        Sampling --> [*] : Hold complete OR\nSafety Timeout OR\nBuffer Full
    }
    
    FLOAT_PROFILING --> FLOAT_PROFILE_DONE : Mission Finished
    
    FLOAT_PROFILE_DONE --> FLOAT_DUMPING_DATA : Received\nCMD_SEND_DATA
    FLOAT_PROFILE_DONE --> FLOAT_PROFILE_DONE : Broadcast\nDONE_PROFILE (3s)
    
    FLOAT_DUMPING_DATA --> FLOAT_IDLE : All Data Sent\n(CMD_DATA_DONE)
    FLOAT_DUMPING_DATA --> FLOAT_DUMPING_DATA : Wait for ACKs /\nRetransmit
```

---

## 3. NeoPixel Status LED

The float uses an on-board NeoPixel (GPIO 16) to provide immediate visual feedback of its internal state. This is driven by the RP2040's hardware PIO (Programmable I/O) to ensure timing does not interfere with time-critical PID control.

| State | Color | Description |
| :--- | :--- | :--- |
| **IDLE** | 🟢 **Green** | Ready and waiting for commands from the Surface Station. |
| **PRE_DIVE** | 🟡 **Yellow** | Transmitting starting telemetry packet. |
| **PROFILING** | 🔵 **Blue** | **Autonomous PID active.** Actively managing depth. |
| **PROFILE_DONE** | 💠 **Cyan** | Mission finished; surfaced and broadcasting beacon. |
| **DUMPING_DATA** | 🟣 **Magenta** | Transferring high-resolution data log to Surface. |
| **TEST_MODE** | ⚪ **White** | Live streaming depth/ADC for real-time calibration. |
| **ERROR** | 🔴 **Red** | System error or unexpected FSM state transition. |

---

## 4. Implementation Details

- **Unified Boot**: `system_init()` handles the deterministic startup of Serial, Storage (LittleFS), I2C, MS5837 Sensor, and the SX1276 Radio.
- **Safety Overrides**: Implements 1-second stall detection and 8-second total movement timeout to protect the buoyancy mechanism.
- **Persistent Storage**: Mission parameters (PID, bounds, depth offset) are saved to internal flash.
- **Checksum Verification**: Every incoming packet is validated using an XOR checksum.
