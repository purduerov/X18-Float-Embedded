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

# Render Unified Dashboard (auto-updates via internal fragment)
from modules.ui_elements import render_dashboard_body
render_dashboard_body(hw)
