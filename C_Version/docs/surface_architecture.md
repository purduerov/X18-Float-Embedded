# Surface Station Architecture

This document describes the software architecture for the surface station, including its state machine and main loop execution flow.

---

## 1. Surface FSM (State Transitions)

The Surface Station FSM coordinates the mission stages and handles data logging from the float.

```mermaid
stateDiagram-v2
    [*] --> SURFACE_IDLE
    
    SURFACE_IDLE --> SURFACE_WAITING_PROFILE : Dashboard: Begin Profile\n(Sends CMD_BEGIN_PROFILE)
    
    SURFACE_WAITING_PROFILE --> SURFACE_DOWNLOADING : Received CMD_DONE_PROFILE\n(Sends CMD_SEND_DATA)
    SURFACE_WAITING_PROFILE --> SURFACE_WAITING_PROFILE : Logging Pre-dive Telemetry
    
    state SURFACE_DOWNLOADING {
        [*] --> ReceivingData
        ReceivingData --> ReceivingData : Store Packet & Send ACK
    }
    
    SURFACE_DOWNLOADING --> SURFACE_IDLE : Received CMD_DATA_DONE\n(Dumps CSV)
    
    %% Global Reset
    SURFACE_WAITING_PROFILE --> SURFACE_IDLE : Dashboard: Reset\n(Sends CMD_RESET_FSM)
    SURFACE_DOWNLOADING --> SURFACE_IDLE : Dashboard: Reset\n(Sends CMD_RESET_FSM)
```

---

## 2. Surface Program Flow (Main Loop)

The main loop in `src/surface_main.c` manages communications between the radio and the Streamlit dashboard.

```mermaid
flowchart TD
    Start([Start]) --> Init[Initialize Hardware & Data Logger]
    Init --> Loop[Main Loop]
    
    subgraph LoopSection [Main Execution Loop]
        Loop --> DebugTimer{Debug Timer: 2s?}
        DebugTimer -- Yes --> PrintDebug[Print Status to Console]
        PrintDebug --> SerialIn{Serial Input?}
        
        DebugTimer -- No --> SerialIn
        
        SerialIn -- Yes --> HandleCmd[Parse Command from Dashboard]
        HandleCmd --> RadioIRQ{Radio Interrupt?}
        
        SerialIn -- No --> RadioIRQ
        
        RadioIRQ -- Yes --> ProcessRadio[Process Incoming Packet / ACK]
        ProcessRadio --> Sleep[Sleep 1ms]
        
        RadioIRQ -- No --> Sleep
        Sleep --> Loop
    end
```

---

## 3. Implementation Details

- **Reliability Mechanism**: Uses a **Stop-and-Wait ARQ** protocol during `SURFACE_DOWNLOADING` to ensure every mission sample is received without corruption or loss.
- **Serial Protocol**: Communicates with the Streamlit dashboard using a single-character command protocol over USB-Serial.
- **Data Logging**: Received samples are stored in RAM and dumped to a CSV-compatible format in the console when the mission is complete.
- **Forced Resets**: The `CMD_RESET_FSM` command allows the operator to manually override the surface station's state and return it to `IDLE`.
