import os
from SCons.Script import Import

Import("env")

def bin_elf_copy(source, target, env):
    elf_file = str(target[0])
    bin_file = elf_file.replace(".elf", ".bin")
    print(f"[POST-BUILD] Generating binary: {bin_file} from {elf_file}")
    cmd = [env.subst("$OBJCOPY"), "-O", "binary", elf_file, bin_file]
    env.Execute(" ".join(cmd))

env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", bin_elf_copy)
