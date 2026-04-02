# Mission Control Dashboard

The Mission Control Dashboard is a real-time interface for monitoring and controlling the X18-Float. It is built using **Streamlit** and communicates with the float via **Serial**.

## Features

- **🔌 Real-time Connectivity:** Connect/Disconnect to the float's COM port.
- **📈 Telemetry Visualization:** Live plotting of Depth (m) vs. Time (s).
- **⚙️ Remote Configuration:**
  - Update PID gains (P, I, D).
  - Set Team/Company ID.
  - Set Mission Duration.
- **🚀 Mission Control:** Trigger pre-dive profiles and download logged data.
- **📋 Live Console:** View raw serial output from the float.
- **📥 Data Export:** Download telemetry sessions as CSV files.

## Prerequisites

- Python 3.8+
- [Streamlit](https://streamlit.io/)
- [pyserial](https://pythonhosted.org/pyserial/)
- [pandas](https://pandas.pydata.org/)
- [plotly](https://plotly.com/python/)

## Installation

```bash
pip install streamlit pyserial pandas plotly
```

## Running the Dashboard

Navigate to the `front_end` directory and run:

```bash
python -m streamlit run main.py
```

## Usage Guide

1. **Connection:** Select the correct COM port in the sidebar and click **Connect**.
2. **Configuration:** Use the forms in the sidebar to update PID values or mission settings. Click the respective "SET" or "UPDATE" buttons to send commands.
3. **Sync:** Click **SYNC FROM FLOAT** to fetch current settings stored on the float.
4. **Mission:** Click **BEGIN PROFILE** to start a mission. The countdown timer will reflect the active duration.
5. **Data:** After a mission, telemetry data can be downloaded using the **DOWNLOAD CSV** button.

## Serial Protocol

The dashboard expects telemetry data in a comma-separated format:
`ID,Time_ms,Depth_m`

Example: `67,12345,0.45`

It also listens for specific status strings:
- `PREDIVE_READY`
- `START DATA DUMP`
- `END DATA DUMP`
- `DATA_DONE`
- `[SYNC]` followed by `Key=Value` pairs (e.g., `P=3.2 I=4.5 D=38.4 Co#=67 Time=40`)
