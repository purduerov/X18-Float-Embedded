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
        if available_ports:
            selected_port_obj = st.selectbox(
                "COM Port", 
                available_ports, 
                format_func=lambda x: f"{x.device} - {x.description}"
            )
            selected_port = selected_port_obj.device
        else:
            selected_port = st.text_input("Manual Port", "COM9")
            
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

        if st.button("🧪 TEST / CALIBRATE MODE", width="stretch", type="secondary"):
            hw.test_mode()
        st.caption("Continuously stream live depth and ADC.")

        with st.form("team_id_form"):
            new_id = st.number_input("Team ID", step=1, value=DEFAULT_TEAM_ID)
            if st.form_submit_button("SET TEAM ID", width="stretch"): hw.update_team_id(int(new_id))
                
        with st.form("duration_form"):
            new_dur = st.number_input("Duration (Secs)", step=1, value=DEFAULT_DURATION_S)
            if st.form_submit_button("SET DURATION", width="stretch"): hw.update_duration(int(new_dur))

        with st.form("deep_depth_form"):
            new_deep = st.number_input("Deep Target (m)", step=0.1, value=DEFAULT_DEEP_TARGET)
            if st.form_submit_button("SET DEEP TARGET", width="stretch"): hw.update_deep_target(float(new_deep))

        with st.form("shallow_depth_form"):
            new_shallow = st.number_input("Shallow Target (m)", step=0.1, value=DEFAULT_SHALLOW_TARGET, help="Set to 0 to skip shallow stage")
            if st.form_submit_button("SET SHALLOW TARGET", width="stretch"): hw.update_shallow_target(float(new_shallow))

        with st.form("num_profiles_form"):
            new_count = st.number_input("Number of Profiles", step=1, value=DEFAULT_NUM_PROFILES, min_value=1)
            if st.form_submit_button("SET PROFILE COUNT", width="stretch"): hw.update_num_profiles(int(new_count))

        with st.form("tolerance_form"):
            new_tol = st.number_input("Arrival Tolerance (m)", min_value=0.01, max_value=2.0, step=0.01, value=0.1)
            if st.form_submit_button("SET TOLERANCE", width="stretch"): hw.update_tolerance(float(new_tol))

        with st.form("pid_form"):
            p_val = st.number_input("P", step=0.1, value=DEFAULT_P)
            i_val = st.number_input("I", step=0.1, value=DEFAULT_I)
            d_val = st.number_input("D", step=0.1, value=DEFAULT_D)
            if st.form_submit_button("UPDATE GAINS", width="stretch"): hw.update_pid(round(p_val,2), round(i_val,2), round(d_val,2))

        st.divider()
        st.header("🦾 Actuator Control")

        # Quick Presets
        c1, c2 = st.columns(2)
        with c1:
            if st.button("🔽 DIVE (0)", width="stretch"):
                hw.move_actuator(0)
        with c2:
            if st.button("🔼 SURFACE (4095)", width="stretch"):
                hw.move_actuator(4095)

        with st.form("actuator_form"):
            act_pos = st.number_input("Target Position (0-4095)", min_value=0, max_value=4095, value=DEFAULT_ACTUATOR_POS, step=100)
            if st.form_submit_button("MOVE TO CUSTOM", width="stretch"): 
                hw.move_actuator(int(act_pos))

        with st.form("bounds_form"):
            st.write("**Set Limits**")
            b_min = st.number_input("Min ADC", min_value=0, max_value=4095, value=0)
            b_max = st.number_input("Max ADC", min_value=0, max_value=4095, value=4095)
            if st.form_submit_button("UPDATE BOUNDS", width="stretch"): hw.update_bounds(int(b_min), int(b_max))

        with st.form("neutral_form"):
            st.write("**Buoyancy Baseline**")
            n_adc = st.number_input("Neutral ADC", min_value=0, max_value=4095, value=2048, step=50)
            if st.form_submit_button("SET NEUTRAL ADC", width="stretch"): hw.update_neutral_adc(int(n_adc))

def render_metrics(hw):
    with hw.lock:
        data_points = len(hw.data_log)
        max_depth = 0.0
        if data_points > 0:
            # More efficient way to get max depth
            max_depth = max(p.get("Depth (m)", 0.0) for p in hw.data_log)

    time_left_str = "--"
    if hw.profile_start_time:
        elapsed = time.time() - hw.profile_start_time
        remaining = int(hw.active_duration - elapsed)
        if remaining > 0:
            time_left_str = f"{remaining}s"
        else:
            time_left_str = "DONE"
            hw.profile_start_time = None

    m1, m2, m3, m4 = st.columns(4)
    m1.metric("Mission State", hw.mission_status)
    m2.metric("⏱️ Countdown", time_left_str)
    m3.metric("Max Depth", f"{max_depth:.2f} m")
    m4.metric("Data Points", data_points)

def render_main_content(hw):
    col_chart, col_actions = st.columns([4, 1], gap="medium")
    
    with col_chart:
        with hw.lock:
            local_data = list(hw.data_log)
        
        if local_data:
            df = pd.DataFrame(local_data)
            
            # Downsample if too many points to keep UI snappy
            if len(df) > 300:
                df = df.iloc[::max(1, len(df)//300)]

            # Ensure required columns exist
            if "Time (s)" in df.columns and "Depth (m)" in df.columns:
                tab1, tab2 = st.tabs(["📉 Depth Profile", "🦾 Actuator Position"])
                
                with tab1:
                    # Use Scattergl (Web GL) for high-performance plotting of large datasets
                    hover_cols = [c for c in ["Depth (m)", "Actuator (ADC)", "Target (ADC)"] if c in df.columns]
                    fig = px.scatter(df, x="Time (s)", y="Depth (m)", hover_data=hover_cols, render_mode='webgl', height=350)
                    fig.update_traces(mode='lines+markers', line=dict(width=2), marker=dict(size=4))
                    fig.update_yaxes(autorange="reversed")
                    fig.update_layout(margin=dict(l=0, r=0, t=10, b=0), hovermode="x unified")
                    st.plotly_chart(fig, width="stretch", key="p_depth_chart", use_container_width=True)
                
                with tab2:
                    if "Actuator (ADC)" in df.columns:
                        hover_cols = [c for c in ["Actuator (ADC)", "Target (ADC)", "Depth (m)"] if c in df.columns]
                        fig2 = px.scatter(df, x="Time (s)", y="Actuator (ADC)", hover_data=hover_cols, render_mode='webgl', height=350)
                        fig2.update_traces(mode='lines+markers', line=dict(width=2), marker=dict(size=4))
                        fig2.update_layout(margin=dict(l=0, r=0, t=10, b=0), hovermode="x unified")
                        st.plotly_chart(fig2, width="stretch", key="p_act_chart", use_container_width=True)
                    else:
                        st.info("Actuator data not available for this session.")
            else:
                st.error(f"Telemetry data keys mismatch. Columns: {df.columns.tolist()}")
        else:
            st.info("Waiting for telemetry data... (No data points received yet)")

    with col_actions:
        st.button("🚀 BEGIN PROFILE", width="stretch", type="primary", on_click=lambda: hw.start_profile())
        st.button("🔄 SYNC FROM FLOAT", width="stretch", on_click=lambda: hw.send_command('?'))
        
        st.markdown("### Active Config")
        st.write(f"**ID:** {hw.float_settings.get('Co#', '--')} | **Profiles:** {hw.float_settings.get('N', '--')}")
        st.write(f"**Deep Target:** {hw.float_settings.get('Deep', '--')} m")
        st.write(f"**Shallow Target:** {hw.float_settings.get('Shallow', '--')} m")
        st.write(f"**Hold Duration:** {hw.float_settings.get('Time', '--')} s")
        st.write(f"**Arrival Tol:** {hw.float_settings.get('Tol', '--')} m")
        st.write(f"**PID:** {hw.float_settings.get('P', '--')} / {hw.float_settings.get('I', '--')} / {hw.float_settings.get('D', '--')}")
        st.write(f"**Bounds:** {hw.float_settings.get('ActMin', '--')} - {hw.float_settings.get('ActMax', '--')}")
        st.write(f"**Neutral ADC:** {hw.float_settings.get('Neutral', '--')}")
        st.write(f"**Depth Offset:** {hw.float_settings.get('Off', '--')} m")
        st.write(f"**Live Depth:** {hw.float_settings.get('LiveDepth', '--')} m")
        st.write(f"**Live ADC:** {hw.float_settings.get('ADC', '--')}")
        
        if local_data:
            df_csv = pd.DataFrame(local_data).to_csv(index=False).encode('utf-8')
            st.download_button("📥 DOWNLOAD CSV", data=df_csv, file_name="mate_profile.csv", mime="text/csv", width="stretch")

def render_console(hw):
    st.markdown("**Live Serial Console**")
    with hw.lock:
        escaped_logs = [html.escape(line) for line in hw.console_log]
    log_html = "<br>".join(escaped_logs)
    components.html(CONSOLE_STYLE.format(log_html=log_html), height=220)
