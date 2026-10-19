# FE7 native-port implementation roadmap

The supplied `fe7_us-main.zip` has now been inspected and its archive checksum recorded in `docs/FE7_SOURCE_ARCHIVE_INVENTORY.md`. The branch has a software GBA runtime foundation and source adapters, but no FE7 gameplay source has yet been compiled into the Android runtime.

Next engineering sequence:

1. Inspect `include/gbaio.h`, `include/gbasvc.h`, `src/hardware.c`, `src/gbasvc.s`, `src/interrupt.c`, and `src/main.c` to define the minimal GBA platform boundary. (Done for the first pass.)
2. Create a host-testable portability target for the safe, hardware-independent utilities that can be separated without relying on GBA memory addresses or ARM7 assembly.
3. Add tests using synthetic inputs and no ROM, and wire this target into Android CMake as a separate library rather than replacing the existing launcher target.
4. Decide whether a direct decompilation port can reasonably implement all required hardware behavior or whether a reusable GBA compatibility layer is necessary. Keep that decision evidence-based and document each dependency.
5. The source-preparation script now conditionally splits `AgbMain` into one-time initialization and `Fe7EngineRunFrame()` under `FE7_NATIVE_PORT`; this lets an Android host own frame pacing instead of trapping inside FE7's infinite loop. It is still an adapter, not proof that `main.c` or the full engine compiles on Android.
6. Next, port or replace the ARM-only BIOS/service assembly and map graphics, timers, input, SRAM, and audio to the software GBA runtime. Then compile/link the adapted FE7 source and call `AgbMain` once plus `Fe7EngineRunFrame()` on the native engine thread. Only after that can a real title-screen boot be tested.

Do not claim the app is playable until a physical-device build boots the actual FE7 engine. Keep the ROM picker, and do not commit ROMs or extracted commercial assets.
