import streamlit as st
import pandas as pd
import plotly.express as px
import html
import streamlit.components.v1 as components
import time
from modules.constants import *

def render_sidebar(hw):
    with st.sidebar:
        st.header("🔌 Connection")
        available_ports = hw.get_available_ports()
        selected_port = st.selectbox("COM Port", available_ports) if available_ports else st.text_input("Manual Port", "COM9")
            
        col1, col2 = st.columns(2)
        with col1:
            if st.button("Connect", use_container_width=True, type="primary"): hw.connect(selected_port)
        with col2:
            if st.button("Disconnect", use_container_width=True): hw.disconnect()
                
        st.write(f"**Status:** {'🟢 Connected' if hw.ser and hw.ser.is_open else '🔴 Disconnected'}")
        st.divider()
        
        st.header("⚙️ Float Settings")
        
        if st.button("📏 ZERO DEPTH", use_container_width=True, type="secondary"):
            hw.zero_depth()
        st.caption("Sets current pressure as 0.0m depth.")

        if st.button("⚠️ RESET FSM", use_container_width=True, type="primary"):
            hw.reset_fsm()
        st.caption("Forces the Surface and Float back to IDLE.")

        if st.button("🧪 TEST / CALIBRATE MODE", use_container_width=True, type="secondary"):
            hw.test_mode()
        st.caption("Continuously stream live depth and ADC.")

        with st.form("team_id_form"):
            new_id = st.number_input("Team ID", step=1, value=DEFAULT_TEAM_ID)
            if st.form_submit_button("SET TEAM ID", use_container_width=True): hw.update_team_id(int(new_id))
                
        with st.form("duration_form"):
            new_dur = st.number_input("Duration (Secs)", step=1, value=DEFAULT_DURATION_S)
            if st.form_submit_button("SET DURATION", use_container_width=True): hw.update_duration(int(new_dur))

        with st.form("pid_form"):
            p_val = st.number_input("P", step=0.1, value=DEFAULT_P)
            i_val = st.number_input("I", step=0.1, value=DEFAULT_I)
            d_val = st.number_input("D", step=0.1, value=DEFAULT_D)
            if st.form_submit_button("UPDATE GAINS", use_container_width=True): hw.update_pid(round(p_val,2), round(i_val,2), round(d_val,2))

        st.divider()
        st.header("🦾 Actuator Control")
        with st.form("actuator_form"):
            act_pos = st.number_input("Target Position (0-4095)", min_value=0, max_value=4095, value=DEFAULT_ACTUATOR_POS, step=100)
            if st.form_submit_button("MOVE ACTUATOR", use_container_width=True): hw.move_actuator(int(act_pos))

        with st.form("bounds_form"):
            st.write("**Set Limits**")
            b_min = st.number_input("Min ADC", min_value=0, max_value=4095, value=0)
            b_max = st.number_input("Max ADC", min_value=0, max_value=4095, value=4095)
            if st.form_submit_button("UPDATE BOUNDS", use_container_width=True): hw.update_bounds(int(b_min), int(b_max))

def render_metrics(hw):
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

    m1, m2, m3, m4, m5 = st.columns(5)
    m1.metric("Mission State", hw.mission_status)
    m2.metric("⏱️ Countdown", time_left_str)
    m3.metric("Max Depth", f"{max_depth:.2f} m")
    m4.metric("2.5m Hold", "✅ VALIDATED" if hold25 >= 7 else "⏳ Searching")
    m5.metric("40cm Hold", "✅ VALIDATED" if hold40 >= 7 else "⏳ Searching")

def render_main_content(hw):
    col_chart, col_actions = st.columns([4, 1], gap="medium")
    
    with col_chart:
        if hw.data_log:
            df = pd.DataFrame(hw.data_log)
            fig = px.line(df, x="Time (s)", y="Depth (m)", height=350)
            fig.update_yaxes(autorange="reversed")
            fig.update_layout(margin=dict(l=0, r=0, t=10, b=0))
            st.plotly_chart(fig, use_container_width=True, key="depth_chart")
        else:
            st.info("Waiting for telemetry data...")

    with col_actions:
        st.button("🚀 BEGIN PROFILE", use_container_width=True, type="primary", on_click=lambda: hw.start_profile())
        st.button("🔄 SYNC FROM FLOAT", use_container_width=True, on_click=lambda: hw.send_command('?'))
        
        st.markdown("### Active Config")
        st.write(f"**ID:** {hw.float_settings.get('Co#', '--')} | **Time:** {hw.float_settings.get('Time', '--')}s")
        st.write(f"**PID:** {hw.float_settings.get('P', '--')} / {hw.float_settings.get('I', '--')} / {hw.float_settings.get('D', '--')}")
        st.write(f"**Bounds:** {hw.float_settings.get('ActMin', '--')} - {hw.float_settings.get('ActMax', '--')}")
        st.write(f"**Live Depth:** {hw.float_settings.get('LiveDepth', '--')} m")
        st.write(f"**Live ADC:** {hw.float_settings.get('ADC', '--')}")
        
        if hw.data_log:
            df_csv = pd.DataFrame(hw.data_log).to_csv(index=False).encode('utf-8')
            st.download_button("📥 DOWNLOAD CSV", data=df_csv, file_name="mate_profile.csv", mime="text/csv", use_container_width=True)

def render_console(hw):
    st.markdown("**Live Serial Console**")
    escaped_logs = [html.escape(line) for line in hw.console_log]
    log_html = "<br>".join(escaped_logs)
    components.html(CONSOLE_STYLE.format(log_html=log_html), height=220)
