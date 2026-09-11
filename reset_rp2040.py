if __name__ != "__main__":
    Import("env")
    import os

    # 1. Find local picotool path from PlatformIO packages
    picotool_path = "picotool"
    try:
        platform = env.PioPlatform()
        package_dir = None
        for pkg_name in ["tool-picotool-rp2040-earlephilhower", "tool-picotool"]:
            try:
                package_dir = platform.get_package_dir(pkg_name)
                if package_dir:
                    break
            except Exception:
                continue

        if package_dir:
            potential_path = os.path.join(package_dir, "picotool")
            if os.path.exists(potential_path):
                picotool_path = potential_path
            elif os.path.exists(potential_path + ".exe"):
                picotool_path = potential_path + ".exe"
    except Exception:
        pass

    # Normalize path separators for shell execution
    picotool_path = picotool_path.replace("\\", "/")

    max_size = env.BoardConfig().get("upload.maximum_size", 0)

    # Override the PlatformIO upload command to invoke this script
    env.Replace(
        UPLOADCMD=f'python "$PROJECT_DIR/reset_rp2040.py" "$UPLOAD_PORT" "$SOURCE" {max_size} "{picotool_path}"'
    )

else:
    import sys
    import os
    import subprocess
    import re
    import time

    def discover_bootsel_devices(picotool_path):
        """Run picotool info and parse bus/address pairs from output or error."""
        devices = []
        try:
            result = subprocess.run(
                [picotool_path, "info"],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
            )
            combined = result.stdout + "\n" + result.stderr
        except Exception as e:
            print(f"[CUSTOM UPLOAD] Error running picotool info: {e}")
            return devices

        # Match patterns like "bus 1, address 23" with optional "Device at" prefix
        for m in re.finditer(r"(?:Device at )?bus\s+(\d+),\s*address\s+(\d+)", combined):
            devices.append((m.group(1), m.group(2)))
        return devices

    def get_device_flash_kb(picotool_path, bus, addr):
        """Query a specific device's flash size in KB."""
        try:
            result = subprocess.run(
                [picotool_path, "info", "-d", "--bus", bus, "--address", addr],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
            )
            match = re.search(r"flash size:\s*(\d+)\s*K", result.stdout)
            if match:
                return int(match.group(1))
        except Exception:
            pass
        return None

    def find_target_device(picotool_path, target_flash_kb):
        """Find the BOOTSEL device matching target_flash_kb."""
        devices = discover_bootsel_devices(picotool_path)
        if not devices:
            print("[CUSTOM UPLOAD] No BOOTSEL devices discovered by picotool.")
            return None

        print(f"[CUSTOM UPLOAD] Found {len(devices)} BOOTSEL device(s). Looking for {target_flash_kb}K flash...")
        for bus, addr in devices:
            flash_kb = get_device_flash_kb(picotool_path, bus, addr)
            print(f"[CUSTOM UPLOAD]   Bus {bus}, Addr {addr} -> flash {flash_kb}K")
            if flash_kb == target_flash_kb:
                return bus, addr
        return None

    def main():
        if len(sys.argv) < 5:
            print("Usage: python reset_rp2040.py <port> <source> <max_size> <picotool_path>")
            sys.exit(1)

        port = sys.argv[1]
        source_file = sys.argv[2]
        try:
            max_size = int(sys.argv[3])
        except ValueError:
            max_size = 8384512
        picotool_path = sys.argv[4]

        target_flash_kb = 8192 if max_size > 4194304 else 2048

        print(f"\n[CUSTOM UPLOAD] Target Port: {port}")
        print(f"[CUSTOM UPLOAD] Firmware:    {source_file}")
        print(f"[CUSTOM UPLOAD] Target Flash: {target_flash_kb} KB")
        print(f"[CUSTOM UPLOAD] Picotool:     {picotool_path}")

        # 1. Perform 1200-baud touch to reboot the device on the port
        if port and port != "None" and port != "auto":
            print(f"[CUSTOM UPLOAD] Forcing RP2040 on {port} into BOOTSEL mode via 1200-baud touch...")
            try:
                import serial
                ser = serial.Serial(port, 1200)
                ser.close()
                print("[CUSTOM UPLOAD] 1200-baud touch signal sent. Waiting 1.5s for reboot...")
                time.sleep(1.5)
            except Exception as e:
                print(f"[CUSTOM UPLOAD] 1200-baud touch on {port} failed/skipped: {e}")
                print("[CUSTOM UPLOAD] Proceeding anyway (device may already be in BOOTSEL mode)...")

        # 2. Check for multiple devices in BOOTSEL and find the matched one
        target_args = []
        device_info = find_target_device(picotool_path, target_flash_kb)
        if device_info:
            bus, addr = device_info
            print(f"[CUSTOM UPLOAD] Multi-device detected. Targeting RP2040 at Bus {bus}, Address {addr}")
            target_args = ["--bus", bus, "--address", addr]
        else:
            print("[CUSTOM UPLOAD] Auto-selecting target device (single device or default path)")

        # 3. Run upload command: picotool load -x <source_file> [target_args]
        cmd = [picotool_path, "load", "-x", source_file] + target_args
        print(f"[CUSTOM UPLOAD] Running: {' '.join(cmd)}")

        try:
            result = subprocess.run(cmd, check=True)
            print("[CUSTOM UPLOAD] Upload successful!")
            sys.exit(0)
        except subprocess.CalledProcessError as e:
            print(f"[CUSTOM UPLOAD] Error: picotool upload failed with exit code {e.returncode}")
            sys.exit(e.returncode)

    main()
