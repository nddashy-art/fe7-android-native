# FE7 native-port implementation roadmap

The supplied `fe7_us-main.zip` has now been inspected and its archive checksum recorded in `docs/FE7_SOURCE_ARCHIVE_INVENTORY.md`. This branch is still an audit/planning branch: no FE7 gameplay source has been integrated into the Android runtime yet.

Next engineering sequence:

1. Inspect `include/gbaio.h`, `include/gbasvc.h`, `src/hardware.c`, `src/gbasvc.s`, `src/interrupt.c`, and `src/main.c` to define the minimal GBA platform boundary.
2. Create a host-testable portability target for the safe, hardware-independent utilities that can be separated without relying on GBA memory addresses or ARM7 assembly.
3. Add tests using synthetic inputs and no ROM, and wire this target into Android CMake as a separate library rather than replacing the existing launcher target.
4. Decide whether a direct decompilation port can reasonably implement all required hardware behavior or whether a reusable GBA compatibility layer is necessary. Keep that decision evidence-based and document each dependency.
5. Add the first actual engine code only after the boundary compiles, then iterate toward a real title-screen boot.

Do not claim the app is playable until a physical-device build boots the actual FE7 engine. Keep the ROM picker, and do not commit ROMs or extracted commercial assets.
