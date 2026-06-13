import streamlit as st
import pandas as pd
import plotly.express as px
import html
import streamlit.components.v1 as components
import time
from modules.constants import *

def render_sidebar(hw):
    with st.sidebar:
        st.header(":material/power: Connection")
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
                
        status_txt = ":green[Connected]" if hw.ser and hw.ser.is_open else ":red[Disconnected]"
        st.write(f"**Status:** {status_txt}")
        st.divider()
        
        st.header(":material/settings: Float Settings")
        
        if st.button("ZERO DEPTH", icon=":material/straighten:", width="stretch", type="secondary"):
            hw.zero_depth()
        st.caption("Sets current pressure as 0.0m depth.")

        if st.button("RESET FSM", icon=":material/warning:", width="stretch", type="primary"):
            hw.reset_fsm()
        st.caption("Forces the Surface and Float back to IDLE.")

        if st.button("TEST / CALIBRATE MODE", icon=":material/biotech:", width="stretch", type="secondary"):
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
        st.header(":material/precision_manufacturing: Actuator Control")

        # Quick Presets
        c1, c2 = st.columns(2)
        with c1:
            if st.button("DIVE (0)", icon=":material/arrow_downward:", width="stretch"):
                hw.move_actuator(0)
        with c2:
            if st.button("SURFACE (4095)", icon=":material/arrow_upward:", width="stretch"):
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

        st.divider()
        st.header(":material/publish: OTA Reflash")
        
        col_load, col_upload = st.columns(2)
        with col_load:
            if st.button("Load Built Float Firmware", icon=":material/folder_open:", width="stretch", disabled=hw.reflash_in_progress):
                data, err = hw.load_local_firmware()
                if err:
                    st.session_state["fw_data"] = None
                    st.session_state["fw_error"] = err
                    st.session_state["fw_info"] = None
                else:
                    st.session_state["fw_data"] = data
                    st.session_state["fw_error"] = None
                    crc = hw.calculate_crc32(data)
                    st.session_state["fw_info"] = f"Float Build: {len(data):,} bytes | CRC: 0x{crc:08X}"
        with col_upload:
            uploaded_file = st.file_uploader("Or Upload Custom .bin", type=["bin"], label_visibility="collapsed")
            if uploaded_file is not None:
                st.session_state["fw_data"] = uploaded_file.getvalue()
                st.session_state["fw_error"] = None
                crc = hw.calculate_crc32(st.session_state["fw_data"])
                st.session_state["fw_info"] = f"Uploaded File: {len(st.session_state['fw_data']):,} bytes | CRC: 0x{crc:08X}"

        if "fw_error" in st.session_state and st.session_state["fw_error"]:
            st.error(st.session_state["fw_error"])
            
        if "fw_data" in st.session_state and st.session_state["fw_data"] is not None:
            st.success(st.session_state["fw_info"])
            confirm_flash = st.checkbox("Confirm firmware flash to Float", value=False)
            if st.button("FLASH FIRMWARE", icon=":material/flash_on:", width="stretch", type="primary", disabled=(not confirm_flash or hw.reflash_in_progress)):
                hw.reflash_firmware(st.session_state["fw_data"])
                st.session_state["fw_data"] = None
                st.session_state["fw_info"] = None
                st.rerun()

        if hw.reflash_in_progress:
            st.progress(hw.reflash_progress / 100.0, text=f"Flashing... {hw.reflash_progress}%")
            col_warn, col_cancel = st.columns([3, 1])
            with col_warn:
                st.warning("Do not disconnect during flash! Click Cancel to abort safely.")
            with col_cancel:
                if st.button("Cancel OTA", icon=":material/cancel:", width="stretch", type="secondary"):
                    hw.cancel_reflash()
                    st.rerun()

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
    m2.metric(":material/timer: Countdown", time_left_str)
    m3.metric(":material/height: Max Depth", f"{max_depth:.2f} m")
    m4.metric(":material/query_stats: Data Points", data_points)

def render_packet_log(hw):
    with hw.lock:
        raw_packets = list(hw.packet_log)
    
    formatted_packets = []
    for pkt in raw_packets:
        escaped = html.escape(pkt)
        styled = escaped
        # Add highlight colors for inline telemetry items
        styled = styled.replace("Company #", '<span style="color:#00ffcc; font-weight:bold;">Company #</span><span style="color:#ffffff; font-weight:bold;">')
        styled = styled.replace(", Time:", '</span>, Time:<span style="color:#ffffff;">')
        styled = styled.replace(", Pressure:", '</span>, Pressure:<span style="color:#ffcc00;">')
        styled = styled.replace(", Depth:", '</span>, Depth:<span style="color:#39ff14;">')
        styled += '</span>'
        formatted_packets.append(f'<div style="margin-bottom: 6px; border-bottom: 1px dashed #142834; padding-bottom: 4px;">► {styled}</div>')
        
    log_html = "".join(formatted_packets)
    components.html(PACKET_CONSOLE_STYLE.format(log_html=log_html), height=420)

def render_main_content(hw):
    # Two distinct halves
    left_col, right_col = st.columns(2, gap="large")
    
    with left_col:
        st.subheader(":material/terminal: Left Panel: Text/Data Log")
        st.caption("Scrolling telemetry showing raw incoming packet strings (Depth & Pressure)")
        
        render_packet_log(hw)
        
        st.markdown("### Action Controls")
        c1, c2, c3 = st.columns(3)
        with c1:
            st.button("BEGIN PROFILE", icon=":material/play_arrow:", use_container_width=True, type="primary", key="btn_begin_profile", on_click=lambda: hw.start_profile())
        with c2:
            st.button("SYNC FROM FLOAT", icon=":material/sync:", use_container_width=True, key="btn_sync_settings", on_click=lambda: hw.send_command('?'))
        with c3:
            with hw.lock:
                local_data = list(hw.data_log)
            if local_data:
                df_csv = pd.DataFrame(local_data).to_csv(index=False).encode('utf-8')
                st.download_button("DOWNLOAD CSV", icon=":material/download:", data=df_csv, file_name="mate_profile.csv", mime="text/csv", use_container_width=True, key="btn_download_csv")
            else:
                st.button("DOWNLOAD CSV", icon=":material/download:", use_container_width=True, disabled=True, key="btn_download_csv_disabled")

    with right_col:
        st.subheader(":material/show_chart: Right Panel: Depth vs Time Chart")
        st.caption("Clean digital line graph of under-ice profile depths (No secondary Y-axis)")
        
        with hw.lock:
            local_data = list(hw.data_log)
            
        if local_data:
            df = pd.DataFrame(local_data)
            
            # Downsample if too many points to keep UI snappy
            if len(df) > 300:
                df = df.iloc[::max(1, len(df)//300)]
                
            if "Time (s)" in df.columns and "Depth (m)" in df.columns:
                hover_cols = [c for c in ["Depth (m)", "Pressure (kPa)", "Actuator (ADC)", "Target (ADC)"] if c in df.columns]
                fig = px.scatter(df, x="Time (s)", y="Depth (m)", hover_data=hover_cols, render_mode='webgl', height=400)
                fig.update_traces(mode='lines+markers', line=dict(color='#00ffcc', width=3), marker=dict(size=6, color='#00ffcc'))
                fig.update_yaxes(autorange="reversed", gridcolor='#1e293b', title_text="Depth (m)")
                fig.update_xaxes(gridcolor='#1e293b', title_text="Time (s)")
                fig.update_layout(
                    plot_bgcolor='#070f1a',
                    paper_bgcolor='#070f1a',
                    font=dict(color='#00ffcc', family='monospace'),
                    margin=dict(l=0, r=0, t=10, b=0),
                    hovermode="x unified"
                )
                st.plotly_chart(fig, key="p_depth_chart", use_container_width=True)
                
                # Expandable Actuator Position Chart
                if "Actuator (ADC)" in df.columns:
                    with st.expander("View Actuator Position Chart", icon=":material/precision_manufacturing:"):
                        hover_cols_act = [c for c in ["Actuator (ADC)", "Target (ADC)", "Depth (m)"] if c in df.columns]
                        fig2 = px.scatter(df, x="Time (s)", y="Actuator (ADC)", hover_data=hover_cols_act, render_mode='webgl', height=250)
                        fig2.update_traces(mode='lines+markers', line=dict(color='#ffaa00', width=2), marker=dict(size=4, color='#ffaa00'))
                        fig2.update_yaxes(gridcolor='#1e293b', title_text="Actuator Position (ADC)")
                        fig2.update_xaxes(gridcolor='#1e293b', title_text="Time (s)")
                        fig2.update_layout(
                            plot_bgcolor='#070f1a',
                            paper_bgcolor='#070f1a',
                            font=dict(color='#ffaa00', family='monospace'),
                            margin=dict(l=0, r=0, t=10, b=0),
                            hovermode="x unified"
                        )
                        st.plotly_chart(fig2, key="p_act_chart", use_container_width=True)
            else:
                st.error(f"Telemetry data keys mismatch. Columns: {df.columns.tolist()}")
        else:
            # Informative visual placeholder when no data exists yet
            st.info("Waiting for profile telemetry data... Start a profile to see real-time plots.")
            
        st.markdown("### Active Configuration")
        col_cfg1, col_cfg2, col_cfg3 = st.columns(3)
        with col_cfg1:
            st.markdown(f"**FW Version:** `v{hw.float_settings.get('FW', '--')}`")
            st.markdown(f"**Company ID:** `{hw.float_settings.get('Co#', '--')}`")
            st.markdown(f"**Profiles (N):** `{hw.float_settings.get('N', '--')}`")
        with col_cfg2:
            st.markdown(f"**Deep Target:** `{hw.float_settings.get('Deep', '--')} m`")
            st.markdown(f"**Shallow Target:** `{hw.float_settings.get('Shallow', '--')} m`")
            st.markdown(f"**Hold Duration:** `{hw.float_settings.get('Time', '--')} s`")
        with col_cfg3:
            st.markdown(f"**PID Gains:** `{hw.float_settings.get('P', '--')}/{hw.float_settings.get('I', '--')}/{hw.float_settings.get('D', '--')}`")
            st.markdown(f"**Bounds:** `{hw.float_settings.get('ActMin', '--')} - {hw.float_settings.get('ActMax', '--')}`")
            st.markdown(f"**Neutral ADC:** `{hw.float_settings.get('Neutral', '--')}`")

def render_console(hw):
    st.subheader(":material/developer_board: System Debug Serial Console")
    with hw.lock:
        raw_logs = list(hw.console_log)
    
    formatted_lines = []
    for line in raw_logs:
        # Style lines to maximize readability for debugging
        if "🔴" in line or "[ERROR]" in line or "error" in line.lower() or "critical" in line.lower() or "lost" in line.lower() or "failed" in line.lower():
            color = "#ff4b4b"  # bright red
        elif "🔵" in line or "[TX]" in line or "sent:" in line.lower() or "commanding" in line.lower():
            color = "#00a3ff"  # bright blue
        elif "🟢" in line or "✅" in line or "[SUCCESS]" in line or "[OK]" in line or "success" in line.lower() or "complete" in line.lower() or "synced" in line.lower():
            color = "#00ff66"  # bright green
        elif "🟡" in line or "[WARN]" in line or "warning" in line.lower() or "progress" in line.lower() or "ota" in line.lower():
            color = "#ffd700"  # gold/yellow
        elif "⚠️" in line or "[WARNING]" in line:
            color = "#ffa500"  # orange
        else:
            color = "#a0a5b5"  # default visible grey-blue
            
        escaped = html.escape(line)
        formatted_lines.append(f'<span style="color: {color};">{escaped}</span>')
        
    log_html = "<br>".join(formatted_lines)
    components.html(CONSOLE_STYLE.format(log_html=log_html), height=320)

