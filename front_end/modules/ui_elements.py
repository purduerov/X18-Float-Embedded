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
            if st.button("Connect", use_container_width=True, type="primary"): hw.connect(selected_port)
        with col2:
            if st.button("Disconnect", use_container_width=True): hw.disconnect()
                
        status_txt = ":green[Connected]" if hw.ser and hw.ser.is_open else ":red[Disconnected]"
        st.write(f"**Surface Link:** {status_txt}")
        
        # Show Surface FSM State with warning if non-IDLE
        surface_state = hw.mission_status
        if surface_state == "WAITING_PROFILE":
            st.warning("⚠️ Surface is waiting for Float. Radio commands are BLOCKED. Click RESET FSM below to return to IDLE.")
        elif surface_state == "DOWNLOADING":
            st.info("ℹ️ Surface is downloading data. Please wait.")
        st.write(f"**Surface State:** `{surface_state}`")

        hw.hil_enabled = st.checkbox(
            "Enable HIL Simulation", 
            value=hw.hil_enabled,
            help="Bypasses physical depth sensor and runs pool physics based on real actuator ADC."
        )
        
        if hw.hil_enabled:
            st.markdown("#### HIL Target Link (Float Pico)")
            if available_ports:
                # Filter out the primary port to avoid conflicts
                hil_ports = [p for p in available_ports if p.device != selected_port]
                if hil_ports:
                    selected_hil_port_obj = st.selectbox(
                        "HIL COM Port", 
                        hil_ports, 
                        format_func=lambda x: f"{x.device} - {x.description}",
                        key="hil_port_selectbox"
                    )
                    selected_hil_port = selected_hil_port_obj.device
                else:
                    selected_hil_port = st.text_input("Manual HIL Port", "COM9", key="hil_port_manual")
            else:
                selected_hil_port = st.text_input("Manual HIL Port", "COM9", key="hil_port_manual")
                
            col_hil1, col_hil2 = st.columns(2)
            with col_hil1:
                if st.button("Connect HIL", use_container_width=True, type="secondary"): 
                    hw.connect_hil(selected_hil_port)
            with col_hil2:
                if st.button("Disconnect HIL", use_container_width=True): 
                    hw.disconnect_hil()
                    
            hil_status_txt = ":green[Connected]" if hw.hil_ser and hw.hil_ser.is_open else ":red[Disconnected]"
            st.write(f"**HIL Link:** {hil_status_txt}")
            
        st.divider()
        
        st.header(":material/settings: Float Settings")
        
        if st.button("ZERO DEPTH", icon=":material/straighten:", use_container_width=True, type="secondary"):
            hw.zero_depth()
        st.caption("Sets current pressure as 0.0m depth.")

        if st.button("RESET FSM", icon=":material/warning:", use_container_width=True, type="primary"):
            hw.reset_fsm()
        st.caption("Forces the Surface and Float back to IDLE.")

        if st.button("TEST / CALIBRATE MODE", icon=":material/biotech:", use_container_width=True, type="secondary"):
            hw.test_mode()
        st.caption("Continuously stream live depth and ADC.")

        with st.form("team_id_form"):
            new_id = st.number_input("Team ID", step=1, value=DEFAULT_TEAM_ID)
            if st.form_submit_button("SET TEAM ID", use_container_width=True): hw.update_team_id(int(new_id))
                
        with st.form("duration_form"):
            new_dur = st.number_input("Duration (Secs)", step=1, value=DEFAULT_DURATION_S)
            if st.form_submit_button("SET DURATION", use_container_width=True): hw.update_duration(int(new_dur))

        with st.form("deep_depth_form"):
            new_deep = st.number_input("Deep Target (m)", step=0.1, value=DEFAULT_DEEP_TARGET)
            if st.form_submit_button("SET DEEP TARGET", use_container_width=True): hw.update_deep_target(float(new_deep))

        with st.form("shallow_depth_form"):
            new_shallow = st.number_input("Shallow Target (m)", step=0.1, value=DEFAULT_SHALLOW_TARGET, help="Set to 0 to skip shallow stage")
            if st.form_submit_button("SET SHALLOW TARGET", use_container_width=True): hw.update_shallow_target(float(new_shallow))

        with st.form("num_profiles_form"):
            new_count = st.number_input("Number of Profiles", step=1, value=DEFAULT_NUM_PROFILES, min_value=1)
            if st.form_submit_button("SET PROFILE COUNT", use_container_width=True): hw.update_num_profiles(int(new_count))

        with st.form("tolerance_form"):
            new_tol = st.number_input("Arrival Tolerance (m)", min_value=0.01, max_value=2.0, step=0.01, value=0.1)
            if st.form_submit_button("SET TOLERANCE", use_container_width=True): hw.update_tolerance(float(new_tol))

        with st.form("pid_form"):
            p_val = st.number_input("P", step=0.1, value=DEFAULT_P)
            i_val = st.number_input("I", step=0.1, value=DEFAULT_I)
            d_val = st.number_input("D", step=0.1, value=DEFAULT_D)
            if st.form_submit_button("UPDATE GAINS", use_container_width=True): hw.update_pid(round(p_val,2), round(i_val,2), round(d_val,2))

        st.divider()
        st.header(":material/precision_manufacturing: Actuator Control")

        # Quick Presets
        c1, c2 = st.columns(2)
        with c1:
            if st.button("DIVE (0)", icon=":material/arrow_downward:", use_container_width=True):
                hw.move_actuator(0)
        with c2:
            if st.button("SURFACE (4095)", icon=":material/arrow_upward:", use_container_width=True):
                hw.move_actuator(4095)

        with st.form("actuator_form"):
            act_pos = st.number_input("Target Position (0-4095)", min_value=0, max_value=4095, value=DEFAULT_ACTUATOR_POS, step=100)
            if st.form_submit_button("MOVE TO CUSTOM", use_container_width=True): 
                hw.move_actuator(int(act_pos))

        with st.form("bounds_form"):
            st.write("**Set Limits**")
            b_min = st.number_input("Min ADC", min_value=0, max_value=4095, value=0)
            b_max = st.number_input("Max ADC", min_value=0, max_value=4095, value=4095)
            if st.form_submit_button("UPDATE BOUNDS", use_container_width=True): hw.update_bounds(int(b_min), int(b_max))

        with st.form("neutral_form"):
            st.write("**Buoyancy Baseline**")
            n_adc = st.number_input("Neutral ADC", min_value=0, max_value=4095, value=2048, step=50)
            if st.form_submit_button("SET NEUTRAL ADC", use_container_width=True): hw.update_neutral_adc(int(n_adc))

        st.divider()
        render_ota_section(hw)

def render_ota_section(hw):
    with st.container():
        st.header(":material/publish: OTA Reflash")
        
        col_load, col_upload = st.columns(2)
        with col_load:
            if st.button("Load Built Float Firmware", icon=":material/folder_open:", use_container_width=True, disabled=hw.reflash_in_progress, key="btn_load_fw"):
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
            uploaded_file = st.file_uploader("Or Upload Custom .bin", type=["bin"], label_visibility="collapsed", key="file_uploader_fw")
            if uploaded_file is not None:
                st.session_state["fw_data"] = uploaded_file.getvalue()
                st.session_state["fw_error"] = None
                crc = hw.calculate_crc32(st.session_state["fw_data"])
                st.session_state["fw_info"] = f"Uploaded File: {len(st.session_state['fw_data']):,} bytes | CRC: 0x{crc:08X}"

        if "fw_error" in st.session_state and st.session_state["fw_error"]:
            st.error(st.session_state["fw_error"])
            
        if "fw_data" in st.session_state and st.session_state["fw_data"] is not None:
            st.success(st.session_state["fw_info"])
            confirm_flash = st.checkbox("Confirm firmware flash to Float", value=False, key="chk_confirm_flash")
            if st.button("FLASH FIRMWARE", icon=":material/flash_on:", use_container_width=True, type="primary", disabled=(not confirm_flash or hw.reflash_in_progress), key="btn_flash_fw"):
                hw.reflash_firmware(st.session_state["fw_data"])
                st.session_state["fw_data"] = None
                st.session_state["fw_info"] = None
                # st.rerun() removed for stability

        if hw.reflash_in_progress:
            st.progress(hw.reflash_progress / 100.0, text=f"Flashing... {hw.reflash_progress}%")
            col_warn, col_cancel = st.columns([3, 1])
            with col_warn:
                st.warning("Do not disconnect during flash! Click Cancel to abort safely.")
            with col_cancel:
                if st.button("Cancel OTA", icon=":material/cancel:", use_container_width=True, type="secondary", key="btn_cancel_ota"):
                    hw.cancel_reflash()
                    # st.rerun() removed for stability

@st.fragment(run_every=REFRESH_RATE_S)
def render_dashboard_body(hw):
    # Top Row: Key Metrics
    render_metrics(hw)
    st.divider()

    # Middle Row: Chart & Quick Actions
    render_main_content(hw)
    st.divider()

    # Bottom Row: Serial Console
    render_console(hw)

def render_metrics(hw):
    with st.container():
        with hw.lock:
            data_points = len(hw.data_log)
            max_depth = 0.0
            if data_points > 0:
                # More efficient way to get max depth
                max_depth = max(p.get("Depth (m)", 0.0) for p in hw.data_log)
            latest_entry = hw.data_log[-1] if hw.data_log else {}

        time_left_str = "--"
        if hw.profile_start_time:
            elapsed = time.time() - hw.profile_start_time
            remaining = int(hw.active_duration - elapsed)
            if remaining > 0:
                time_left_str = f"{remaining}s"
            else:
                time_left_str = "DONE"
                hw.profile_start_time = None

        # First row: Mission Status and Countdown
        m1, m2, m3, m4 = st.columns(4)
        m1.metric("Mission State", hw.mission_status)
        m2.metric(":material/timer: Countdown", time_left_str)
        m3.metric(":material/height: Max Depth", f"{max_depth:.2f} m")
        m4.metric(":material/query_stats: Data Points", data_points)


def render_packet_log(hw):
    with st.container():
        with hw.lock:
            raw_packets = list(hw.packet_log)
        
        formatted_packets = []
        for pkt in reversed(raw_packets):
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
        container_style = (
            f'<div style="background-color: #070f1a; color: #00ffcc; font-family: \'Courier New\', Courier, monospace; '
            f'font-size: 14px; height: 400px; overflow-y: auto; padding: 12px; border: 1px solid #00ccff; border-radius: 5px; '
            f'line-height: 1.5; box-shadow: inset 0 0 10px rgba(0, 204, 255, 0.2); display: flex; flex-direction: column-reverse;">'
            f'{log_html}'
            f'</div>'
        )
        st.markdown(container_style, unsafe_allow_html=True)

def render_charts_and_visualizer(hw):
    with st.container():
        # Display live 2D pool animation
        render_pool_visualizer(hw)
        st.divider()
        
        with hw.lock:
            local_data = list(hw.data_log)
            
        # Always build a DataFrame to keep the Plotly components mounted
        if local_data:
            df = pd.DataFrame(local_data)
            # Downsample if too many points to keep UI snappy
            if len(df) > 300:
                df = df.iloc[::max(1, len(df)//300)]
        else:
            df = pd.DataFrame(columns=["Time (s)", "Depth (m)", "Pressure (kPa)", "Actuator (ADC)", "Target (ADC)"])
            
        if "Time (s)" in df.columns and "Depth (m)" in df.columns:
            hover_cols = [c for c in ["Depth (m)", "Pressure (kPa)", "Actuator (ADC)", "Target (ADC)"] if c in df.columns]
            fig = px.scatter(df, x="Time (s)", y="Depth (m)", hover_data=hover_cols, height=400)
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
                    fig2 = px.scatter(df, x="Time (s)", y="Actuator (ADC)", hover_data=hover_cols_act, height=250)
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
            
            if not local_data:
                st.caption("ℹ️ Waiting for profile telemetry... Start a profile to see real-time data points.")
            else:
                st.caption("📊 Live telemetry active.")
        else:
            st.error(f"Telemetry data keys mismatch. Columns: {df.columns.tolist()}")
            
        st.markdown("### Active Configuration")
        col_cfg1, col_cfg2, col_cfg3, col_cfg4 = st.columns(4)
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
        with col_cfg4:
            st.markdown("**Live Status**")
            st.markdown(f"**Depth:** `{hw.float_settings.get('LiveDepth', '--')} m`")
            st.markdown(f"**ADC:** `{hw.float_settings.get('ADC', '--')}`")

def render_main_content(hw):
    st.markdown("""
    <style>
    div[data-testid="stButton"] button, 
    div[data-testid="stDownloadButton"] button {
        white-space: nowrap !important;
    }
    </style>
    """, unsafe_allow_html=True)

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
            
            # Use a stable widget type and key to prevent fragment mutation crashes
            df_csv = pd.DataFrame(local_data).to_csv(index=False).encode('utf-8') if local_data else b""
            st.download_button(
                "DOWNLOAD CSV", 
                icon=":material/download:", 
                data=df_csv, 
                file_name="mate_profile.csv", 
                mime="text/csv", 
                use_container_width=True, 
                key="btn_download_csv",
                disabled=not local_data
            )

    with right_col:
        st.subheader(":material/show_chart: Right Panel: Depth vs Time Chart")
        st.caption("Clean digital line graph of under-ice profile depths (No secondary Y-axis)")
        
        render_charts_and_visualizer(hw)

def render_log_view(hw, search_query, log_type_filter):
    with st.container():
        with hw.lock:
            raw_logs = list(hw.console_log)
            
        # 2. Filter logic
        filtered_lines = []
        for line in raw_logs:
            # Classify the log line
            is_float = "[Float USB]" in line or "[HIL Target]" in line
            is_error = "🔴" in line or "[ERROR]" in line or "error" in line.lower() or "critical" in line.lower() or "failed" in line.lower()
            is_tx = "🔵" in line or "[TX]" in line or "sent:" in line.lower()
            
            # Check source filters
            show_line = False
            if is_float and "Float USB Logs" in log_type_filter:
                show_line = True
            elif is_error and "Errors / Warnings" in log_type_filter:
                show_line = True
            elif is_tx and "Commands (TX)" in log_type_filter:
                show_line = True
            elif not is_float and not is_error and not is_tx and "Surface Logs" in log_type_filter:
                show_line = True
                
            # Check search query
            if show_line and search_query:
                if search_query.lower() not in line.lower():
                    show_line = False
                    
            if show_line:
                filtered_lines.append(line)
                
        # 3. Render colorized log window
        # Note: flex-direction: column-reverse breaks normal copy-paste flow.
        # Using standard column with autoscroll script instead.
        formatted_lines = []
        for line in filtered_lines:
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
            formatted_lines.append(f'<div style="color: {color}; margin-bottom: 2px; white-space: pre-wrap; word-break: break-all;">{escaped}</div>')
            
        log_html = "".join(formatted_lines)
        
        # Static ID to prevent script collisions and JS errors on re-renders
        log_id = "log_container_main"
        
        container_style = (
            f'<div id="{log_id}" style="background-color: #0e1117; color: #d4d4d4; font-family: \'Courier New\', Courier, monospace; '
            f'font-size: 13px; height: 300px; overflow-y: auto; padding: 10px; border: 1px solid #333; border-radius: 5px; '
            f'line-height: 1.4; display: flex; flex-direction: column;">'
            f'{log_html}'
            f'</div>'
            f'<script>'
            f'    var el = document.getElementById("{log_id}");'
            f'    if (el) el.scrollTop = el.scrollHeight;'
            f'</script>'
        )
        st.markdown(container_style, unsafe_allow_html=True)
        
        # 4. Copy helper: Expandable raw log block
        if filtered_lines:
            with st.expander("📋 Copy Raw Log Text", expanded=False):
                st.caption("Standard plain text format for easy copy-pasting.")
                # Join with standard newlines for plain text copy
                st.code("\n".join(filtered_lines), language="text")

def render_console(hw):
    st.subheader(":material/developer_board: System Debug Serial Console")
    
    # 1. Log Filtering & Search UI
    col_f1, col_f2 = st.columns([2, 1])
    with col_f1:
        search_query = st.text_input(
            "Search Logs / Filter by keyword", 
            "", 
            placeholder="Search logs... (e.g. [ERROR], test, sync)",
            key="console_search_input"
        )
    with col_f2:
        log_type_filter = st.multiselect(
            "Source Filter",
            options=["Surface Logs", "Float USB Logs", "Errors / Warnings", "Commands (TX)"],
            default=["Surface Logs", "Float USB Logs", "Errors / Warnings", "Commands (TX)"],
            key="console_source_multiselect"
        )
        
    # Render dynamic log view fragment
    render_log_view(hw, search_query, log_type_filter)


def render_pool_visualizer(hw):
    import html
    # Retrieve parameters safely
    try:
        live_depth = float(hw.float_settings.get("LiveDepth", 0.0))
    except (ValueError, TypeError):
        live_depth = 0.0
        
    try:
        actuator_adc = int(float(hw.float_settings.get("ADC", 2048)))
    except (ValueError, TypeError):
        actuator_adc = 2048
        
    try:
        deep_target = float(hw.float_settings.get("Deep", 2.5))
    except (ValueError, TypeError):
        deep_target = 2.5
        
    try:
        shallow_target = float(hw.float_settings.get("Shallow", 0.4))
    except (ValueError, TypeError):
        shallow_target = 0.4
        
    try:
        act_min = int(float(hw.float_settings.get("ActMin", 126)))
        act_max = int(float(hw.float_settings.get("ActMax", 3900)))
    except (ValueError, TypeError):
        act_min = 126
        act_max = 3900
        
    # Get FSM State and Stage
    try:
        state_val = int(hw.float_settings.get("State", 0))
    except (ValueError, TypeError):
        state_val = 0
        
    try:
        stage_val = int(hw.float_settings.get("Stage", 0))
    except (ValueError, TypeError):
        stage_val = 0
        
    # Determine target depth
    target_depth = 0.0
    if state_val == 2:  # PROFILING
        if stage_val == 0:
            target_depth = deep_target
        elif stage_val == 1:
            target_depth = shallow_target
            
    # Max pool depth (15 feet = 4.572m)
    max_pool_depth = 4.572
    
    # Calculate Y positions for SVG (height is 200px, y goes from 30 to 230)
    pool_height_px = 200
    y_min_px = 30
    
    # Scale depth
    float_center_y = y_min_px + (live_depth / max_pool_depth) * pool_height_px
    # Clamp to keep inside SVG bounds
    float_center_y = max(y_min_px, min(float_center_y, y_min_px + pool_height_px))
    float_y = float_center_y - 25  # Center the 50px height cylinder
    
    # Scale actuator to syringe extension (max 20px)
    adc_range = max(act_max - act_min, 1)
    extension_ratio = max(0.0, min(1.0, (actuator_adc - act_min) / adc_range))
    syringe_len_px = 5 + extension_ratio * 18
    syringe_circle_cy = 50 + syringe_len_px
    
    # Map state to LED color
    # FloatState_t: IDLE=0, PRE_DIVE=1, PROFILING=2, PROFILE_DONE=3, DUMPING_DATA=4, TEST_CALIBRATE=5
    led_color = "#475569"  # Slate for IDLE
    state_name = "IDLE"
    if state_val == 1:
        led_color = "#eab308"  # Yellow for PRE_DIVE
        state_name = "PRE_DIVE"
    elif state_val == 2:
        state_name = "PROFILING"
        if stage_val == 0:
            led_color = "#06b6d4"  # Cyan for DEEP stage
            state_name = "PROFILING (DEEP)"
        elif stage_val == 1:
            led_color = "#f97316"  # Orange for SHALLOW stage
            state_name = "PROFILING (SHALLOW)"
        elif stage_val == 2:
            led_color = "#ec4899"  # Pink for EXITING stage
            state_name = "PROFILING (EXITING)"
    elif state_val == 3:
        led_color = "#22c55e"  # Green for DONE
        state_name = "PROFILE DONE"
    elif state_val == 4:
        led_color = "#a855f7"  # Purple for DATA DUMP
        state_name = "DUMPING DATA"
    elif state_val == 5:
        led_color = "#3b82f6"  # Blue for TEST/CALIBRATE
        state_name = "TEST_CALIBRATE"
        
    # Generate Target Line SVG
    target_line_svg = ""
    if target_depth > 0:
        target_y = y_min_px + (target_depth / max_pool_depth) * pool_height_px
        target_line_svg = f"""
        <line x1="50" y1="{target_y}" x2="350" y2="{target_y}" stroke="#ef4444" stroke-width="2" stroke-dasharray="4"/>
        <text x="355" y="{target_y + 3}" fill="#ef4444" font-size="10" font-family="monospace">Target: {target_depth:.1f}m</text>
        """
        
    # SVG Drawing
    svg_html = f"""
    <div style="background-color: #070f1a; border: 1px solid #00ccff; border-radius: 5px; padding: 12px; font-family: monospace; color: #00ffcc; box-shadow: 0 0 10px rgba(0, 204, 255, 0.1);">
        <h4 style="margin: 0 0 10px 0; text-align: center; color: #00ffcc; font-size: 14px;">2D POOL SIMULATION VIEW (15ft Deep)</h4>
        <svg width="100%" height="250" viewBox="0 0 440 250" style="background-color: #0c1829; border-radius: 3px;">
            <!-- Water Body -->
            <rect x="50" y="30" width="300" height="200" fill="rgba(0, 204, 255, 0.12)" stroke="#0088cc" stroke-width="1.5"/>
            
            <!-- Grid lines -->
            <line x1="50" y1="30" x2="350" y2="30" stroke="#0088cc" stroke-dasharray="3,3" opacity="0.6"/>
            <line x1="50" y1="74.4" x2="350" y2="74.4" stroke="#0088cc" stroke-dasharray="3,3" opacity="0.3"/>
            <line x1="50" y1="118.8" x2="350" y2="118.8" stroke="#0088cc" stroke-dasharray="3,3" opacity="0.3"/>
            <line x1="50" y1="163.2" x2="350" y2="163.2" stroke="#0088cc" stroke-dasharray="3,3" opacity="0.3"/>
            <line x1="50" y1="207.6" x2="350" y2="207.6" stroke="#0088cc" stroke-dasharray="3,3" opacity="0.3"/>
            <line x1="50" y1="230" x2="350" y2="230" stroke="#0088cc" stroke-dasharray="3,3" opacity="0.6"/>
            
            <!-- Labels -->
            <text x="10" y="33" fill="#00aa88" font-size="10">0.0m</text>
            <text x="10" y="122" fill="#00aa88" font-size="10">2.0m</text>
            <text x="10" y="211" fill="#00aa88" font-size="10">4.0m</text>
            <text x="10" y="233" fill="#00aa88" font-size="10">4.5m</text>
            
            <!-- Target Line if any -->
            {target_line_svg}
            
            <!-- Float Cylinder -->
            <g transform="translate(180, {float_y})">
                <!-- Outer enclosure -->
                <rect x="0" y="0" width="30" height="50" rx="6" fill="#1e293b" stroke="#00ffcc" stroke-width="2" style="transition: transform 0.1s;"/>
                <!-- End caps -->
                <rect x="3" y="-3" width="24" height="4" fill="#0f172a" stroke="#00ffcc" stroke-width="1"/>
                <!-- Piston/plunger extension (representing actuator ADC) -->
                <rect x="12" y="50" width="6" height="{syringe_len_px}" fill="#ffaa00" stroke="#ffaa00" stroke-width="1"/>
                <circle cx="15" cy="{syringe_circle_cy}" r="3" fill="#ffaa00"/>
                <!-- Status LED light -->
                <circle cx="15" cy="25" r="5" fill="{led_color}" stroke="#ffffff" stroke-width="1"/>
            </g>
        </svg>
        <div style="margin-top: 8px; font-size: 11px; text-align: center; line-height: 1.5; color: #a1a1aa;">
            Live: <span style="color: #00ffcc; font-weight: bold;">{live_depth:.3f} m</span> | 
            Target: <span style="color: #ef4444; font-weight: bold;">{target_depth:.1f} m</span> | 
            Actuator: <span style="color: #ffaa00; font-weight: bold;">{actuator_adc} ADC</span> |
            State: <span style="color: #ffffff; font-weight: bold; background: #1e293b; padding: 2px 5px; border-radius: 3px;">{state_name}</span>
        </div>
    </div>
    """
    clean_html = "\n".join(line.strip() for line in svg_html.split("\n") if line.strip())
    st.markdown(clean_html, unsafe_allow_html=True)

