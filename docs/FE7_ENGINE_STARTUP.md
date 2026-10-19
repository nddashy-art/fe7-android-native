# FE7 engine startup integration — first milestone

This commit introduces a native startup-preparation boundary, not a playable native port.

## What it does

- Rejects missing/too-small ROM buffers and files whose title header does not resemble Fire Emblem.
- Resets the software GBA memory bus and attaches the caller-owned ROM buffer.
- Exposes a small lifecycle API and status message so JNI/frontend code can prepare and tear down the native runtime.
- Adds a synthetic-header test that does not contain commercial game data.

## What it does not do

It does **not** call `AgbMain`, run FE7's game loop, render GBA backgrounds/sprites, advance timers/HBlank, or synthesize FE7 audio. The current Android CMake target still does not compile the FE7 decompilation sources, and the existing play path remains mGBA/libretro. The ROM buffer must remain alive for as long as the memory bus is attached.

## Current build progress

When a local `FE7_SOURCE_ROOT` is passed to Gradle, 13 original FE7 C units are linked into the existing native library. Additional game/service C files, the adapted `main.c`, the ARMv7 C sound driver, and eight ARMv7 assembly units are compiled as isolated probes. They are not linked or executed.

A trial full link exposed fixed-address `R_ARM_ABS32` relocations in the original assembly/C code that are not valid for Android's position-independent shared library. Some game functions and ROM-backed data also remain unresolved. The current build therefore intentionally keeps those probes out of the shipped library.

## Next integration step

Create a dependency/link report for the complete source/object set, then convert or replace fixed-address references and GBA-only BIOS instructions using tested host services. Only after those objects can be linked with no text relocations or unresolved game symbols should a native `AgbMain` entrypoint be called. Until then, the frontend must keep using the established emulator path rather than implying the game has booted natively.
