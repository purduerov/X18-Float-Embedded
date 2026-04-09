import streamlit as st
from modules.hardware import HardwareManager
from modules.ui_elements import render_sidebar, render_metrics, render_main_content, render_console
from modules.constants import PAGE_TITLE, PAGE_ICON, REFRESH_RATE_S

# -----------------------------------------
# 1. INITIALIZATION
# -----------------------------------------
st.set_page_config(page_title=PAGE_TITLE, layout="wide", page_icon=PAGE_ICON)

@st.cache_resource
def get_hardware():
    return HardwareManager()

hw = get_hardware()

# -----------------------------------------
# 2. UI LAYOUT & DASHBOARD
# -----------------------------------------
st.title(f"{PAGE_ICON} MATE Floats 2026: {PAGE_TITLE}")

# Render Sidebar (Connection & Settings)
render_sidebar(hw)

# Main Dashboard Area (Auto-refreshes every 0.25 seconds)
@st.fragment(run_every=f"{REFRESH_RATE_S}s")
def live_dashboard():
    # Top Row: Key Metrics
    render_metrics(hw)
    st.divider()

    # Middle Row: Chart & Quick Actions
    render_main_content(hw)
    st.divider()

    # Bottom Row: Serial Console
    render_console(hw)

# Launch the live dashboard
live_dashboard()
