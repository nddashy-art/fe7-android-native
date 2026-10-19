# FE7 engine startup integration — first milestone

This commit introduces a native startup-preparation boundary, not a playable native port.

## What it does

- Rejects missing/too-small ROM buffers and files whose title header does not resemble Fire Emblem.
- Resets the software GBA memory bus and attaches the caller-owned ROM buffer.
- Exposes a small lifecycle API and status message so JNI/frontend code can prepare and tear down the native runtime.
- Adds a synthetic-header test that does not contain commercial game data.

## What it does not do

It does **not** call `AgbMain`, run FE7's game loop, render GBA backgrounds/sprites, advance timers/HBlank, or synthesize FE7 audio. The current Android CMake target still does not compile the FE7 decompilation sources, and the existing play path remains mGBA/libretro. The ROM buffer must remain alive for as long as the memory bus is attached.

## Next integration step

Build the decompilation as a separate `fe7_engine` target with `FE7_NATIVE_PORT`, then resolve each remaining ARM assembly/hardware dependency before exposing a real `AgbMain` entrypoint. Only after that entrypoint can execute should the frontend replace the emulator path.
