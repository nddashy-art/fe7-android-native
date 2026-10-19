# FE7 engine startup integration — first milestone

This commit introduces a native startup-preparation boundary, not a playable native port.

## What it does

- Rejects missing/too-small ROM buffers and files whose title header does not resemble Fire Emblem.
- Resets the software GBA memory bus and attaches the caller-owned ROM buffer.
- Exposes a small lifecycle API and status message so JNI/frontend code can prepare and tear down the native runtime.
- The Android `nativeStartGame` path now passes the same user-selected ROM buffer into `fe7_engine_prepare` before starting mGBA. The runtime borrows that buffer; shutdown detaches it before the vector is cleared.
- Host smoke tests verify that the ROM bus maps the exact caller-owned buffer and no longer maps it after shutdown, without requiring commercial game data.

## What it does not do

It does **not** call `AgbMain`, run FE7's game loop, render game-derived backgrounds/sprites, advance accurate timers/HBlank, or synthesize FE7 audio. The optional Android build compiles and links 13 FE7 C translation units, but keeps the startup object and additional service/ARM7 assembly probes out of the gameplay link. mGBA/libretro remains responsible for actual CPU execution and gameplay. The ROM buffer must remain alive for as long as the memory bus is attached.

## Current build progress

When a local `FE7_SOURCE_ROOT` is passed to Gradle, 13 original FE7 C units are linked into the existing native library. Additional game/service C files, the adapted `main.c`, the ARMv7 C sound driver, and eight ARMv7 assembly units are compiled as isolated probes. They are not linked or executed.

A trial full link exposed fixed-address `R_ARM_ABS32` relocations in the original assembly/C code that are not valid for Android's position-independent shared library. Some game functions and ROM-backed data also remain unresolved. The current build therefore intentionally keeps those probes out of the shipped library.

## Next integration step

Create a dependency/link report for the complete source/object set, then convert or replace fixed-address references and GBA-only BIOS instructions using tested host services. Only after those objects can be linked with no text relocations or unresolved game symbols should a native `AgbMain` entrypoint be called. Until then, the frontend must keep using the established emulator path rather than implying the game has booted natively.
