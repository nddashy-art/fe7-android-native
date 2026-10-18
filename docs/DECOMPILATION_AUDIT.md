# FE7 decompilation: first porting audit

This audit is based on the supplied `fe7_us-main` source archive. It records why compiling the decompilation as an Android shared library is not a one-switch change.

## Findings

- The upstream `Makefile` targets the ARM7TDMI/GBA toolchain. It uses `agbcc`, `arm-none-eabi-as/ld/objcopy`, `-mcpu=arm7tdmi`, and a GBA linker script.
- The normal `make compare` target builds a GBA ROM and checks its SHA-1 against `fe7_us.sha1`. That workflow is separate from producing an Android APK.
- `include/gbaio.h` maps hardware registers by dereferencing fixed GBA memory addresses (for example, the display, sound and DMA register ranges near `0x04000000`). Those addresses are not Android graphics/audio APIs.
- `src/main.c:AgbMain` clears fixed GBA RAM areas, initializes interrupts, keys, SRAM, sprites and music, then enters the game's main loop. The interrupt, DMA, timer, video, audio and save assumptions need compatible replacements or an emulated GBA hardware layer.
- The codebase includes ARM assembly as well as C. Building it with Android's NDK does not automatically translate GBA hardware behavior or make its ARM7TDMI assembly compatible with Android's ARM64 devices.

## Current Android-side foundation

The app now has a small C++/JNI bridge compiled with the Android NDK. It parses the title and game-code fields from the first `0xB0` bytes supplied by the Kotlin document reader. A host-side C++ test exercises the parser without any commercial ROM.

This bridge is intentionally **not** a GBA emulator and does not call FE7 gameplay routines. The current APK remains a launcher/diagnostic prototype; it cannot boot the game yet.

## Correct next order

1. Keep the app's native build green and test the portable header parser.
2. Trace the decompilation's startup path and hardware dependencies (input, interrupts/timers, graphics/VRAM, audio, and SRAM).
3. Choose an architecture for executing the GBA game: either a real hardware-compatibility/emulation layer or a substantial decompilation port with equivalent platform APIs. Do not mix GBA memory-mapped register writes directly into Android code.
4. Only after that path runs, add a renderer/frame loop, input translation, audio output, and save persistence; verify an actual title-screen boot on a device.
5. Treat local multiplayer and original-character modding as separate later milestones. Neither is provided by the current bridge.

## Privacy/build boundary

The header parser receives only the bytes read by the installed app from the user's chosen document URI. The CI test uses a generated synthetic header. The ROM itself must stay out of this Git repository and CI artifacts.
