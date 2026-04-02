import serial
import struct
import zlib
import sys
import os
import time

def calculate_crc32(file_path):
    with open(file_path, 'rb') as f:
        crc = 0xFFFFFFFF
        while True:
            chunk = f.read(4096)
            if not chunk:
                break
            crc = zlib.crc32(chunk, crc)
    return crc & 0xFFFFFFFF

def stream_firmware(port, bin_path):
    if not os.path.exists(bin_path):
        print(f"Error: File {bin_path} not found.")
        return

    file_size = os.path.getsize(bin_path)
    file_crc = calculate_crc32(bin_path)

    print(f"--- Reflash Tool ---")
    print(f"File: {bin_path}")
    print(f"Size: {file_size} bytes")
    print(f"CRC:  0x{file_crc:08X}")

    try:
        ser = serial.Serial(port, 115200, timeout=1)
        # Give the RP2040 a moment to reset after opening serial
        time.sleep(2)
        ser.reset_input_buffer()

        print(f"Sending Start Command 'S'...")
        # Header: 'S', 4-byte size, 4-byte CRC
        header = b'S' + struct.pack('<I', file_size) + struct.pack('<I', file_crc)
        ser.write(header)
        ser.flush()

        # Wait for the Transmitter to acknowledge the start
        print("Waiting for Transmitter to sync LoRa...")
        while True:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(f"[Node] {line}")
            if "Starting reflash" in line:
                break
            if "Failed to start" in line:
                print("Error: Transmitter failed to connect to Receiver.")
                return

        # Start streaming data
        sent_bytes = 0
        with open(bin_path, 'rb') as f:
            while sent_bytes < file_size:
                chunk = f.read(256)
                if not chunk:
                    break
                
                # Pad last chunk if necessary
                if len(chunk) < 256:
                    chunk += b'\xFF' * (256 - len(chunk))

                ser.write(chunk)
                ser.flush()
                
                # Synchronization: Wait for the "Progress" message from the RP2040
                # before sending the next 256 bytes to prevent buffer overflow.
                while True:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        print(f"[Node] {line}")
                    if "Progress" in line:
                        break
                    if "Link lost" in line:
                        print("Error: LoRa link lost.")
                        return

                sent_bytes += 256

        print("\n--- Transfer Complete ---")
        # Final messages from node (CRC verification and reboot)
        for _ in range(5):
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line: print(f"[Node] {line}")

    except serial.SerialException as e:
        print(f"Serial Error: {e}")
    except KeyboardInterrupt:
        print("\nAborted by user.")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python flash_tool.py <PORT> <BINARY_PATH>")
        print("Example: python flash_tool.py COM3 firmware.bin")
    else:
        stream_firmware(sys.argv[1], sys.argv[2])
