# 2026 MATE ROV Competition: Task 4 - MATE Floats Under the Ice

## Mission Overview
Design, build, and operate an independent vertical profiling float that mimics real-world oceanographic floats used in polar regions.

## 1. Pre-Dive Communication
- **Requirement**: Transmit at least one "defined data packet" before descending.
- **Payload**:
  - Company Number
  - Time Data
  - Pressure/Depth Data
- **Status**: Currently implemented in `FLOAT_PRE_DIVE` state.

## 2. Vertical Profiling (Under-Ice Simulation)
The float must complete **two** identical vertical profiles using a **buoyancy engine**.

### Profile Sequence (Repeated twice):
1. **Deep Depth Maintenance**:
   - Target: **2.5 meters** (+/- 33 cm)
   - Duration: **30 consecutive seconds**
2. **Shallow Depth Maintenance**:
   - Target: **40 cm** (0.4m) (+/- 33 cm)
   - Duration: **30 consecutive seconds**

### Operational Constraints:
- **Ice Avoidance**: Must NOT break the surface (Depth > 0) or contact the ice sheet (which usually means staying within the specified bands).
- **Buoyancy Engine**: No thrusters or propellers allowed.

## 3. Recovery and Data Transmission
- **Recovery**: Graspable feature (U-bolt/rope loop) required (5cm wide, 5cm protrusion).
- **Post-Recovery Transmission**: Autonomously and wirelessly transmit recorded data packets to the shore station once on the pool deck.
- **Requirement**: At least 20 data packets must be transmitted.
- **Status**: Implemented in `FLOAT_DUMPING_DATA` state.

## 4. Data Analysis
- **Graphing**: Computer-generated graph of **Depth vs. Time**.
- **Requirement**: Must include at least 20 data packets.
- **Status**: Supported by the Dashboard's auto-save CSV feature and Plotly charts.

## 5. Physical Specifications
- **Dimensions**: < 1m height/length, < 18cm diameter/width.
- **Independence**: No tether or airline.
- **Power**: NiMH or AGM batteries only, fused within 5cm of battery pack.

---

## Final Implementation Plan
To fully meet these requirements, the current FSM needs to be updated from a single target depth to a **Mission Sequencer**.

### Proposed FSM Updates:
1. **State Expansion**:
   - `FLOAT_PROFILING` should be split or handled by a sub-state machine.
   - Sequence: `PRE_DIVE` -> `DIVE_1_DEEP` -> `DIVE_1_SHALLOW` -> `DIVE_2_DEEP` -> `DIVE_2_SHALLOW` -> `DONE`.
2. **Dynamic Target Syncing**: 
   - Instead of a single `settings.target_depth`, the FSM should cycle through a predefined list of targets.
3. **Arrival Timer Persistence**:
   - Ensure the 30s countdown only starts when within +/- 0.33m.
   - If it drifts out of the band, the timer should pause or reset (depending on rule interpretation - usually "consecutive" means it must reset).
