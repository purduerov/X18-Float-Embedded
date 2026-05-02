# Default Hardware/Mission Constants
DEFAULT_TEAM_ID = 18
DEFAULT_DURATION_S = 30
DEFAULT_DEEP_TARGET = 2.5
DEFAULT_SHALLOW_TARGET = 0.4
DEFAULT_NUM_PROFILES = 2
DEFAULT_P = 1200.0
DEFAULT_I = 150.0
DEFAULT_D = 600.0
DEFAULT_ACTUATOR_POS = 4095

# UI/UX Constants
PAGE_TITLE = "Mission Control"
PAGE_ICON = "🌊"
REFRESH_RATE_S = 0.5

# CSS/Styles
CONSOLE_STYLE = """
    <div id="term" style="background-color: #0e1117; color: #00ff00; font-family: 'Courier New', Courier, monospace; 
         font-size: 14px; height: 200px; overflow-y: auto; padding: 10px; border: 1px solid #444; border-radius: 5px;">
        {log_html}
    </div>
    <script>
        var d = document.getElementById("term");
        d.scrollTop = d.scrollHeight;
    </script>
"""
