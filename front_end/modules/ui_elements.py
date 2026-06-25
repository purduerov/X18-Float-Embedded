import streamlit as st
import pandas as pd
import plotly.express as px
import html
import time
from modules.constants import *

def render_sidebar(hw):
    st.markdown("""
        <style>
        [data-testid="stSidebar"] [data-testid="stVerticalBlock"] {
            gap: 0.2rem !important;
            padding-top: 1rem !important;
        }
        [data-testid="stSidebar"] .stButton > button {
            margin-bottom: 0px !important;
        }
        [data-testid="stSidebar"] .stDivider {
            margin-top: 0.5rem !important;
            margin-bottom: 0.5rem !important;
        }
        [data-testid="stSidebar"] h1, [data-testid="stSidebar"] h2, [data-testid="stSidebar"] h3, [data-testid="stSidebar"] h4 {
            margin-top: 0px !important;
            margin-bottom: 0.2rem !important;
            padding-top: 0px !important;
        }
        </style>
    """, unsafe_allow_html=True)
    with st.sidebar:
        st.header(":material/navigation: Navigation")
        view = st.radio(
            "View", 
            ["Mission Dashboard", "System Debug Logs", "Buoyancy Calculator & Simulator Config"], 
            label_visibility="collapsed"
        )
        st.divider()
        st.header(":material/power: Connection")
        available_ports = hw.get_available_ports()
        
        # Always show a dropdown, but handle the empty case gracefully
        if not available_ports:
            st.selectbox("COM Port", ["No Ports Found"], disabled=True, key="empty_port_sel")
            selected_port = None
        else:
            selected_port_obj = st.selectbox(
                "COM Port", 
                available_ports, 
                format_func=lambda x: f"{x.device} - {x.description}"
            )
            selected_port = selected_port_obj.device
            
        col1, col2, col3 = st.columns([1, 1, 1])
        with col1:
            if st.button("Connect", width="stretch", type="primary", disabled=(selected_port is None)):
                hw.connect(selected_port)
        with col2:
            if st.button("Disconnect", width="stretch"): hw.disconnect()
        with col3:
            if st.button("Refresh", icon=":material/refresh:", width="stretch", help="Scan for new COM ports"):
                st.rerun()
                
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
                if st.button("Connect HIL", width="stretch", type="secondary"): 
                    hw.connect_hil(selected_hil_port)
            with col_hil2:
                if st.button("Disconnect HIL", width="stretch"): 
                    hw.disconnect_hil()
                    
            hil_status_txt = ":green[Connected]" if hw.hil_ser and hw.hil_ser.is_open else ":red[Disconnected]"
            st.write(f"**HIL Link:** {hil_status_txt}")
            
        st.divider()
        
        st.header(":material/settings: Float Settings")
        
        if st.button("ZERO DEPTH", icon=":material/straighten:", width="stretch", type="secondary"):
            hw.zero_depth()

        if st.button("TEST / CALIBRATE MODE", icon=":material/biotech:", width="stretch", type="secondary"):
            hw.test_mode()

        st.markdown("---")
        st.write("**Quick Update**")
        with st.form("unified_settings_form"):
            param_label = st.selectbox("Parameter", [
                "Manual Move (ADC)", "Team ID", "Duration (s)", 
                "Deep Target (m)", "Shallow Target (m)", 
                "Number of Profiles", "Arrival Tolerance (m)", 
                "Neutral ADC", "Min ADC Limit", "Max ADC Limit"
            ])
            
            # Map labels to keys to get current values
            label_to_key = {
                "Manual Move (ADC)": "ADC",
                "Team ID": "Co#",
                "Duration (s)": "Time",
                "Deep Target (m)": "Deep",
                "Shallow Target (m)": "Shallow",
                "Number of Profiles": "N",
                "Arrival Tolerance (m)": "Tol",
                "Neutral ADC": "Neutral",
                "Min ADC Limit": "ActMin",
                "Max ADC Limit": "ActMax"
            }
            key = label_to_key[param_label]
            curr_val_str = hw.float_settings.get(key, "0")
            try:
                curr_val = float(curr_val_str)
            except (ValueError, TypeError):
                curr_val = 0.0
            
            step = 1.0
            if "(m)" in param_label: step = 0.1
            if "Tolerance" in param_label: step = 0.01
            if "ADC" in param_label: step = 50.0
            
            new_val = st.number_input("New Value", value=curr_val, step=step)
            
            if st.form_submit_button("SEND UPDATE / MOVE", width="stretch"):
                if param_label == "Manual Move (ADC)": hw.move_actuator(int(new_val))
                elif param_label == "Team ID": hw.update_team_id(int(new_val))
                elif param_label == "Duration (s)": hw.update_duration(int(new_val))
                elif param_label == "Deep Target (m)": hw.update_deep_target(float(new_val))
                elif param_label == "Shallow Target (m)": hw.update_shallow_target(float(new_val))
                elif param_label == "Number of Profiles": hw.update_num_profiles(int(new_val))
                elif param_label == "Arrival Tolerance (m)": hw.update_tolerance(float(new_val))
                elif param_label == "Neutral ADC": hw.update_neutral_adc(int(new_val))
                elif param_label == "Min ADC Limit":
                    try:
                        max_a = int(float(hw.float_settings.get("ActMax", 4095)))
                    except: max_a = 4095
                    hw.update_bounds(int(new_val), max_a)
                elif param_label == "Max ADC Limit":
                    try:
                        min_a = int(float(hw.float_settings.get("ActMin", 0)))
                    except: min_a = 0
                    hw.update_bounds(min_a, int(new_val))

        st.divider()
        st.header(":material/precision_manufacturing: Manual Overrides")

        # Quick Presets
        c1, c2 = st.columns(2)
        with c1:
            if st.button("DIVE (0)", icon=":material/arrow_downward:", width="stretch"):
                hw.move_actuator(0)
        with c2:
            if st.button("SURFACE (4095)", icon=":material/arrow_upward:", width="stretch"):
                hw.move_actuator(4095)

        with st.form("pid_form"):
            st.write("**Buoyancy Controller (PID)**")
            p_val = st.number_input("P (Lead)", step=0.1, value=DEFAULT_P)
            i_val = st.number_input("I (Effort)", step=1.0, value=DEFAULT_I)
            d_val = st.number_input("D (Deadband)", step=0.01, value=DEFAULT_D)
            if st.form_submit_button("UPDATE GAINS", width="stretch"): hw.update_pid(round(p_val,2), round(i_val,2), round(d_val,2))

        st.divider()
        st.header(":material/system_update_alt: OTA & Firmware")
        
        # New automated update workflows
        with st.expander("🛠️ Firmware Utilities", expanded=False):
            st.write("**1. Generate bsdiff Patch (OTA)**")
            c_patch1, c_patch2 = st.columns(2)
            with c_patch1:
                if st.button("Gen Patch", width="stretch", help="Run tools/ota_patch_gen.py"):
                    import subprocess
                    import os
                    proj_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
                    old_bin = os.path.join(proj_dir, ".pio", "build", "float", "firmware_old.bin")
                    new_bin = os.path.join(proj_dir, ".pio", "build", "float", "firmware.bin")
                    patch_bin = os.path.join(proj_dir, ".pio", "build", "float", "patch.bin")
                    # Fallback to compare against itself if no old binary exists for UI demo purposes
                    if not os.path.exists(old_bin) and os.path.exists(new_bin):
                        old_bin = new_bin 
                    try:
                        subprocess.run(["python", os.path.join(proj_dir, "tools", "ota_patch_gen.py"), old_bin, new_bin, patch_bin], check=True)
                        st.toast("Patch generated successfully!")
                    except Exception as e:
                        st.error(f"Patch Gen Error: {e}")
            with c_patch2:
                if st.button("Send OTA", width="stretch", help="Send patch via LoRa", type="primary"):
                    st.toast("Sending Patch OTA...")
            
            st.write("**2. Wired Dual-Flash (USB)**")
            if st.button("Flash Bootloader + App", width="stretch", icon=":material/usb:"):
                import subprocess
                import os
                proj_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
                try:
                    # Assumes pio is in path or we are running in the pio env
                    subprocess.run(["pio", "run", "-t", "upload_dual"], cwd=proj_dir, check=True)
                    st.toast("Wired Flash Complete!")
                except Exception as e:
                    st.error("Wired flash failed. Check console.")

        # Legacy direct file OTA
        render_ota_section(hw)

    return view

def render_ota_section(hw):
    with st.container():
        st.write("**Direct Flash Upload (.bin)**")
        
        if st.button("Load Built Float Firmware", icon=":material/folder_open:", width="stretch", disabled=hw.reflash_in_progress, key="btn_load_fw"):
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

        if "fw_error" in st.session_state and st.session_state["fw_error"]:
            st.error(st.session_state["fw_error"])
            
        if "fw_data" in st.session_state and st.session_state["fw_data"] is not None:
            st.success(st.session_state["fw_info"])
            confirm_flash = st.checkbox("Confirm firmware flash to Float", value=False, key="chk_confirm_flash")
            if st.button("FLASH FIRMWARE", icon=":material/flash_on:", width="stretch", type="primary", disabled=(not confirm_flash or hw.reflash_in_progress), key="btn_flash_fw"):
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
                if st.button("Cancel OTA", icon=":material/cancel:", width="stretch", type="secondary", key="btn_cancel_ota"):
                    hw.cancel_reflash()
                    # st.rerun() removed for stability

@st.fragment(run_every=REFRESH_RATE_S)
def render_mission_dashboard_fragment(hw):
    # Top Row: Compact Key Metrics
    render_metrics(hw)
    
    # Action Bar: Directly under Active Config
    with st.container():
        c1, c2, c3 = st.columns(3)
        with c1:
            st.button("BEGIN PROFILE", icon=":material/play_arrow:", width="stretch", type="primary", key="btn_begin_profile", on_click=lambda: hw.start_profile())
        with c2:
            st.button("SYNC FROM FLOAT", icon=":material/sync:", width="stretch", key="btn_sync_settings", on_click=lambda: hw.send_command('?'))
        with c3:
            if st.button("RESET FSM", icon=":material/warning:", width="stretch", key="btn_reset_bar", on_click=lambda: hw.reset_fsm()):
                pass

    # Two distinct halves
    left_col, right_col = st.columns([1, 2], gap="medium")
    
    with left_col:
        st.subheader("Telemetry Feed")
        packet_container = st.container()
        with packet_container:
            render_packet_log(hw)

    with right_col:
        st.subheader("Depth vs Time")
        chart_container = st.container()
        with chart_container:
            render_charts(hw)

def render_metrics(hw):
    with hw.lock:
        settings = dict(hw.float_settings)
        mission_status = hw.mission_status
    with st.container():
        # Replace metrics with compact configuration summary
        m1, m2, m3, m4 = st.columns(4)
        with m1:
            st.markdown(f"**State:** `{mission_status}`")
            st.markdown(f"**FW:** `v{settings.get('FW', '--')}` | **ID:** `{settings.get('Co#', '--')}`")
        with m2:
            st.markdown(f"**Target:** `{settings.get('Deep', '--')}m` / `{settings.get('Shallow', '--')}m`")
            st.markdown(f"**PID:** `{settings.get('P', '--')}/{settings.get('I', '--')}/{settings.get('D', '--')}`")
        with m3:
            st.markdown(f"**ADC:** `{settings.get('ActMin', '--')}-{settings.get('ActMax', '--')}`")
            st.markdown(f"**Neutral:** `{settings.get('Neutral', '--')}`")
        with m4:
            st.markdown(f"**Live:** `{settings.get('LiveDepth', '--')}m`")
            st.markdown(f"**Pos:** `{settings.get('ADC', '--')} ADC`")


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

def render_charts(hw):
    with st.container():
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
            fig = px.scatter(df, x="Time (s)", y="Depth (m)", hover_data=hover_cols, height=350)
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
            st.plotly_chart(fig, key="p_depth_chart", width="stretch")
            
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
                    st.plotly_chart(fig2, key="p_act_chart", width="stretch")
            
            if not local_data:
                st.caption("ℹ️ Waiting for profile telemetry... Start a profile to see real-time data points.")
        else:
            st.error(f"Telemetry data keys mismatch. Columns: {df.columns.tolist()}")



@st.fragment(run_every=2.0)
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
        # Reverse the lines in Python so that newest is at the bottom with column-reverse flex layout.
        # This achieves autoscroll naturally without introducing memory/WebSocket breaking inline scripts.
        formatted_lines = []
        for line in reversed(filtered_lines):
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
            f'line-height: 1.4; display: flex; flex-direction: column-reverse;">'
            f'{log_html}'
            f'</div>'
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
    
    with st.form("console_filter_form"):
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
        st.form_submit_button("Apply Filters")
        
    # Render dynamic log view fragment
    render_log_view(hw, search_query, log_type_filter)

def render_buoyancy_calculator(hw):
    import math
    st.subheader("⚖️ Buoyancy Calculator & HIL Simulator Configurator")
    st.markdown("""
        This tool helps calculate the target physical weight (ballast) required for the float to achieve neutral buoyancy at the midpoint of the actuator's range. It also configures the pool physics parameters for the **Hardware-in-the-Loop (HIL) Simulator**.
    """)
    
    # Pre-populate settings from the simulator instance
    sim = hw.simulator
    
    col1, col2 = st.columns([1, 1], gap="large")
    
    with col1:
        st.markdown("### 🛠️ Physical Parameters (Fixed)")
        
        # Fixed physical specifications of the MATE float
        c_dia = 4.5
        c_len = 12.0
        c_add = 19.311
        c_syr = 90.0

        st.markdown(f"""
        *   **Cylinder Outer Diameter:** `{c_dia:.1f} inches`
        *   **Cylinder Length:** `{c_len:.1f} inches`
        *   **Additional Hull Volume:** `{c_add:.3f} in³` *(end caps, sensor housings, etc.)*
        *   **Maximum Syringe Volume:** `{c_syr:.1f} mL`
        """)
        
        st.divider()
        st.markdown("### 🌡️ Environment & State")
        c_temp = st.slider("Water Temperature (°C)", min_value=0.0, max_value=40.0, value=float(sim.temp_c), step=0.5)
        
        # Mass
        c_mass = st.number_input("Actual Float Mass / Weight (grams)", min_value=100.0, max_value=10000.0, value=float(round(sim.mass * 1000.0, 1)), step=1.0, format="%.1f")
        
        st.divider()
        st.markdown("### 🎛️ Actuator Limits & Calibration")
        c_min_adc = st.number_input("Actuator Min ADC (Retracted)", min_value=0, max_value=4095, value=int(sim.act_min), step=10)
        c_max_adc = st.number_input("Actuator Max ADC (Extended)", min_value=0, max_value=4095, value=int(sim.act_max), step=10)
        c_neu_adc = st.number_input("Calibrated Neutral ADC (Settings)", min_value=0, max_value=4095, value=int(sim.neutral_adc), step=10)
        
        # Mode
        c_real = st.toggle("Enable Realistic Physical Simulation", value=bool(sim.realistic_physics), help="If enabled, the simulator uses the fixed physical volume computed from dimensions. If disabled, it adapts the volume to match the calibrated Neutral ADC.")

    # Calculations
    # 1. Pure water density UNESCO EOS-80
    T = c_temp
    rho_w = (999.842594 + 6.793952e-2 * T - 9.095290e-3 * T**2 + 
             1.001685e-4 * T**3 - 1.120083e-6 * T**4 + 6.536332e-9 * T**5)
    
    # 2. Volumes
    dia_m = c_dia * 0.0254
    len_m = c_len * 0.0254
    v_cylinder_m3 = math.pi * ((dia_m / 2.0) ** 2) * len_m
    v_cylinder_ml = v_cylinder_m3 * 1e6
    
    v_add_ml = c_add * 16.387064
    v_add_m3 = v_add_ml / 1e6
    
    v_hull_ml = v_cylinder_ml + v_add_ml
    v_hull_m3 = v_hull_ml / 1e6
    
    v_syringe_max_ml = c_syr
    v_syringe_max_m3 = v_syringe_max_ml / 1e6
    
    # 3. Targets (rho_w is kg/m^3, so multiply by volume in m^3 to get kg, then 1000 for g)
    m_midpoint = rho_w * (v_hull_m3 + 0.5 * v_syringe_max_m3) * 1000.0
    m_min = rho_w * v_hull_m3 * 1000.0
    m_max = rho_w * (v_hull_m3 + v_syringe_max_m3) * 1000.0
    
    with col2:
        st.markdown("### 📊 Calculated Buoyancy Analysis")
        
        # Density card
        st.info(f"**Water Density at {c_temp:.1f}°C:** `{rho_w:.4f} kg/m³` (or `{rho_w/1000.0:.6f} g/mL`)")
        
        # Volumes breakdown
        st.markdown("#### 📏 Volume Breakdown")
        vol_df = pd.DataFrame({
            "Component": ["Main Cylinder Hull", "Additional External Hull", "Total Dry Hull Volume", "Syringe Range"],
            "Volume (mL)": [f"{v_cylinder_ml:.1f}", f"{v_add_ml:.1f}", f"{v_hull_ml:.1f}", f"0.0 to {v_syringe_max_ml:.1f}"]
        })
        st.table(vol_df)
        
        # Ballast Target Weights
        st.markdown("#### 🎯 Target Ballast Weights for Pool (Fresh Water)")
        
        metric_col1, metric_col2, metric_col3 = st.columns(3)
        with metric_col1:
            st.metric(
                label="Min Weight (Retracted, 0 mL)",
                value=f"{m_min:.1f} g",
                help="If the float is lighter than this, it will permanently float and can never dive."
            )
        with metric_col2:
            st.metric(
                label="Midpoint Target (50%, 45/90 mL)",
                value=f"{m_midpoint:.1f} g",
                help="RECOMMENDED WEIGHT. Achieves neutral buoyancy when the syringe is exactly in the middle of its stroke."
            )
        with metric_col3:
            st.metric(
                label="Max Weight (Extended, 90 mL)",
                value=f"{m_max:.1f} g",
                help="If the float is heavier than this, it will permanently sink and can never rise."
            )
            
        st.divider()
        st.markdown("### 🔍 Current Float Status")
        
        # Analyze current mass
        if c_mass > m_max:
            deficit = c_mass - m_max
            st.error(f"❌ **TOO HEAVY:** The float is `{c_mass:.1f} g` but max buoyancy limit is `{m_max:.1f} g`. "
                     f"It will sink to the bottom and will NOT be able to ascend even with the syringe fully extended. "
                     f"**Action:** Remove at least `{deficit:.1f} grams` of weight / ballast.")
        elif c_mass < m_min:
            surplus = m_min - c_mass
            st.error(f"❌ **TOO LIGHT:** The float is `{c_mass:.1f} g` but minimum diving limit is `{m_min:.1f} g`. "
                     f"It will remain floating on the surface and will NOT be able to dive even with the syringe fully retracted. "
                     f"**Action:** Add at least `{surplus:.1f} grams` of weight / ballast.")
        else:
            # Valid range!
            st.success("✔️ **VALID OPERATIONAL RANGE:** The float is inside the operational buoyancy bounds. It can actively dive and ascend.")
            
            # Calculate target syringe volume for hover
            req_syr_vol_m3 = (c_mass / 1000.0) / rho_w - v_hull_m3
            req_syr_vol_ml = req_syr_vol_m3 * 1e6
            req_ratio = req_syr_vol_ml / v_syringe_max_ml
            
            expected_adc = c_min_adc + req_ratio * (c_max_adc - c_min_adc)
            expected_adc = max(c_min_adc, min(c_max_adc, int(expected_adc)))
            
            st.write(f"**Required Syringe Volume to Hover:** `{req_syr_vol_ml:.2f} mL` "
                     f"({req_ratio*100.0:.1f}% extension)")
            st.write(f"**Expected Neutral Buoyancy ADC:** `{expected_adc}`")
            
            # Calibration mismatch warning
            adc_diff = abs(c_neu_adc - expected_adc)
            if adc_diff > 200:
                st.warning(f"⚠️ **Calibration Mismatch:** Your current settings expect neutral buoyancy at ADC `{c_neu_adc}`, "
                           f"but physics indicates it will actually hover around ADC `{expected_adc}` (difference of {adc_diff} counts). "
                           f"If you use Realistic Simulation, the float will drift until the Adaptive Learning updates the neutral point. "
                           f"**Recommendation:** Update the 'Neutral' setting on the Pico to `{expected_adc}`.")
                           
        st.divider()
        if st.button("💾 Apply Configuration to HIL Simulator", type="primary", use_container_width=True):
            hw.update_simulator_physical_params(
                mass_g=c_mass,
                diameter_in=c_dia,
                length_in=c_len,
                additional_volume_in3=c_add,
                syringe_ml=c_syr,
                temp_c=c_temp,
                realistic_physics=c_real
            )
            # Sync min/max/neutral to simulator's internal calibration as well
            hw.simulator.set_calibration(c_neu_adc, c_min_adc, c_max_adc)
            st.success("Successfully synchronized and reset HIL Simulator with new physics parameters!")
            time.sleep(1.0)
            st.rerun()




