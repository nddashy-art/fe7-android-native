# FE7 native-source slice — milestone 1

This adds the first build path that compiles and links translation units from the supplied FE7 decompilation into the Android native library. It is intentionally small and does **not** imply that the game engine boots yet.

## Included source units

The optional `fe7_engine_data` object target builds these original source files from a local `fe7_us-main` tree:

- `util.c`, `text.c`, `face.c`, `hardware.c`, `proc.c`, `unit.c`, `bm.c`, `map.c`
- `sprite.c`, `icon.c`, `msg.c`, `debug_text.c`, `banim_sprite.c`

They define engine-facing global state and data tables; none needs a baserom to compile. The object files are linked into `libfe7native.so`, rather than merely being syntax-checked in a disconnected project.

## Build from a local source copy

Keep your own source archive out of Git. Extract it somewhere local, then preferably copy it before applying the adapters because the patcher edits source/header files and makes `.fe7native.bak` backups.

```sh
unzip fe7_us-main.zip -d /tmp/fe7-source
cp -a /tmp/fe7-source/fe7_us-main /tmp/fe7-us-native
python3 scripts/prepare_fe7_native_source.py /tmp/fe7-us-native
python3 scripts/test_fe7_native_data_slice.py /tmp/fe7-us-native
./gradlew assembleDebug -Pfe7SourceRoot=/tmp/fe7-us-native
```

Use an absolute path for `fe7SourceRoot`. The smoke-test script compiles the 13 data-only C files with the host compiler, verifies they link into a shared object, and separately compiles adapted `src/main.c` to an object. Android CMake repeats these steps with the NDK, adds the data objects to the APK's native library, and keeps the startup object out of the link until its dependencies are ported.

For every ABI, CMake also compiles five additional FE7 service/game translation units as object-only probes: `async_upload.c`, `audio.c`, `event.c`, `gbasram.c`, and `interrupt.c`. They are intentionally not linked yet because unresolved dependencies and hardware effects need to be mapped first.

For `armeabi-v7a` only, CMake also invokes an assembly compile probe for eight original units: `asm/code.s` (through a generated normalized copy), `asm/code_1.s`, `asm/event.s`, `asm/fake_glue.s`, `src/armfunc.s`, `src/crt0.s`, `src/gbasvc.s`, and `src/sio_link_loader.s`. The probe produces object files but deliberately does not link or execute them. The source normalizer removes one adjacent duplicate local-label declaration from a generated copy of `code.s`, without editing the user’s source tree. ARM64 cannot consume these ARMv7 objects.

## What this milestone proves — and does not prove

It proves a reproducible route for building a real, ROM-independent slice of FE7's own C source and packaging it with the Android native library. It does **not** link or call `AgbMain`, link the probed service/ARM7TDMI assembly objects, render the GBA PPU, run game audio, or reach the title screen. Compiling `main.c` alone proves source-level compatibility for that translation unit only. The app's current play path remains mGBA/libretro until the complete engine and its platform dependencies are ported and verified.

Remaining assembly and boot blockers: `asm/data.s` and `asm/rodata.s` include data from `baserom.gba`, which is intentionally not in the source archive or repository. `src/m4a_1.s` also still needs assembler syntax/backend work. The currently compiled assembly objects are not linked because the absolute GBA address map and unresolved game data/services are not yet compatible with the Android runtime.

Next work: compile/link the adapted startup dependencies in order, replace or bridge any remaining GBA-address assumptions, then implement deterministic VBlank/frame timing and a software renderer before wiring it into Android's render loop. Never package a ROM or proprietary game assets in CI.
