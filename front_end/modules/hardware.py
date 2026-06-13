import serial
import serial.tools.list_ports
import threading
import time
import re
import os
import struct
from datetime import datetime

class HardwareManager:
    """Manages the serial connection and state behind the scenes."""
    def __init__(self):
        self.ser = None
        self.data_log = []
        self.console_log = []
        self.packet_log = []
        self.lock = threading.Lock()
        self.mission_status = "IDLE"
        self.first_timestamp = None
        self.reflash_in_progress = False
        self.reflash_cancelled = False
        self.reflash_progress = 0
        self.last_acked_bytes = -1
        self.reflash_error = None
        
        self.float_settings = {
            "P": "--", "I": "--", "D": "--", "Deep": "--", "Shallow": "--", "N": "--",
            "Co#": "--", "Time": "--", "ADC": "--",
            "TarAct": "--",
            "ActMin": "--", "ActMax": "--",
            "Neutral": "--",
            "Off": "--",
            "LiveDepth": "--",
            "Tol": "--",
            "FW": "--"
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
                self.ser = serial.Serial(port, baud, timeout=1.0, write_timeout=None)
                # Force DTR/RTS to reset Pico serial if needed
                self.ser.dtr = False
                self.ser.rts = False
                time.sleep(0.1)
                self.ser.dtr = True
                self.ser.rts = True
                
                self.console_log.append(f"[SUCCESS] Connected to {port} at {baud} baud.")
                self.mission_status = "IDLE"
            except Exception as e:
                self.ser = None
                self.console_log.append(f"[ERROR] Could not connect to {port}. {e}")

    def disconnect(self):
        with self.lock:
            if self.ser:
                try:
                    self.ser.close()
                except:
                    pass
                self.ser = None
                self.console_log.append("[INFO] Disconnected.")
                self.mission_status = "DISCONNECTED"

    def send_command(self, cmd):
        with self.lock:
            if self.ser and self.ser.is_open:
                try:
                    self.ser.write(f"{cmd}\n".encode('utf-8'))
                    self.console_log.append(f"[TX] > Sent: {cmd}")
                except (serial.SerialException, OSError) as e:
                    self.console_log.append(f"[ERROR] Connection Lost: {e}")
                    self.ser = None
                    self.mission_status = "DISCONNECTED"
            else:
                self.console_log.append("[ERROR] Cannot send command: Not connected.")

    def update_team_id(self, val):
        self.send_command(f"c {val}")
        time.sleep(0.5)
        self.console_log.append("[SYNC] Auto-Syncing...")
        self.send_command("?")
        
    def update_duration(self, val):
        self.send_command(f"t {val}")
        time.sleep(0.5)
        self.console_log.append("[SYNC] Auto-Syncing...")
        self.send_command("?")

    def update_deep_target(self, val):
        self.send_command(f"d {val}")
        time.sleep(0.5)
        self.console_log.append("[SYNC] Auto-Syncing...")
        self.send_command("?")

    def update_shallow_target(self, val):
        self.send_command(f"u {val}")
        time.sleep(0.5)
        self.console_log.append("[SYNC] Auto-Syncing...")
        self.send_command("?")

    def update_num_profiles(self, val):
        self.send_command(f"m {val}")
        time.sleep(0.5)
        self.console_log.append("[SYNC] Auto-Syncing...")
        self.send_command("?")
        
    def update_pid(self, p, i, d):
        self.send_command(f"s {p} {i} {d}")
        time.sleep(0.5)
        self.console_log.append("[SYNC] Auto-Syncing...")
        self.send_command("?")

    def update_bounds(self, min_val, max_val):
        self.send_command(f"b {min_val} {max_val}")
        time.sleep(0.5)
        self.console_log.append("[SYNC] Auto-Syncing...")
        self.send_command("?")

    def update_neutral_adc(self, val):
        self.send_command(f"n {val}")
        time.sleep(0.5)
        self.console_log.append("[SYNC] Auto-Syncing...")
        self.send_command("?")

    def update_tolerance(self, val):
        self.send_command(f"v {val}")
        time.sleep(0.5)
        self.console_log.append("[SYNC] Auto-Syncing...")
        self.send_command("?")

    def zero_depth(self):
        self.send_command("z")
        time.sleep(0.5)
        self.console_log.append("[SYNC] Auto-Syncing...")
        self.send_command("?")

    def reset_fsm(self):
        self.send_command("r")
        self.mission_status = "IDLE"
        with self.lock:
            self.packet_log = []
        self.console_log.append("[WARNING] > Sent: r (Forced FSM Reset)")

    def move_actuator(self, val):
        self.send_command(f"a {val}")

    def test_mode(self):
        self.send_command("k")

    def calculate_crc32(self, data):
        """Calculates CRC32 exactly as the Pico's crc32_software function does."""
        crc = 0xFFFFFFFF
        for byte in data:
            crc ^= byte
            for _ in range(8):
                if crc & 1:
                    crc = (crc >> 1) ^ 0xEDB88320
                else:
                    crc >>= 1
        # Final XOR 0xFFFFFFFF is standard for CRC32 (zlib)
        return (crc ^ 0xFFFFFFFF) & 0xFFFFFFFF

    def cancel_reflash(self):
        """Signals the reflash thread to stop at the next safe point."""
        with self.lock:
            self.reflash_cancelled = True
            self.console_log.append("[WARN] OTA cancelled by user. Surface will recover in ~5s.")

    def reflash_firmware(self, firmware_data):
        """Starts a background thread to handle the OTA reflash process."""
        if not self.ser or not self.ser.is_open:
            self.console_log.append("[ERROR] Cannot reflash: Not connected.")
            return

        def run_reflash():
            with self.lock:
                self.reflash_in_progress = True
                self.reflash_cancelled = False
                self.reflash_progress = 0
                self.last_acked_bytes = -1
                self.reflash_error = None
                
            # 1. Prepare Data
            CHUNK_SIZE = 220
            padding_needed = (CHUNK_SIZE - (len(firmware_data) % CHUNK_SIZE)) % CHUNK_SIZE
            padded_data = bytearray(firmware_data)
            padded_data.extend(b'\xFF' * padding_needed)
            
            file_size = len(padded_data)
            file_crc = self.calculate_crc32(padded_data)
            
            with self.lock:
                self.console_log.append(f"[SYSTEM] STARTING OTA REFLASH: {file_size} bytes, CRC 0x{file_crc:08X}")
            
            try:
                # 2. Handshake
                # Header: 'S', 4-byte size, 4-byte CRC
                header = b'S' + struct.pack('<I', file_size) + struct.pack('<I', file_crc)
                with self.lock:
                    if self.ser and self.ser.is_open:
                        self.ser.write(header)
                        self.ser.flush()
                
                # 3. Wait for Sync (Wait for ACK for seq 0xFFFFFFFF)
                sync_start = time.time()
                synced = False
                while time.time() - sync_start < 15:
                    with self.lock:
                        if self.last_acked_bytes == 0xFFFFFFFF or any("ACK received for seq" in line for line in self.console_log[-5:]):
                            synced = True
                            break
                        if self.reflash_error:
                            break
                    time.sleep(0.1)
                
                if not synced:
                    with self.lock:
                        self.console_log.append(f"[ERROR] REFLASH ERROR: {self.reflash_error if self.reflash_error else 'Timeout waiting for sync.'}")
                        self.reflash_in_progress = False
                    return

                # 4. Stream Data
                sent_bytes = 0
                while sent_bytes < file_size:
                    chunk = padded_data[sent_bytes:sent_bytes+CHUNK_SIZE]

                    # Give the Pico's USB stack time to flush the "Progress" print
                    # and enter the getchar_timeout_us read loop before blasting 220 bytes.
                    time.sleep(0.02)

                    # Send full chunk — write_timeout=None means this blocks until drained
                    with self.lock:
                        if self.reflash_cancelled:
                            self.console_log.append(f"[WARN] OTA cancelled at {sent_bytes} bytes.")
                            self.reflash_in_progress = False
                            return
                        if self.ser and self.ser.is_open:
                            self.ser.write(chunk)
                            self.ser.flush()

                    # Wait for Progress (no Python-side timeout — surface firmware drives it via "Link lost")
                    target_bytes = sent_bytes + CHUNK_SIZE
                    chunk_ack = False
                    while True:
                        with self.lock:
                            if self.last_acked_bytes >= target_bytes:
                                chunk_ack = True
                                break
                            if self.reflash_error:
                                break
                        time.sleep(0.01)
                    
                    if not chunk_ack:
                        with self.lock:
                            self.console_log.append(f"[ERROR] REFLASH ERROR: {self.reflash_error if self.reflash_error else f'Link lost at {sent_bytes} bytes.'}")
                            self.reflash_in_progress = False
                        return
                        
                    sent_bytes += CHUNK_SIZE
                    with self.lock:
                        self.reflash_progress = int((sent_bytes / file_size) * 100)

                
                with self.lock:
                    self.console_log.append("[SUCCESS] REFLASH SUCCESS: Data transfer complete.")
            except Exception as e:
                with self.lock:
                    self.console_log.append(f"[ERROR] REFLASH CRITICAL ERROR: {e}")
            finally:
                with self.lock:
                    self.reflash_in_progress = False

        threading.Thread(target=run_reflash, daemon=True).start()

    def load_local_firmware(self):
        """Attempts to load the float firmware binary from the platformio build directory."""
        root_dir = os.path.dirname(os.path.dirname(os.path.dirname(__file__)))
        firmware_path = os.path.join(root_dir, ".pio", "build", "float", "firmware.bin")
        
        if not os.path.exists(firmware_path):
            return None, f"Firmware binary not found at: {firmware_path}. Please build the 'float' environment in PlatformIO first."
            
        try:
            with open(firmware_path, "rb") as f:
                data = f.read()
            return data, None
        except Exception as e:
            return None, f"Failed to read firmware file: {e}"

    def start_profile(self):
        """Triggers the start command. Timer starts after PRE-DIVE confirmation."""
        with self.lock:
            self.packet_log = []
        self.send_command('p') 

    def save_profile_data(self):
        """Automatically saves mission telemetry and config to a CSV file."""
        if not self.data_log:
            return

        try:
            # Create profiles directory if it doesn't exist
            profile_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), "profiles")
            if not os.path.exists(profile_dir):
                os.makedirs(profile_dir)

            # Generate filename with timestamp
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = os.path.join(profile_dir, f"profile_{timestamp}.csv")

            with open(filename, "w") as f:
                # Write Configuration Headers
                f.write("# MISSION PROFILE DATA\n")
                f.write(f"# Timestamp: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
                f.write(f"# Deep Target: {self.float_settings.get('Deep', '--')} m\n")
                f.write(f"# Shallow Target: {self.float_settings.get('Shallow', '--')} m\n")
                f.write(f"# Profile Count: {self.float_settings.get('N', '--')}\n")
                f.write(f"# Arrival Tolerance: {self.float_settings.get('Tol', '--')} m\n")
                f.write(f"# Hold Duration: {self.float_settings.get('Time', '--')} s\n")
                f.write(f"# PID: P={self.float_settings.get('P', '--')}, I={self.float_settings.get('I', '--')}, D={self.float_settings.get('D', '--')}\n")
                f.write(f"# Neutral ADC: {self.float_settings.get('Neutral', '--')}\n")
                f.write(f"# Bounds: {self.float_settings.get('ActMin', '--')} to {self.float_settings.get('ActMax', '--')}\n")
                f.write(f"# Depth Offset: {self.float_settings.get('Off', '--')} m\n")
                f.write("# ------------------------------------------\n")
                
                # Write CSV Header
                if any("Pressure (kPa)" in entry for entry in self.data_log):
                    f.write("Time (s),Depth (m),Pressure (kPa),Actuator (ADC),Target (ADC)\n")
                else:
                    f.write("Time (s),Depth (m),Actuator (ADC),Target (ADC)\n")
                
                # Write Data
                for entry in self.data_log:
                    if "Pressure (kPa)" in entry:
                        line = f"{entry.get('Time (s)', 0):.2f},{entry.get('Depth (m)', 0):.3f},{entry.get('Pressure (kPa)', 0):.2f},{entry.get('Actuator (ADC)', 0)},{entry.get('Target (ADC)', 0)}\n"
                    else:
                        line = f"{entry.get('Time (s)', 0):.2f},{entry.get('Depth (m)', 0):.3f},{entry.get('Actuator (ADC)', 0)},{entry.get('Target (ADC)', 0)}\n"
                    f.write(line)

            self.console_log.append(f"[SYSTEM] AUTO-SAVE: Saved profile to {os.path.basename(filename)}")
        except Exception as e:
            self.console_log.append(f"[ERROR] AUTO-SAVE ERROR: {e}")

    def serial_listener(self):
        serial_buffer = ""
        while self.running:
            if self.ser and self.ser.is_open:
                try:
                    # Read all available bytes to prevent readline() from splitting lines
                    data = b''
                    with self.lock:
                        if self.ser and self.ser.is_open:
                            in_waiting = self.ser.in_waiting
                            if in_waiting > 0:
                                data = self.ser.read(in_waiting)
                    if data:
                        serial_buffer += data.decode('utf-8', errors='ignore')

                    while '\n' in serial_buffer:
                        line_raw, serial_buffer = serial_buffer.split('\n', 1)
                        line = line_raw.strip()

                        if line:
                            with self.lock:
                                self.console_log.append(line)
                                if len(self.console_log) > 100: 
                                    self.console_log.pop(0)

                            # Mission Status Logic
                            if "PRE-DIVE Packet Logged" in line: 
                                self.mission_status = "PROFILING (Diving to Deep)"
                                try:
                                    self.active_duration = int(self.float_settings.get("Time", 30))
                                except ValueError:
                                    self.active_duration = 30
                                self.profile_start_time = None 
                            elif "MISSION: Arrived at" in line:
                                stage = "Deep" if "DEEP" in line else "Shallow"
                                self.mission_status = f"PROFILING (Holding {stage})"
                                self.profile_start_time = time.time()
                            elif "Moving to" in line:
                                stage = "Shallow" if "SHALLOW" in line else "Deep"
                                self.mission_status = f"PROFILING (Moving to {stage})"
                                self.profile_start_time = None
                            elif "START DATA DUMP" in line:
                                self.mission_status = "DOWNLOADING DATA"
                                with self.lock:
                                    self.data_log = [] 
                                    self.packet_log = []
                                self.first_timestamp = None 
                            elif "Download Complete" in line: 
                                self.mission_status = "MISSION COMPLETE"
                                self.save_profile_data()
                            elif "[SYNC]" in line:
                                matches = re.findall(r'([A-Za-z0-9#]+)=([-]?[\d\.]+)', line)
                                if matches:
                                    for key, value in matches:
                                        if key in self.float_settings:
                                            self.float_settings[key] = value
                                    with self.lock:
                                        self.console_log.append(f"[SUCCESS] UI Synced Successfully.")

                            # OTA Progress Detection (for smoother UI)
                            if "Progress:" in line:
                                try:
                                    # Format: "Progress: 123/456"
                                    parts = line.split("Progress: ")[1].split("/")
                                    cur = int(parts[0])
                                    total = int(parts[1])
                                    self.reflash_progress = int((cur/total) * 100)
                                    self.last_acked_bytes = cur
                                except:
                                    pass

                            if any(x in line for x in ["Link lost", "Stalled", "Aborting"]):
                                self.reflash_error = line

                            # Match live telemetry logs printed by surface unit
                            # ">> PRE-DIVE Packet Logged: Co# 18 | Time 15000 ms | Depth 2.48 m | Pressure 124.30 kPa"
                            # ">> Stored Data #1: Co# 18 | Time 16000 ms | Depth 2.48 m | Pressure 124.30 kPa"
                            m = re.search(
                                r"(?:PRE-DIVE Packet Logged|Stored Data #\d+):\s*Co#\s*(\d+)\s*\|\s*Time\s*(\d+)\s*ms\s*\|\s*Depth\s*([\d\.-]+)\s*m\s*\|\s*Pressure\s*([\d\.-]+)\s*kPa",
                                line
                            )
                            if m:
                                try:
                                    co_id = int(m.group(1))
                                    time_ms = int(m.group(2))
                                    depth_m = float(m.group(3))
                                    pressure_kpa = float(m.group(4))
                                    time_s = time_ms / 1000.0
                                    raw_str = f"Company #{co_id}, Time: {time_s:.1f}s, Pressure: {pressure_kpa:.2f} kPa, Depth: {depth_m:.2f}m"
                                    with self.lock:
                                        self.packet_log.append(raw_str)
                                        if len(self.packet_log) > 100:
                                            self.packet_log.pop(0)
                                except Exception:
                                    pass

                            # CSV Parsing Logic
                            parts = [p.strip() for p in line.split(',')]
                            if len(parts) >= 3 and parts[0].isdigit():
                                try:
                                    abs_time_ms = int(parts[1])
                                    depth_m = float(parts[2])

                                    if self.first_timestamp is None:
                                        self.first_timestamp = abs_time_ms
                                    rel_time_s = (abs_time_ms - self.first_timestamp) / 1000.0

                                    entry = {
                                        "Time (s)": rel_time_s,
                                        "Depth (m)": depth_m
                                    }

                                    if len(parts) >= 6:
                                        try:
                                            entry["Pressure (kPa)"] = float(parts[3])
                                            entry["Actuator (ADC)"] = int(parts[4])
                                            entry["Target (ADC)"] = int(parts[5])
                                        except ValueError:
                                            pass
                                    else:
                                        if len(parts) >= 4 and parts[3].isdigit():
                                            entry["Actuator (ADC)"] = int(parts[3])
                                        if len(parts) >= 5 and parts[4].isdigit():
                                            entry["Target (ADC)"] = int(parts[4])

                                    co_id = int(parts[0])
                                    pressure_kpa = entry.get("Pressure (kPa)", 0.0)
                                    raw_str = f"Company #{co_id}, Time: {rel_time_s:.1f}s, Pressure: {pressure_kpa:.2f} kPa, Depth: {depth_m:.2f}m"

                                    with self.lock:
                                        self.data_log.append(entry)
                                        if raw_str not in self.packet_log:
                                            self.packet_log.append(raw_str)
                                            if len(self.packet_log) > 100:
                                                self.packet_log.pop(0)
                                except (ValueError, IndexError):
                                    pass
                    else:
                        time.sleep(0.01)
 
                except (serial.SerialException, OSError, Exception) as e:
                    with self.lock:
                        if self.ser:
                            try: self.ser.close()
                            except: pass
                            self.ser = None
                            self.mission_status = "DISCONNECTED"
                            self.console_log.append(f"[ERROR] Serial error: {e}")
            else:
                time.sleep(0.1)
