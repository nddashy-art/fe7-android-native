# FE7 native-port implementation roadmap

The supplied FE7 US decompilation archive has been inventoried and its checksum recorded in `docs/FE7_SOURCE_ARCHIVE_INVENTORY.md`. The repository now has the software GBA runtime foundation and an opt-in Android build path that compiles and links 13 genuine, low-dependency FE7 C translation units. The actual gameplay engine is still not linked or booted.

## Completed milestones

- Software GBA memory bus plus initial BIOS-compatible helpers, DMA and IRQ services, with host tests.
- Source adapter script that conditionally maps GBA memory/registers, DMA, interrupts, SRAM fast routines and the `AgbMain` loop to the host bridge. The script is regression-tested but does not mean every adapted engine file compiles.
- Optional `FE7_SOURCE_ROOT` CMake/Gradle wiring for the local FE7 decompilation.
- Android JNI now attaches the exact ROM buffer used by the mGBA frontend to the experimental native GBA memory bus for preparation/diagnostics, and detaches it before releasing the buffer. Host tests assert the mapping and shutdown behavior.
- First authentic source slice: 13 low-dependency FE7 C translation units compile/link into the native library with the local source root enabled. Five additional service/game translation units (`async_upload.c`, `audio.c`, `event.c`, `gbasram.c`, `interrupt.c`) and `main.c` compile as isolated objects.
- The C sound driver `m4a.c` also compiles to an isolated object for `armeabi-v7a` (but is not linked/executed). Eight original ARM assembly units compile as objects for the `armeabi-v7a` ABI only; these assembly objects are not yet linked or executed.

## Next engineering sequence

1. Inventory and reduce the full-engine link failures by grouping missing symbols into game functions, ROM-backed data, and GBA hardware services. A cross-link experiment already confirmed that the original assembly carries `R_ARM_ABS32` relocations (including references to game globals and `AgbMain`) that cannot be linked directly into an Android position-independent shared library. This must be fixed before the objects can be executed by Android.
2. Bring over the smallest startup dependencies in dependency order: key state, SRAM/save compatibility, process manager, sprite system, RNG, and audio initialization. Replace unsupported ARM7TDMI service assembly with tested C equivalents or explicitly isolate the remaining assembly.
3. Build a testable engine lifecycle that calls `AgbMain` once and then `Fe7EngineRunFrame()` at a deterministic 59.7 Hz cadence; add synthetic tests for initialization failure, one-frame execution and shutdown.
4. Extend the software 240×160 PPU beyond text BGs: affine BG2/BG3 rendering in modes 1/2 and affine OBJ matrix transforms (including double-size bounds) are now implemented with host tests. Next are bitmap modes, windows, blending/mosaic and accurate per-scanline effects. The memory bus and DMA/IRQ services remain incomplete, and direct IO-register accesses are not yet full GBA hardware semantics.
5. Add deterministic frame pacing, VBlank/HBlank and timer callbacks, then connect `fe7_gba_render_frame()` to a native-only frame path for isolated synthetic tests.
6. Trace FE7's MP2K APIs, voice data and driver dependencies before connecting an Android PCM backend. Use TMC / Project Picori as an architectural reference only, not as a drop-in engine.
7. Connect the native engine path behind a clearly gated runtime switch, preserving the existing shell as a fallback until the native path is proved.

## Verification rule

The current opt-in source slice is only proof that authentic FE7 C data modules compile and are packaged; it does not call `AgbMain`, render a frame, play FE7 audio, or reach the title screen. Do not report a playable native port until an Android device boots the actual FE7 engine to its title screen. Keep ROMs and proprietary extracted assets out of Git and CI.

## Startup trace from the supplied source archive (2026-10-19)

A closer read of the actual `fe7_us-main/src/main.c` shows that the startup boundary is deeper than a simple `AgbMain` wrapper. Before reaching the host-owned frame loop, `AgbMain` calls `ClearEwram`, programs `REG_WAITCNT`, initializes IRQ/VBlank and key state, calls `InitRamFuncs`, `SramInit`, `InitProcs`, `InitSpriteAnims`, `InitMus`, seeds two RNG paths, calls `func_0809F924`, `m4aSoundInit`, `func_08003F6C`, sets `OnVBlank`, sets language, and calls `StartGame()`. Only after that does the original infinite loop begin.

The archive's public headers declare `StartGame` and multiple unknown startup routines, but the supplied source slice does not define them as ordinary C functions; some are in the game's assembly/data units. Therefore compiling `main.c` alone (or adapting its infinite loop) is not evidence that startup can execute. A valid native boot needs the real implementations/data for this chain plus host replacements for the GBA hardware/BIOS operations it reaches.

**Current result:** this trace narrows the next work item, but does not change the status of the APK. The shipped gameplay route still uses mGBA. Do not label or distribute it as a native FE7 engine build until an actual native startup path links and reaches a title-screen frame.



## Runtime update (2026-10-19)

- Added affine BG2/BG3 rendering for GBA display modes 1 and 2. It reads the signed 8.8 affine matrix and signed 28-bit 20.8 reference registers, supports non-wrapping transparent out-of-bounds pixels and the BG wrap flag, and uses 8bpp affine tile maps. Host tests cover identity transform, out-of-range behavior, and wraparound.
- Added affine OBJ transform support using OAM PA/PB/PC/PD matrix entries, including double-size bounds, 1D/2D tile addressing, palette transparency and OBJ priority. Blend and OBJ-window modes are still skipped; host tests cover identity and scale/double-size behavior.
- Added a public KEYINPUT bridge that writes GBA's active-low, 10-bit key register. Android/libretro button bits are mapped to the GBA hardware order (A/B/Select/Start/Right/Left/Up/Down/R/L) while retaining the original bits for mGBA.
- Both changes are runtime groundwork, not proof of a playable native engine. `AgbMain` is still not linked/called by the Android gameplay path, and the title-screen boot criterion remains unmet.


## Android CI source-root verification (2026-10-19)

The Android debug workflow now checks out the upstream FE7 US source at pinned commit `5c8b3c5e254e00f0f3458b9772c27e1a5a326e3b`, runs the compatibility adapter, and builds with `FE7_SOURCE_ROOT` enabled. This compiles and links the selected C source slice into the shared library and compiles the isolated startup/service/ARMv7 probes on CI. The workflow succeeded on run `39239326207` and uploaded a debug APK artifact. This is materially stronger source-build coverage, but the app's gameplay route still calls mGBA; CI still does not prove `AgbMain` or `StartGame` can run natively.


## Native ROM-buffer lifecycle integration (2026-10-19)

The Android game-start JNI now validates and attaches the selected FE7 ROM to the native software GBA bus, while mGBA continues to execute the game. The runtime borrows the frontend's ROM vector without copying it and detaches before that vector is cleared. Host tests verify pointer identity and that ROM reads are no longer mapped after shutdown. This is an integration milestone only; no FE7 CPU or `AgbMain` execution is implied.


## Automated object-level link audit (2026-10-20)

Added `scripts/audit_fe7_native_objects.py` and an Android CI step that records per-object undefined symbols and relocation types for the actual FE7 source/startup/service/ARM7 probe objects. The generated `fe7-native-link-audit` artifact is intended to classify the next blockers (missing game functions/data versus GBA services and position-dependent relocations) before attempting a full shared-library link. It does not resolve those blockers by itself and is not a native boot test. The audit step must pass in CI and its report must be reviewed before choosing the next link changes.


## ABI-specific unresolved-symbol audit (2026-10-20)

The first audit run succeeded on Android CI run [39403611100](https://github.com/nddashy-art/fe7-android-native/actions/runs/39403611100) and audited 47 compile-probe objects. It found a large dependency surface in the isolated startup/service/assembly probes, including startup dependencies (`StartGame`, `InitProcs`, `InitKeySt`, RNG, IRQ and audio initialization), plus many event/map/render APIs and globals. The aggregate relocation inventory includes `R_AARCH64_ABS32` / `R_AARCH64_ABS64` on arm64 and `R_ARM_ABS32` on ARMv7. These findings justify inspecting relocation targets and source data layout before linking; relocation type counts alone do not prove every relocation is invalid.

The audit script now compares undefined references against global definitions in the audited object set **per Android ABI**, then groups remaining candidates into GBA bridge/runtime, compiler runtime, audio, game-data, and game/API buckets. This avoids falsely treating a symbol defined by another probe object—or a definition for a different ABI—as a resolved dependency. Symbols provided by `fe7runtime`, Android/system libraries, ROM-backed data, and source objects not yet included still require separate accounting. The audit remains diagnostic only and is not a native boot test.
