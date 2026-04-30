import serial
import serial.tools.list_ports
import threading
import time
import re

class HardwareManager:
    """Manages the serial connection and state behind the scenes."""
    def __init__(self):
        self.ser = None
        self.data_log = []
        self.console_log = []
        self.lock = threading.Lock()
        self.mission_status = "IDLE"
        self.first_timestamp = None
        
        self.float_settings = {
            "P": "--", "I": "--", "D": "--", "Tar": "--",
            "Co#": "--", "Time": "--", "ADC": "--",
            "TarAct": "--",
            "ActMin": "--", "ActMax": "--",
            "Neutral": "--",
            "Off": "--",
            "LiveDepth": "--",
            "Tol": "--"
        }
        
        # Countdown Timer variables
        self.profile_start_time = None
        self.active_duration = 0
        
        self.running = True
        self.thread = threading.Thread(target=self.serial_listener, daemon=True)
        self.thread.start()

    def get_available_ports(self):
        """Returns a list of ListPortInfo objects containing device, description, etc."""
        return serial.tools.list_ports.comports()

    def connect(self, port, baud=115200):
        with self.lock:
            if self.ser and self.ser.is_open:
                try:
                    self.ser.close()
                except:
                    pass
            try:
                # Increased timeout for more reliable readline()
                self.ser = serial.Serial(port, baud, timeout=0.1, write_timeout=0.5)
                # Force DTR/RTS to reset Pico serial if needed
                self.ser.dtr = False
                self.ser.rts = False
                time.sleep(0.1)
                self.ser.dtr = True
                self.ser.rts = True
                
                self.console_log.append(f"🟢 Connected to {port} at {baud} baud.")
                self.mission_status = "IDLE"
            except Exception as e:
                self.ser = None
                self.console_log.append(f"🔴 ERROR: Could not connect to {port}. {e}")

    def disconnect(self):
        with self.lock:
            if self.ser:
                try:
                    self.ser.close()
                except:
                    pass
                self.ser = None
                self.console_log.append("⚪ Disconnected.")
                self.mission_status = "DISCONNECTED"

    def send_command(self, cmd):
        with self.lock:
            if self.ser and self.ser.is_open:
                try:
                    self.ser.write(f"{cmd}\n".encode('utf-8'))
                    self.console_log.append(f"🔵 > Sent: {cmd}")
                except (serial.SerialException, OSError) as e:
                    self.console_log.append(f"🔴 Connection Lost: {e}")
                    self.ser = None
                    self.mission_status = "DISCONNECTED"
            else:
                self.console_log.append("🔴 Cannot send command: Not connected.")

    def update_team_id(self, val):
        self.send_command(f"c {val}")
        
    def update_duration(self, val):
        self.send_command(f"t {val}")

    def update_target_depth(self, val):
        self.send_command(f"d {val}")
        
    def update_pid(self, p, i, d):
        self.send_command(f"s {p} {i} {d}")

    def update_bounds(self, min_val, max_val):
        self.send_command(f"b {min_val} {max_val}")

    def update_neutral_adc(self, val):
        self.send_command(f"n {val}")

    def update_tolerance(self, val):
        self.send_command(f"v {val}")

    def zero_depth(self):
        self.send_command("z")

    def reset_fsm(self):
        self.send_command("r")
        self.mission_status = "IDLE"
        self.console_log.append("⚠️ > Sent: r (Forced FSM Reset)")

    def move_actuator(self, val):
        self.send_command(f"a {val}")

    def test_mode(self):
        self.send_command("k")

    def start_profile(self):
        """Triggers the start command. Timer starts after PRE-DIVE confirmation."""
        self.send_command('p') 

    def serial_listener(self):
        while self.running:
            if self.ser and self.ser.is_open:
                try:
                    # Read all available data
                    if self.ser.in_waiting > 0:
                        lines = self.ser.readlines() # Reads all lines up to timeout
                        for line_raw in lines:
                            if not line_raw:
                                continue
                                
                            line = line_raw.decode('utf-8', errors='ignore').strip()
                            if line:
                                with self.lock:
                                    self.console_log.append(line)
                                    if len(self.console_log) > 100: 
                                        self.console_log.pop(0)
                                
                                # Mission Status Logic
                                if "PRE-DIVE Packet Logged" in line: 
                                    self.mission_status = "PROFILING"
                                    try:
                                        self.active_duration = int(self.float_settings.get("Time", 40))
                                    except ValueError:
                                        self.active_duration = 40
                                    self.profile_start_time = time.time()
                                elif "START DATA DUMP" in line:
                                    self.mission_status = "DOWNLOADING DATA"
                                    with self.lock:
                                        self.data_log = [] # Reset for new mission
                                    self.first_timestamp = None 
                                elif "Download Complete" in line: 
                                    self.mission_status = "MISSION COMPLETE"
                                elif "[SYNC]" in line:
                                    matches = re.findall(r'([A-Za-z0-9#]+)=([-]?[\d\.]+)', line)
                                    if matches:
                                        for key, value in matches:
                                            if key in self.float_settings:
                                                self.float_settings[key] = value
                                        with self.lock:
                                            self.console_log.append(f"✅ UI Synced Successfully.")
                                        
                                # CSV Parsing Logic
                                parts = [p.strip() for p in line.split(',')]
                                if len(parts) >= 3 and parts[0].isdigit():
                                    try:
                                        # Format: Co#,TimeMs,DepthM,ActuatorADC
                                        abs_time_ms = int(parts[1])
                                        depth_m = float(parts[2])
                                        
                                        if self.first_timestamp is None:
                                            self.first_timestamp = abs_time_ms
                                        rel_time_s = (abs_time_ms - self.first_timestamp) / 1000.0
                                        
                                        entry = {
                                            "Time (s)": rel_time_s,
                                            "Depth (m)": depth_m
                                        }
                                        
                                        if len(parts) >= 4 and parts[3].isdigit():
                                            entry["Actuator (ADC)"] = int(parts[3])
                                            
                                        with self.lock:
                                            self.data_log.append(entry)
                                    except (ValueError, IndexError):
                                        pass
                    else:
                        time.sleep(0.01) # Small sleep if no data to avoid CPU hammering
                except (serial.SerialException, OSError, Exception) as e:
                    with self.lock:
                        if self.ser:
                            try: self.ser.close()
                            except: pass
                            self.ser = None
                            self.mission_status = "DISCONNECTED"
                            self.console_log.append(f"🔴 Serial error: {e}")
            else:
                time.sleep(0.1)
