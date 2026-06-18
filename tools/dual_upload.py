Import("env")
import os
import shutil

# Custom script to handle the "Dual Binary" flash over USB
def combined_upload(source, target, env):
    print("\n[CUSTOM UPLOAD] Starting Dual-Binary Wired Flash...")
    
    # Paths
    boot_bin = os.path.join(env.subst("$PROJECT_BUILD_DIR"), "bootloader", "firmware.bin")
    app_bin = os.path.join(env.subst("$PROJECT_BUILD_DIR"), "float", "firmware.bin")
    
    # Verify files exist
    if not os.path.exists(boot_bin):
        print(f"Error: Bootloader binary missing at {boot_bin}. Run 'pio run -e bootloader' first.")
        return
    if not os.path.exists(app_bin):
        print(f"Error: App binary missing at {app_bin}. Run 'pio run -e float' first.")
        return

    # Use picotool to load both at offsets
    # Note: This assumes picotool is in the PATH. 
    # If not, we can find it in the platformio packages folder.
    
    print(f"1. Flashing Bootloader to 0x10000000...")
    env.Execute(f"picotool load {boot_bin} --offset 0x10000000")
    
    print(f"2. Flashing Main App to 0x10008000...")
    env.Execute(f"picotool load {app_bin} --offset 0x10008000")
    
    print("3. Resetting and starting...")
    env.Execute("picotool reboot")
    print("[CUSTOM UPLOAD] Done!\n")

# Register custom target
env.AddCustomTarget(
    "upload_dual",
    None,
    combined_upload,
    title="Upload Bootloader + App",
    description="Flashes both the bootloader (0x0) and main app (0x8000) over USB."
)
