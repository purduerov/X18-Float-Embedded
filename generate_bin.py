import os
import shutil
import subprocess
import sys
from SCons.Script import Import

Import("env")

def bin_elf_copy(source, target, env):
    elf_file = str(target[0])
    bin_file = elf_file.replace(".elf", ".bin")
    old_bin_file = elf_file.replace(".elf", "_old.bin")
    patch_bin_file = elf_file.replace(".elf", "_patch.bin")
    
    print(f"[POST-BUILD] Generating binary: {bin_file} from {elf_file}")
    cmd = [env.subst("$OBJCOPY"), "-O", "binary", elf_file, bin_file]
    env.Execute(" ".join(cmd))
    
    # Auto-generate bsdiff patch if we have a previous build
    if os.path.exists(old_bin_file) and os.path.exists(bin_file):
        try:
            print(f"[OTA] Generating bsdiff patch against previous build...")
            # Use the explicit sys.executable to ensure we hit the host python with pip modules
            proj_dir = env.subst("$PROJECT_DIR")
            script_path = os.path.join(proj_dir, "tools", "ota_patch_gen.py")
            subprocess.run([sys.executable, script_path, old_bin_file, bin_file, patch_bin_file], check=True)
            print(f"[OTA] Auto-patch created: {patch_bin_file}")
        except Exception as e:
            print(f"[OTA] WARNING: Error generating patch automatically: {e}")
            
    # Save the new binary to serve as the 'old' reference for the next build
    if os.path.exists(bin_file):
        shutil.copy(bin_file, old_bin_file)

env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", bin_elf_copy)
