# FE7 native-port implementation roadmap

The supplied FE7 US decompilation archive has been inventoried and its checksum recorded in `docs/FE7_SOURCE_ARCHIVE_INVENTORY.md`. The repository now has the software GBA runtime foundation and an opt-in Android build path that compiles and links 11 genuine, ROM-independent FE7 C translation units. The actual gameplay engine is still not linked or booted.

## Completed milestones

- Software GBA memory bus plus initial BIOS-compatible helpers, DMA and IRQ services, with host tests.
- Source adapter script that conditionally maps GBA memory/registers, DMA, interrupts, SRAM fast routines and the `AgbMain` loop to the host bridge. The script is regression-tested but does not mean every adapted engine file compiles.
- Optional `FE7_SOURCE_ROOT` CMake/Gradle wiring for the local FE7 decompilation.
- First authentic source slice: `util.c`, `text.c`, `face.c`, `unit.c`, `bm.c`, `map.c`, `sprite.c`, `icon.c`, `msg.c`, `debug_text.c`, and `banim_sprite.c` compile/link as a shared native object in the local smoke test and are bundled into `libfe7native.so` when the source root is provided.

## Next engineering sequence

1. Compile the adapted `src/main.c` as an isolated object and iteratively resolve type, memory, and BIOS/service incompatibilities without attempting a full link yet.
2. Bring over the smallest startup dependencies in dependency order: key state, SRAM/save compatibility, process manager, sprite system, RNG, and audio initialization. Replace unsupported ARM7TDMI service assembly with tested C equivalents or explicitly isolate the remaining assembly.
3. Build a testable engine lifecycle that calls `AgbMain` once and then `Fe7EngineRunFrame()` at a deterministic 59.7 Hz cadence; add synthetic tests for initialization failure, one-frame execution and shutdown.
4. Add GBA timing, VBlank/HBlank, timers and a software 240×160 PPU path. The current memory bus and DMA/IRQ services are incomplete, and direct reads/writes of IO registers are not yet full GBA hardware semantics.
5. Trace FE7's MP2K APIs, voice data and driver dependencies before connecting an Android PCM backend. Use TMC / Project Picori as an architectural reference only, not as a drop-in engine.
6. Connect the native engine path behind a clearly gated runtime switch, preserving the existing shell as a fallback until the native path is proved.

## Verification rule

The current opt-in source slice is only proof that authentic FE7 C data modules compile and are packaged; it does not call `AgbMain`, render a frame, play FE7 audio, or reach the title screen. Do not report a playable native port until an Android device boots the actual FE7 engine to its title screen. Keep ROMs and proprietary extracted assets out of Git and CI.
