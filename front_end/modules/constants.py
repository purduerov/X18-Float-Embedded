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
    <div id="term" style="background-color: #0e1117; color: #d4d4d4; font-family: 'Courier New', Courier, monospace; 
         font-size: 13px; height: 300px; overflow-y: auto; padding: 10px; border: 1px solid #333; border-radius: 5px; line-height: 1.4;">
        {log_html}
    </div>
    <script>
        var d = document.getElementById("term");
        var autoScroll = sessionStorage.getItem("term_autoScroll");
        var prevScrollTop = sessionStorage.getItem("term_scrollTop");
        
        if (autoScroll === "false" && prevScrollTop !== null) {
            d.scrollTop = parseInt(prevScrollTop, 10);
        } else {
            d.scrollTop = d.scrollHeight;
        }
        
        d.addEventListener("scroll", function() {
            var isAtBottom = d.scrollHeight - d.scrollTop - d.clientHeight < 50;
            sessionStorage.setItem("term_autoScroll", isAtBottom ? "true" : "false");
            sessionStorage.setItem("term_scrollTop", d.scrollTop.toString());
        });
    </script>
"""

PACKET_CONSOLE_STYLE = """
    <div id="pterm" style="background-color: #070f1a; color: #00ffcc; font-family: 'Courier New', Courier, monospace; 
         font-size: 14px; height: 400px; overflow-y: auto; padding: 12px; border: 1px solid #00ccff; border-radius: 5px; line-height: 1.5; box-shadow: inset 0 0 10px rgba(0, 204, 255, 0.2);">
        {log_html}
    </div>
    <script>
        var d = document.getElementById("pterm");
        var autoScroll = sessionStorage.getItem("pterm_autoScroll");
        var prevScrollTop = sessionStorage.getItem("pterm_scrollTop");
        
        if (autoScroll === "false" && prevScrollTop !== null) {
            d.scrollTop = parseInt(prevScrollTop, 10);
        } else {
            d.scrollTop = d.scrollHeight;
        }
        
        d.addEventListener("scroll", function() {
            var isAtBottom = d.scrollHeight - d.scrollTop - d.clientHeight < 50;
            sessionStorage.setItem("pterm_autoScroll", isAtBottom ? "true" : "false");
            sessionStorage.setItem("pterm_scrollTop", d.scrollTop.toString());
        });
    </script>
"""


