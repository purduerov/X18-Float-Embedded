Import("env")
import subprocess

def before_upload(source, target, env):
    print("Forcing RP2040 into BOOTSEL mode via picotool...")
    try:
        # Runs the picotool command to force reboot
        subprocess.run(["picotool", "reboot", "-f", "-u"], check=True)
        print("Successfully sent reboot command.")
    except Exception as e:
        print(f"Warning: picotool failed to reboot device: {e}")
        print("Attempting standard upload anyway...")

# Attach the function to run BEFORE the actual upload action
env.AddPreAction("upload", before_upload)
