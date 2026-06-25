import serial
import serial.tools.list_ports
import threading
import time
import re
import os
import struct
from datetime import datetime
from modules.constants import DEFAULT_TEAM_ID

class HardwareManager:
    """Manages the serial connection and state behind the scenes."""
    _instances = []

    def __init__(self):
        # Stop any previous instances to avoid duplicate thread conflicts
        for inst in list(HardwareManager._instances):
            try:
                inst.running = False
                inst.disconnect()
            except:
                pass
        HardwareManager._instances.clear()
        HardwareManager._instances.append(self)

        self.ser = None
        self.data_log = []
        self.console_log = []
        self.packet_log = []
        self.lock = threading.RLock() # Changed to RLock to allow nested calls
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
        
        # HIL Simulation State
        from modules.simulator import BuoyancySimulator
        # Use defaults for now, will sync with Pico settings on connect/start
        self.simulator = BuoyancySimulator(
            mass_g=3489,
            diameter_in=4.5,
            length_in=12,
            syringe_ml=90,
            pool_depth_ft=15,
            neutral_adc=2048,
            act_min=126,
            act_max=3900
        )
        self.hil_enabled = False
        self.hil_start_time = None
        self.hil_ser = None
        self.hil_thread = None
        self.hil_port = None
        self.hil_running = False
        
        self.running = True
        
        # Stop any previous threads that might be hanging around (Streamlit reloads)
        for t in threading.enumerate():
            if t.name == "HardwareListener" or t.name == "HILListener":
                # We can't easily kill them, but we can signal them if they share a class attribute
                pass

        self.thread = threading.Thread(target=self.serial_listener, daemon=True, name="HardwareListener")
        self.thread.start()

    def log_message(self, message):
        """Centralized logging to the console with size capping to prevent WebSocket overflow."""
        # Safety check: if this is an old instance being used by a new thread
        if not hasattr(self, 'console_log'):
            return
            
        with self.lock:
            self.console_log.append(message)
            if len(self.console_log) > 100:
                self.console_log.pop(0)

    def _safe_log(self, message):
        """Internal helper to log even if log_message is somehow missing (e.g. race during reload)."""
        try:
            self.log_message(message)
        except AttributeError:
            with self.lock:
                if hasattr(self, 'console_log'):
                    self.console_log.append(message)
                    if len(self.console_log) > 100:
                        self.console_log.pop(0)

    def get_available_ports(self):
        """Returns a list of ListPortInfo objects, filtering out Bluetooth serial ports."""
        all_ports = serial.tools.list_ports.comports()
        # Filter out "Standard Serial over Bluetooth" which often causes stalls or errors
        # Check both description and device name for "bluetooth", case-insensitive
        filtered = [
            p for p in all_ports 
            if "bluetooth" not in p.description.lower() and "bluetooth" not in p.device.lower()
        ]
        return filtered

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
                
                self._safe_log(f"[SUCCESS] Connected to {port} at {baud} baud.")
                self.mission_status = "IDLE"
            except Exception as e:
                self.ser = None
                self._safe_log(f"[ERROR] Could not connect to {port}. {e}")

    def disconnect(self):
        self.disconnect_hil()
        with self.lock:
            if self.ser:
                try:
                    self.ser.close()
                except:
                    pass
                self.ser = None
            self.mission_status = "DISCONNECTED"
            self._safe_log("[INFO] Disconnected.")

    def connect_hil(self, port, baud=115200):
        with self.lock:
            if self.hil_ser and self.hil_ser.is_open:
                try:
                    self.hil_ser.close()
                except:
                    pass
            try:
                self.hil_ser = serial.Serial(port, baud, timeout=1.0, write_timeout=None)
                self.hil_ser.dtr = False
                self.hil_ser.rts = False
                time.sleep(0.1)
                self.hil_ser.dtr = True
                self.hil_ser.rts = True
                self.hil_port = port
                
                self._safe_log(f"[SYSTEM] HIL Link connected to {port} at {baud} baud.")
                
                self.hil_running = True
                # Start HIL background listening thread
                self.hil_thread = threading.Thread(target=self.hil_serial_listener, daemon=True)
                self.hil_thread.start()
            except Exception as e:
                self.hil_ser = None
                self.hil_port = None
                self._safe_log(f"[ERROR] Could not connect HIL Link to {port}. {e}")

    def disconnect_hil(self):
        with self.lock:
            self.hil_running = False
            if self.hil_ser:
                try:
                    self.hil_ser.close()
                except:
                    pass
                self.hil_ser = None
                self.hil_port = None
                self._safe_log("[SYSTEM] HIL Link disconnected.")

    def update_simulator_physical_params(self, mass_g, diameter_in, length_in, additional_volume_in3, syringe_ml, temp_c, realistic_physics):
        import math
        with self.lock:
            # Sync settings from float configuration to configure neutral ADC calibration first
            try:
                n_adc = int(float(self.float_settings.get("Neutral", 2048)))
                a_min = int(float(self.float_settings.get("ActMin", 126)))
                a_max = int(float(self.float_settings.get("ActMax", 3900)))
                self.simulator.set_calibration(n_adc, a_min, a_max)
            except (ValueError, AttributeError):
                pass

            self.simulator.mass = mass_g / 1000.0
            self.simulator.diameter = diameter_in * 0.0254
            self.simulator.length = length_in * 0.0254
            self.simulator.additional_volume_in3 = additional_volume_in3
            self.simulator.syringe_volume = syringe_ml / 1e6
            self.simulator.temp_c = temp_c
            self.simulator.realistic_physics = realistic_physics
            
            # Recalculate baseline. In realistic mode, this automatically computes the correct
            # mass so the float is neutrally buoyant at the calibrated neutral_adc.
            self.simulator.reset()
            
            final_mass_g = self.simulator.mass * 1000.0
            
            self._safe_log(f"[SYSTEM] HIL Simulator updated: Dia={diameter_in}\", Len={length_in}\", AddVol={additional_volume_in3}in³, Syringe={syringe_ml}mL, Temp={temp_c}°C, Realistic={realistic_physics}")
            if realistic_physics:
                self._safe_log(f"  -> [PHYSICS] Auto-calculated neutral mass: {final_mass_g:.1f}g (derived from calibrated Neutral ADC {self.simulator.neutral_adc})")
            else:
                self._safe_log(f"  -> [CALIBRATION] Using user-defined mass: {final_mass_g:.1f}g")
            
            # Calculate and log midpoint target weight
            rho_w = (999.842594 + 6.793952e-2 * temp_c - 9.095290e-3 * temp_c**2 + 
                     1.001685e-4 * temp_c**3 - 1.120083e-6 * temp_c**4 + 6.536332e-9 * temp_c**5)
            dia_m = diameter_in * 0.0254
            len_m = length_in * 0.0254
            v_cylinder_m3 = math.pi * ((dia_m / 2.0) ** 2) * len_m
            v_add_m3 = (additional_volume_in3 * 16.387064) / 1e6
            v_hull_m3 = v_cylinder_m3 + v_add_m3
            v_syr_mid_m3 = (syringe_ml / 2.0) / 1e6
            m_recommended = rho_w * (v_hull_m3 + v_syr_mid_m3) * 1000.0
            
            self._safe_log(f"[PHYSICS] Ballast calculation for target pool temp {temp_c}°C:")
            self._safe_log(f"  -> Recommended Float Mass (midpoint neutral): {m_recommended:.1f} grams")
            self._safe_log(f"  -> Mismatch from recommended midpoint: {final_mass_g - m_recommended:+.1f} grams")

    def hil_serial_listener(self):
        serial_buffer = ""
        while self.running and self.hil_running:
            data = None
            try:
                with self.lock:
                    if self.hil_ser and self.hil_ser.is_open:
                        in_waiting = self.hil_ser.in_waiting
                        if in_waiting > 0:
                            data = self.hil_ser.read(in_waiting)
                
                if data:
                    serial_buffer += data.decode('utf-8', errors='ignore')
                    while '\n' in serial_buffer:
                        line_raw, serial_buffer = serial_buffer.split('\n', 1)
                        line = line_raw.strip()
                        if line:
                            try:
                                self.parse_incoming_line(line, from_usb=True)
                            except Exception as pe:
                                self._safe_log(f"[WARN] Error parsing HIL serial line: {pe}")
                else:
                    time.sleep(0.02)
            except Exception as e:
                time.sleep(0.5)

    def send_command(self, cmd):
        with self.lock:
            # 1. Primary path: Surface Station (Radio relay)
            if self.ser and self.ser.is_open:
                try:
                    self.ser.write(f"{cmd}\n".encode('utf-8'))
                    self._safe_log(f"[TX] > Sent: {cmd}")
                except (serial.SerialException, OSError) as e:
                    self._safe_log(f"[ERROR] Surface Link Lost: {e}")
                    self.ser = None
                    self.mission_status = "DISCONNECTED"
            
            # 2. HIL path: Direct to Float USB (bypass radio)
            # If HIL is enabled and we have a direct cable to the Pico, 
            # we should send commands there too. This is much more reliable
            # for bench testing and bypasses Surface FSM state restrictions.
            if self.hil_enabled and self.hil_ser and self.hil_ser.is_open:
                try:
                    self.hil_ser.write(f"{cmd}\n".encode('utf-8'))
                    # We don't duplicate the [TX] log if already sent to Surface
                    if not (self.ser and self.ser.is_open):
                        self._safe_log(f"[TX-HIL] > Sent: {cmd}")
                except Exception as e:
                    self._safe_log(f"[ERROR] HIL Link Write Failed: {e}")
            
            # Final fallback if no ports open
            if not (self.ser and self.ser.is_open) and not (self.hil_enabled and self.hil_ser and self.hil_ser.is_open):
                self._safe_log("[ERROR] Cannot send command: No active connection.")

    def _threaded_cmd_sequence(self, commands, delay=0.5):
        """Runs a sequence of commands with a delay in a background thread."""
        def run():
            for cmd in commands:
                self.send_command(cmd)
                if delay > 0 and cmd != commands[-1]:
                    time.sleep(delay)
        threading.Thread(target=run, daemon=True).start()

    def update_team_id(self, val):
        self._threaded_cmd_sequence([f"c {val}", "?"])
        
    def update_duration(self, val):
        self._threaded_cmd_sequence([f"t {val}", "?"])

    def update_deep_target(self, val):
        self._threaded_cmd_sequence([f"d {val}", "?"])

    def update_shallow_target(self, val):
        self._threaded_cmd_sequence([f"u {val}", "?"])

    def update_num_profiles(self, val):
        self._threaded_cmd_sequence([f"m {val}", "?"])
        
    def update_pid(self, p, i, d):
        self._threaded_cmd_sequence([f"s {p} {i} {d}", "?"])

    def update_bounds(self, min_val, max_val):
        self._threaded_cmd_sequence([f"b {min_val} {max_val}", "?"])

    def update_neutral_adc(self, val):
        self._threaded_cmd_sequence([f"n {val}", "?"])

    def update_tolerance(self, val):
        self._threaded_cmd_sequence([f"v {val}", "?"])

    def zero_depth(self):
        self._threaded_cmd_sequence(["z", "?"])

    def reset_fsm(self):
        self.send_command("r")
        self.mission_status = "IDLE"
        with self.lock:
            self.packet_log = []
        self._safe_log("[WARNING] > Sent: r (Forced FSM Reset)")

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
        self._safe_log("[WARN] OTA cancelled by user. Surface will recover in ~5s.")

    def reflash_firmware(self, firmware_data):
        """Starts a background thread to handle the OTA reflash process."""
        if not self.ser or not self.ser.is_open:
            self._safe_log("[ERROR] Cannot reflash: Not connected.")
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
            
            self._safe_log(f"[SYSTEM] STARTING OTA REFLASH: {file_size} bytes, CRC 0x{file_crc:08X}")
            
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
                    self._safe_log(f"[ERROR] REFLASH ERROR: {self.reflash_error if self.reflash_error else 'Timeout waiting for sync.'}")
                    with self.lock:
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
                            self._safe_log(f"[WARN] OTA cancelled at {sent_bytes} bytes.")
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
                        self._safe_log(f"[ERROR] REFLASH ERROR: {self.reflash_error if self.reflash_error else f'Link lost at {sent_bytes} bytes.'}")
                        with self.lock:
                            self.reflash_in_progress = False
                        return
                        
                    sent_bytes += CHUNK_SIZE
                    with self.lock:
                        self.reflash_progress = int((sent_bytes / file_size) * 100)

                
                self._safe_log("[SUCCESS] REFLASH SUCCESS: Data transfer complete.")
            except Exception as e:
                self._safe_log(f"[ERROR] REFLASH CRITICAL ERROR: {e}")
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
            self.data_log = []
            if self.hil_enabled:
                # Sync current settings to simulator baseline
                try:
                    n_adc = int(float(self.float_settings.get("Neutral", 2048)))
                    a_min = int(float(self.float_settings.get("ActMin", 126)))
                    a_max = int(float(self.float_settings.get("ActMax", 3900)))
                    self.simulator.set_calibration(n_adc, a_min, a_max)
                except ValueError:
                    pass
                self.hil_start_time = None
                self.simulator.reset()
        self.send_command('p') 

    def handle_hil_packet(self, line):
        # Format: [HIL_OUT] Target=%d ADC=%d State=%d Stage=%d Depth=%.3f
        try:
            with self.lock:
                m = re.search(r"Target=(-?\d+)\s+ADC=(\d+)\s+State=(\d+)\s+Stage=(\d+)\s+Depth=([\d\.-]+)", line)
                if m:
                    target = int(m.group(1))
                    adc = int(m.group(2))
                    state = int(m.group(3))
                    stage = int(m.group(4))
                    
                    # Check for state 5 (TEST_CALIBRATE) or state 2 (PROFILING) to log data
                    is_test_mode = "State=5" in line
                    is_profiling = "State=2" in line
                    
                    # Update live settings values for metric widgets
                    self.float_settings["ADC"] = str(adc)
                    self.float_settings["TarAct"] = str(target)
                    self.float_settings["State"] = str(state)
                    self.float_settings["Stage"] = str(stage)
                    
                    if self.hil_enabled:
                        # Run buoyancy physics step using stored calibration
                        sim_depth = self.simulator.step(current_adc=adc)
                        
                        # Update Internal metrics for UI
                        self.float_settings["LiveDepth"] = f"{sim_depth:.3f}"
                        
                        # ONLY feed depth back if the Float is in a state expecting HIL input
                        # (PROFILING=2 or TEST_CALIBRATE=5)
                        if is_profiling or is_test_mode:
                            # 1. Send 'h' command to the direct HIL Target link
                            if self.hil_ser and self.hil_ser.is_open:
                                try:
                                    self.hil_ser.write(f"h {sim_depth:.3f}\n".encode('utf-8'))
                                except Exception as e:
                                    self._safe_log(f"[ERROR] HIL Feed failed (Target): {e}")

                            # 2. ALSO send to primary Surface link (Radio relay to Float)
                            if self.ser and self.ser.is_open:
                                try:
                                    self.ser.write(f"h {sim_depth:.3f}\n".encode('utf-8'))
                                except Exception as e:
                                    self._safe_log(f"[ERROR] HIL Feed failed (Surface): {e}")
                        
                        # Log data point for live chart (Log in HIL mode if profiling or in test mode)
                        if is_profiling or is_test_mode:
                            if self.hil_start_time is None:
                                self.hil_start_time = time.time()
                            rel_time_s = time.time() - self.hil_start_time
                            
                            # Calculate pressure in kPa for MATE 2026 compliance
                            # Using 1029.0 kg/m^3 to match Pico's default fluid density
                            pressure_kpa = (sim_depth * 1029.0 * 9.80665 + 101325.0) / 1000.0
                            
                            try:
                                co_id = int(float(self.float_settings.get("Co#", DEFAULT_TEAM_ID)))
                            except ValueError:
                                co_id = DEFAULT_TEAM_ID
                                
                            raw_str = f"Company #{co_id}, Time: {rel_time_s:.1f}s, Pressure: {pressure_kpa:.2f} kPa, Depth: {sim_depth:.2f}m"
                            
                            entry = {
                                "Time (s)": rel_time_s,
                                "Depth (m)": sim_depth,
                                "Pressure (kPa)": pressure_kpa,
                                "Actuator (ADC)": adc,
                                "Target (ADC)": target,
                                "State": state
                            }
                            # Prevent duplicate entries if simulation step is faster than telemetry
                            if not self.data_log or self.data_log[-1]["Time (s)"] < rel_time_s:
                                self.data_log.append(entry)
                                if len(self.data_log) > 2000: # Increase log size for longer test runs
                                    self.data_log.pop(0)
                                self.packet_log.append(raw_str)
                                if len(self.packet_log) > 100:
                                    self.packet_log.pop(0)
        except Exception as e:
            self._safe_log(f"[ERROR] HIL Packet error: {e}")

    def save_profile_data(self):
        """Automatically saves mission telemetry and config to a CSV file."""
        with self.lock:
            local_data = list(self.data_log)
        if not local_data:
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
                
                # Check columns present in the dataset
                has_pressure = any("Pressure (kPa)" in entry for entry in local_data)
                has_state = any("State" in entry for entry in local_data)
                has_actuator = any("Actuator (ADC)" in entry for entry in local_data)
                has_target = any("Target (ADC)" in entry for entry in local_data)

                # Write CSV Header
                header_cols = ["Time (s)", "Depth (m)"]
                if has_pressure:
                    header_cols.append("Pressure (kPa)")
                if has_actuator:
                    header_cols.append("Actuator (ADC)")
                if has_target:
                    header_cols.append("Target (ADC)")
                if has_state:
                    header_cols.append("State")
                f.write(",".join(header_cols) + "\n")
                
                # Write Data
                for entry in local_data:
                    row_cols = [
                        f"{entry.get('Time (s)', 0):.2f}",
                        f"{entry.get('Depth (m)', 0):.3f}"
                    ]
                    if has_pressure:
                        row_cols.append(f"{entry.get('Pressure (kPa)', 0):.2f}")
                    if has_actuator:
                        row_cols.append(f"{entry.get('Actuator (ADC)', 0)}")
                    if has_target:
                        row_cols.append(f"{entry.get('Target (ADC)', 0)}")
                    if has_state:
                        row_cols.append(f"{entry.get('State', 0)}")
                    f.write(",".join(row_cols) + "\n")

            self._safe_log(f"[SYSTEM] AUTO-SAVE: Saved profile to {os.path.basename(filename)}")
        except Exception as e:
            self._safe_log(f"[ERROR] AUTO-SAVE ERROR: {e}")

    def _process_normal_line(self, line, from_usb=False):
        """Internal helper to process a single line of standard telemetry (non-HIL)."""
        display_line = f"[Float USB] {line}" if from_usb else line
        self._safe_log(display_line)

        # Capture Surface FSM State for UI status
        if "[DEBUG] State:" in line:
            try:
                state_match = re.search(r"State:\s*(\w+)", line)
                if state_match:
                    surface_state = state_match.group(1)
                    with self.lock:
                        # Update status if we aren't in a more specific profiling state
                        if self.mission_status in ["IDLE", "WAITING_PROFILE", "DOWNLOADING", "DISCONNECTED"]:
                            self.mission_status = surface_state
            except:
                pass

        # Mission Status Logic
        if "PRE-DIVE Packet Logged" in line:
            with self.lock:
                self.mission_status = "PROFILING (Diving to Deep)"
                try:
                    self.active_duration = int(self.float_settings.get("Time", 30))
                except ValueError:
                    self.active_duration = 30
                self.profile_start_time = None
        elif "MISSION: Arrived at" in line:
            stage = "Deep" if "DEEP" in line else "Shallow"
            with self.lock:
                self.mission_status = f"PROFILING (Holding {stage})"
                self.profile_start_time = time.time()
        elif "Moving to" in line:
            stage = "Shallow" if "SHALLOW" in line else "Deep"
            with self.lock:
                self.mission_status = f"PROFILING (Moving to {stage})"
                self.profile_start_time = None
        elif "START DATA DUMP" in line:
            with self.lock:
                self.mission_status = "DOWNLOADING DATA"
                self.data_log = []
                self.packet_log = []
                self.first_timestamp = None
        elif "Download Complete" in line:
            should_save = False
            with self.lock:
                if self.mission_status != "MISSION COMPLETE":
                    self.mission_status = "MISSION COMPLETE"
                    should_save = True
            if should_save:
                self.save_profile_data()
        elif "Profile Done" in line or "Aborting mission" in line or "ABORTING MISSION" in line or "Mission Timeout" in line or "[STALL]" in line:
            should_save = False
            with self.lock:
                if self.mission_status not in ["MISSION COMPLETE", "MISSION ABORTED"]:
                    self.mission_status = "MISSION COMPLETE" if "Profile Done" in line else "MISSION ABORTED"
                    if self.hil_enabled:
                        should_save = True
            if should_save:
                self.save_profile_data()
        elif "[SYNC]" in line:
            matches = re.findall(r'([A-Za-z0-9#]+)=([-]?[\d\.]+)', line)
            if matches:
                with self.lock:
                    for key, value in matches:
                        if key in self.float_settings:
                            self.float_settings[key] = value
                    
                    # Keep simulator calibration and mass calculation in sync
                    try:
                        n_adc = int(float(self.float_settings.get("Neutral", 2048)))
                        a_min = int(float(self.float_settings.get("ActMin", 126)))
                        a_max = int(float(self.float_settings.get("ActMax", 3900)))
                        self.simulator.set_calibration(n_adc, a_min, a_max)
                    except (ValueError, AttributeError):
                        pass
                self._safe_log("[SUCCESS] UI Synced Successfully.")
        elif "Progress:" in line:
            m = re.search(r"Progress:\s*(\d+)/(\d+)", line)
            if m:
                with self.lock:
                    self.last_acked_bytes = int(m.group(1))

        # Telemetry Log Parsing
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

                with self.lock:
                    if self.first_timestamp is None:
                        self.first_timestamp = abs_time_ms
                    first_ts = self.first_timestamp
                rel_time_s = (abs_time_ms - first_ts) / 1000.0

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
                    if len(self.data_log) > 2000:
                        self.data_log.pop(0)
                    if raw_str not in self.packet_log:
                        self.packet_log.append(raw_str)
                        if len(self.packet_log) > 100:
                            self.packet_log.pop(0)
            except (ValueError, IndexError):
                pass

    def parse_incoming_line(self, line, from_usb=False):
        """Iteratively extract HIL data and process other text without recursion."""
        # Format: [HIL_OUT] Target=%d ADC=%d State=%d Stage=%d Depth=%.3f
        hil_pattern = r"\[HIL_OUT\].*?(?=\[HIL_OUT\]|$)"
        
        # 1. Process all HIL packets in the line
        for match in re.finditer(hil_pattern, line):
            self.handle_hil_packet(match.group(0))
            
        # 2. Process all non-HIL parts of the line
        non_hil_parts = re.split(hil_pattern, line)
        for part in non_hil_parts:
            part = part.strip()
            # Clean up potential leading '[' if it was part of "[HIL_OUT]" splitting artifact
            if part.endswith('['):
                part = part[:-1].strip()
            if part:
                self._process_normal_line(part, from_usb)

    def serial_listener(self):
        serial_buffer = ""
        while self.running:
            data = None
            try:
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
                            try:
                                # Use unified iterative parser
                                self.parse_incoming_line(line, from_usb=False)
                            except Exception as pe:
                                self._safe_log(f"[WARN] Error parsing serial line: {pe}")
                else:
                    time.sleep(0.02)  # Sleep 20ms if no data to save CPU and reduce lock contention
            except (serial.SerialException, OSError, Exception) as e:
                with self.lock:
                    if self.ser:
                        try: self.ser.close()
                        except: pass
                        self.ser = None
                        self.mission_status = "DISCONNECTED"
                        self._safe_log(f"[ERROR] Serial error: {e}")
                time.sleep(0.1)
