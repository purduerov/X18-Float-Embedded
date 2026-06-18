import bsdiff4
import os
import sys
import shutil

def generate_patch(old_bin, new_bin, patch_out):
    """Generates a bsdiff patch between two firmware binaries."""
    if not os.path.exists(old_bin):
        print(f"Error: Old binary not found at {old_bin}")
        return False
    if not os.path.exists(new_bin):
        print(f"Error: New binary not found at {new_bin}")
        return False
        
    print(f"Generating patch: {os.path.basename(old_bin)} -> {os.path.basename(new_bin)}")
    bsdiff4.file_diff(old_bin, new_bin, patch_out)
    
    old_size = os.path.getsize(old_bin)
    new_size = os.path.getsize(new_bin)
    patch_size = os.path.getsize(patch_out)
    
    print(f"Success!")
    print(f"  Old Size:   {old_size/1024:.1f} KB")
    print(f"  New Size:   {new_size/1024:.1f} KB")
    print(f"  Patch Size: {patch_size/1024:.1f} KB (Compression: {100*(1-patch_size/new_size):.1f}%)")
    return True

if __name__ == "__main__":
    # Example usage: python generate_patch.py old.bin new.bin patch.bin
    if len(sys.argv) < 4:
        print("Usage: python generate_patch.py <old_bin> <new_bin> <patch_out>")
    else:
        generate_patch(sys.argv[1], sys.argv[2], sys.argv[3])
