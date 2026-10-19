# FE7 native-port implementation roadmap

The supplied FE7 US decompilation archive has been inventoried and its checksum recorded in `docs/FE7_SOURCE_ARCHIVE_INVENTORY.md`. The repository now has the software GBA runtime foundation and an opt-in Android build path that compiles and links 13 genuine, low-dependency FE7 C translation units. The actual gameplay engine is still not linked or booted.

## Completed milestones

- Software GBA memory bus plus initial BIOS-compatible helpers, DMA and IRQ services, with host tests.
- Source adapter script that conditionally maps GBA memory/registers, DMA, interrupts, SRAM fast routines and the `AgbMain` loop to the host bridge. The script is regression-tested but does not mean every adapted engine file compiles.
- Optional `FE7_SOURCE_ROOT` CMake/Gradle wiring for the local FE7 decompilation.
- First authentic source slice: 13 low-dependency FE7 C translation units compile/link into the native library with the local source root enabled. Five additional service/game translation units (`async_upload.c`, `audio.c`, `event.c`, `gbasram.c`, `interrupt.c`) and `main.c` compile as isolated objects.
- The C sound driver `m4a.c` also compiles to an isolated object for `armeabi-v7a` (but is not linked/executed). Eight original ARM assembly units compile as objects for the `armeabi-v7a` ABI only; these assembly objects are not yet linked or executed.

## Next engineering sequence

1. Inventory and reduce the full-engine link failures by grouping missing symbols into game functions, ROM-backed data, and GBA hardware services. A cross-link experiment already confirmed that the original assembly carries `R_ARM_ABS32` relocations (including references to game globals and `AgbMain`) that cannot be linked directly into an Android position-independent shared library. This must be fixed before the objects can be executed by Android.
2. Bring over the smallest startup dependencies in dependency order: key state, SRAM/save compatibility, process manager, sprite system, RNG, and audio initialization. Replace unsupported ARM7TDMI service assembly with tested C equivalents or explicitly isolate the remaining assembly.
3. Build a testable engine lifecycle that calls `AgbMain` once and then `Fe7EngineRunFrame()` at a deterministic 59.7 Hz cadence; add synthetic tests for initialization failure, one-frame execution and shutdown.
4. Extend the software 240×160 PPU beyond the initial text-BG/regular-OBJ modes: affine BG/OBJ, bitmap modes, windows, blending/mosaic and accurate per-scanline effects. The current memory bus and DMA/IRQ services are incomplete, and direct reads/writes of IO registers are not yet full GBA hardware semantics.
5. Add deterministic frame pacing, VBlank/HBlank and timer callbacks, then connect `fe7_gba_render_frame()` to a native-only frame path for isolated synthetic tests.
6. Trace FE7's MP2K APIs, voice data and driver dependencies before connecting an Android PCM backend. Use TMC / Project Picori as an architectural reference only, not as a drop-in engine.
7. Connect the native engine path behind a clearly gated runtime switch, preserving the existing shell as a fallback until the native path is proved.

## Verification rule

The current opt-in source slice is only proof that authentic FE7 C data modules compile and are packaged; it does not call `AgbMain`, render a frame, play FE7 audio, or reach the title screen. Do not report a playable native port until an Android device boots the actual FE7 engine to its title screen. Keep ROMs and proprietary extracted assets out of Git and CI.
