import streamlit as st
import streamlit.components.v1 as components
import serial
import serial.tools.list_ports
import threading
import pandas as pd
import plotly.express as px
import time
import re
import html
import os
import json

# -----------------------------------------
# 1. HARDWARE & BACKGROUND THREAD MANAGER
# -----------------------------------------
class HardwareManager:
    """Manages the serial connection and state behind the scenes."""
    def __init__(self):
        self.ser = None
        self.data_log = []
        self.console_log = []
        self.lock = threading.Lock()
        self.mission_status = "IDLE"
        self.first_timestamp = None
        
        self.float_settings = {"P": "--", "I": "--", "D": "--", "Co#": "67", "Time": "40"}
        
        # Countdown Timer variables
        self.profile_start_time = None
        self.active_duration = 0
        
        self.running = True
        self.thread = threading.Thread(target=self.serial_listener, daemon=True)
        self.thread.start()

    def get_available_ports(self):
        ports = serial.tools.list_ports.comports()
        return [port.device for port in ports]

    def connect(self, port, baud=115200):
        with self.lock:
            if self.ser and self.ser.is_open:
                try:
                    self.ser.close()
                except:
                    pass
            try:
                # Use shorter timeout for better responsiveness
                self.ser = serial.Serial(port, baud, timeout=0.05, write_timeout=0.5)
                # Force DTR/RTS to reset Pico serial if needed
                self.ser.dtr = False
                self.ser.rts = False
                time.sleep(0.1)
                self.ser.dtr = True
                self.ser.rts = True
                
                self.console_log.append(f"🟢 Connected to {port} at {baud} baud.")
                self.mission_status = "IDLE"
            except Exception as e:
                self.ser = None
                self.console_log.append(f"🔴 ERROR: Could not connect to {port}. {e}")

    def disconnect(self):
        with self.lock:
            if self.ser:
                try:
                    self.ser.close()
                except:
                    pass
                self.ser = None
                self.console_log.append("⚪ Disconnected.")
                self.mission_status = "DISCONNECTED"

    def send_command(self, cmd):
        with self.lock:
            if self.ser and self.ser.is_open:
                try:
                    self.ser.write(f"{cmd}\n".encode('utf-8'))
                    self.console_log.append(f"🔵 > Sent: {cmd}")
                except (serial.SerialException, OSError) as e:
                    self.console_log.append(f"🔴 Connection Lost: {e}")
                    self.ser = None
                    self.mission_status = "DISCONNECTED"
            else:
                self.console_log.append("🔴 Cannot send command: Not connected.")

    # Optimistic Updates: Updates local memory instantly before the float even responds
    def update_team_id(self, val):
        self.send_command(f"c {val}")
        self.float_settings["Co#"] = str(val)
        
    def update_duration(self, val):
        self.send_command(f"t {val}")
        self.float_settings["Time"] = str(val)
        
    def update_pid(self, p, i, d):
        self.send_command(f"s {p} {i} {d}")
        self.float_settings["P"] = str(p)
        self.float_settings["I"] = str(i)
        self.float_settings["D"] = str(d)

    def zero_depth(self):
        self.send_command("z")

    def reset_fsm(self):
        self.send_command("r")
        self.mission_status = "IDLE"
        self.console_log.append("⚠️ > Sent: r (Forced FSM Reset)")

    def move_actuator(self, val):
        self.send_command(f"a {val}")

    def start_profile(self):
        """Triggers the start command and starts the timer ONLY."""
        self.send_command('p') 
        try:
            self.active_duration = int(self.float_settings.get("Time", 40))
        except ValueError:
            self.active_duration = 40
        self.profile_start_time = time.time()

    def serial_listener(self):
        while self.running:
            if self.ser and self.ser.is_open:
                try:
                    if self.ser.in_waiting > 0:
                        line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                        if line:
                            self.console_log.append(line)
                            if len(self.console_log) > 100: 
                                self.console_log.pop(0)
                            
                            if "PREDIVE_READY" in line: 
                                self.mission_status = "PRE-DIVE READY"
                            elif "START DATA DUMP" in line:
                                self.mission_status = "DOWNLOADING DATA"
                                self.data_log.clear() 
                                self.first_timestamp = None 
                            elif "DATA_DONE" in line: 
                                self.mission_status = "MISSION COMPLETE"
                            elif "[SYNC]" in line:
                                matches = re.findall(r'([A-Za-z0-9#]+)=([\d\.]+)', line)
                                if matches:
                                    for key, value in matches:
                                        if key in self.float_settings:
                                            self.float_settings[key] = value
                                    self.console_log.append(f"✅ UI Synced Successfully.")
                                    
                            parts = line.split(',')
                            if len(parts) == 3 and parts[0].isdigit():
                                try:
                                    abs_time_ms = int(parts[1])
                                    depth_m = float(parts[2])
                                    if self.first_timestamp is None:
                                        self.first_timestamp = abs_time_ms
                                    rel_time_s = (abs_time_ms - self.first_timestamp) / 1000.0
                                    self.data_log.append({
                                        "Time (s)": rel_time_s,
                                        "Depth (m)": depth_m
                                    })
                                except ValueError:
                                    pass
                except (serial.SerialException, OSError, Exception) as e:
                    with self.lock:
                        if self.ser:
                            try: self.ser.close()
                            except: pass
                            self.ser = None
                            self.mission_status = "DISCONNECTED"
                            self.console_log.append(f"🔴 Serial error: {e}")
            else:
                time.sleep(0.01)

@st.cache_resource
def get_hardware():
    return HardwareManager()

hw = get_hardware()

# -----------------------------------------
# 2. UI LAYOUT & DASHBOARD
# -----------------------------------------
st.set_page_config(page_title="Mission Control", layout="wide", page_icon="🌊")

# --- SIDEBAR (CONNECTION & SETTINGS) ---
with st.sidebar:
    st.header("🔌 Connection")
    available_ports = hw.get_available_ports()
    selected_port = st.selectbox("COM Port", available_ports) if available_ports else st.text_input("Manual Port", "COM9")
        
    col1, col2 = st.columns(2)
    with col1:
        if st.button("Connect", width="stretch", type="primary"): hw.connect(selected_port)
    with col2:
        if st.button("Disconnect", width="stretch"): hw.disconnect()
            
    st.write(f"**Status:** {'🟢 Connected' if hw.ser and hw.ser.is_open else '🔴 Disconnected'}")
    st.divider()
    
    st.header("⚙️ Float Settings")
    
    if st.button("📏 ZERO DEPTH", width="stretch", type="secondary"):
        hw.zero_depth()
    st.caption("Sets current pressure as 0.0m depth.")

    if st.button("⚠️ RESET FSM", width="stretch", type="primary"):
        hw.reset_fsm()
    st.caption("Forces the Surface and Float back to IDLE.")

    with st.form("team_id_form"):
        new_id = st.number_input("Team ID", step=1, value=67)
        if st.form_submit_button("SET TEAM ID", width="stretch"): hw.update_team_id(int(new_id))
            
    with st.form("duration_form"):
        new_dur = st.number_input("Duration (Secs)", step=1, value=40)
        if st.form_submit_button("SET DURATION", width="stretch"): hw.update_duration(int(new_dur))

    with st.form("pid_form"):
        p_val = st.number_input("P", step=0.1, value=3.2)
        i_val = st.number_input("I", step=0.1, value=4.5)
        d_val = st.number_input("D", step=0.1, value=38.4)
        if st.form_submit_button("UPDATE GAINS", width="stretch"): hw.update_pid(round(p_val,2), round(i_val,2), round(d_val,2))

    st.divider()
    st.header("🦾 Actuator Control")
    with st.form("actuator_form"):
        act_pos = st.number_input("Target Position (0-4095)", min_value=0, max_value=4095, value=2000, step=100)
        if st.form_submit_button("MOVE ACTUATOR", width="stretch"): hw.move_actuator(int(act_pos))

# --- MAIN DASHBOARD ---
st.title("🌊 MATE Floats 2026: Mission Control")

# Auto-refreshes every 0.25 seconds
@st.fragment(run_every="0.25s")
def live_dashboard():
    # 1. Calculate live metrics & timer
    max_depth = 0.0
    hold25, hold40 = 0, 0
    
    for p in hw.data_log:
        d = p["Depth (m)"]
        if d > max_depth: max_depth = d
        if 2.17 <= d <= 2.83: hold25 += 1
        else: hold25 = 0
        if 0.07 <= d <= 0.73: hold40 += 1
        else: hold40 = 0

    time_left_str = "--"
    if hw.profile_start_time:
        elapsed = time.time() - hw.profile_start_time
        remaining = int(hw.active_duration - elapsed)
        if remaining > 0:
            time_left_str = f"{remaining}s"
        else:
            time_left_str = "DONE"
            hw.profile_start_time = None

    # 2. Top Row: Key Metrics
    m1, m2, m3, m4, m5 = st.columns(5)
    m1.metric("Mission State", hw.mission_status)
    m2.metric("⏱️ Countdown", time_left_str)
    m3.metric("Max Depth", f"{max_depth:.2f} m")
    m4.metric("2.5m Hold", "✅ VALIDATED" if hold25 >= 7 else "⏳ Searching")
    m5.metric("40cm Hold", "✅ VALIDATED" if hold40 >= 7 else "⏳ Searching")

    st.divider()

    # 3. Middle Row: Chart (Left) & Controls (Right)
    col_chart, col_actions = st.columns([4, 1], gap="medium")
    
    with col_chart:
        if hw.data_log:
            df = pd.DataFrame(hw.data_log)
            fig = px.line(df, x="Time (s)", y="Depth (m)", height=350)
            fig.update_yaxes(autorange="reversed")
            fig.update_layout(margin=dict(l=0, r=0, t=10, b=0))
            st.plotly_chart(fig, width="stretch", key="depth_chart")
        else:
            st.info("Waiting for telemetry data...")

    with col_actions:
        st.button("🚀 BEGIN PROFILE", width="stretch", type="primary", on_click=lambda: hw.start_profile())
        st.button("🔄 SYNC FROM FLOAT", width="stretch", on_click=lambda: hw.send_command('?'))
        
        st.markdown("### Active Config")
        st.write(f"**ID:** {hw.float_settings['Co#']} | **Time:** {hw.float_settings['Time']}s")
        st.write(f"**PID:** {hw.float_settings['P']} / {hw.float_settings['I']} / {hw.float_settings['D']}")
        
        if hw.data_log:
            df_csv = pd.DataFrame(hw.data_log).to_csv(index=False).encode('utf-8')
            st.download_button("📥 DOWNLOAD CSV", data=df_csv, file_name="mate_profile.csv", mime="text/csv", width="stretch")

    # 4. Bottom Row: Custom Auto-Scrolling Console via HTML/JS injection
    st.markdown("**Live Serial Console**")
    
    escaped_logs = [html.escape(line) for line in hw.console_log]
    log_html = "<br>".join(escaped_logs)
    
    # This snippet forces a beautiful dark-mode terminal div that guarantees it scrolls to the bottom
    auto_scroll_html = f"""
    <div id="term" style="background-color: #0e1117; color: #00ff00; font-family: 'Courier New', Courier, monospace; 
         font-size: 14px; height: 200px; overflow-y: auto; padding: 10px; border: 1px solid #444; border-radius: 5px;">
        {log_html}
    </div>
    <script>
        var d = document.getElementById("term");
        d.scrollTop = d.scrollHeight;
    </script>
    """
    # Render the HTML block natively in Streamlit
    components.html(auto_scroll_html, height=220)

live_dashboard()
