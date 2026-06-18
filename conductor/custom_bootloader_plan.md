# Custom Bootloader with Binary Diffing (bsdiff)

## Objective
Transition from the current RAM-resident "Double Bank" OTA reflashing strategy to a robust secondary bootloader utilizing binary diffing via `mendsley/bsdiff`. This will significantly reduce the LoRa airtime required for updates (by handling executable code address shifts efficiently) and provide a fail-safe mechanism against bricking.

## Key Files & Context
- `lib/bsdiff/`: (New) Submodule or cloned repository containing the `mendsley/bsdiff` library.
- `src/bootloader_main.c`: (New) Standalone bootloader application logic.
- `lib/reflash/reflash_target.c`: Will be modified to receive patch files, save them to the staging slot, and trigger a watchdog reboot.
- `platformio.ini`: Needs to be updated with new build environments for both the `bootloader` and the `float_app` (with linker script offsets).
- `app_memmap.ld`: (New) Custom linker script to shift the main application offset to `0x10008000`, reserving the first 32KB of flash for the bootloader.

## Implementation Steps
1. **Acquire Library**: Add `https://github.com/mendsley/bsdiff.git` to `lib_deps` in `platformio.ini` (or download core files).
2. **Flash Partitioning & Build Config**: 
    - Resolve the issue with `board_build.ldscript` or use `board_build.pico_sdk_config` to ensure the application starts at `0x10008000`.
    - Update `platformio.ini` to separate the bootloader build (default offset) and the main app build (shifted offset).
3. **Develop Secondary Bootloader**:
    - Write `bootloader_main.c`.
    - Implement logic to check the RP2040 Watchdog Scratch Register `SCRATCH0` for an update flag.
    - Integrate `bspatch` by implementing the `bsdiff_stream` interface mapped to flash read/writes (RAM-resident functions).
    - If the update flag is present, stream the patch from Slot 1, apply it to Slot 0, verify the resulting binary, and clear the flag.
    - Jump to the application vector table at `0x10008000`.
4. **Update Main Application OTA Logic**:
    - Modify `reflash_target.c` to handle patch streams instead of raw binaries.
    - Upon successful reception of a patch in Slot 1, set the watchdog scratch register flag (`0xDEADBEEF`) and reboot the microcontroller to hand control back to the bootloader.

## Verification
- Build both the bootloader and the main application using PlatformIO.
- Verify linker map files confirm the bootloader is at `0x10000000` and the app is at `0x10008000`.
- (Since hardware testing is prohibited per instructions, verification will focus on successful compilation and correct memory mapping.)