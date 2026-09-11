import streamlit as st
from modules.hardware import HardwareManager
from modules.ui_elements import render_sidebar, render_console
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
st.markdown("""
    <style>
    /* Remove top padding from main content area */
    .block-container {
        padding-top: 2rem !important;
        padding-bottom: 0rem !important;
    }
    /* Make the title header more compact */
    header {
        visibility: hidden;
    }
    #MainMenu {
        visibility: hidden;
    }
    footer {
        visibility: hidden;
    }
    /* Sticky header: metrics + buttons stay visible on scroll */
    [data-testid="stVerticalBlock"]:has(.sticky-anchor) {
        position: sticky !important;
        top: 0px !important;
        z-index: 999 !important;
        background: #0e1117 !important;
        padding-bottom: 0.25rem !important;
        border-bottom: 2px solid #1e293b !important;
    }
    .sticky-anchor {
        display: none;
    }
    </style>
""", unsafe_allow_html=True)

st.title(f"{PAGE_ICON} MATE Floats 2026: {PAGE_TITLE}")

# Render Sidebar (Connection & Settings) and get selected view
view = render_sidebar(hw)

# Render selected view
if view == "Mission Dashboard":
    from modules.ui_elements import render_mission_dashboard_fragment
    render_mission_dashboard_fragment(hw)
elif view == "System Debug Logs":
    render_console(hw)
elif view == "Buoyancy Calculator & Simulator Config":
    from modules.ui_elements import render_buoyancy_calculator
    render_buoyancy_calculator(hw)
elif view == "PID Tuner & Profile Analyzer":
    from modules.ui_elements import render_pid_analyzer
    render_pid_analyzer(hw)
