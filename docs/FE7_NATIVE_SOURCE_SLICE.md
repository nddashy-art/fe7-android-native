# FE7 native-source slice — milestone 1

This adds the first build path that compiles and links translation units from the supplied FE7 decompilation into the Android native library. It is intentionally small and does **not** imply that the game engine boots yet.

## Included source units

The optional `fe7_engine_data` object target builds these original source files from a local `fe7_us-main` tree:

- `util.c`, `text.c`, `face.c`, `unit.c`, `bm.c`, `map.c`
- `sprite.c`, `icon.c`, `msg.c`, `debug_text.c`, `banim_sprite.c`

They define engine-facing global state and data tables; none needs a baserom to compile. The object files are linked into `libfe7native.so`, rather than merely being syntax-checked in a disconnected project.

## Build from a local source copy

Keep your own source archive out of Git. Extract it somewhere local, then preferably copy it before applying the adapters because the patcher edits source/header files and makes `.fe7native.bak` backups.

```sh
unzip fe7_us-main.zip -d /tmp/fe7-source
cp -a /tmp/fe7-source/fe7_us-main /tmp/fe7-us-native
python3 scripts/test_fe7_native_data_slice.py /tmp/fe7-us-native
python3 scripts/prepare_fe7_native_source.py /tmp/fe7-us-native
./gradlew assembleDebug -Pfe7SourceRoot=/tmp/fe7-us-native
```

Use an absolute path for `fe7SourceRoot`. The smoke-test script compiles the same 11 C files with the host compiler and verifies they can be linked into a shared object. Android CMake then compiles them with the NDK and adds their objects to the APK's native library.

## What this milestone proves — and does not prove

It proves a reproducible route for building a real, ROM-independent slice of FE7's own C source and packaging it with the Android native library. It does **not** call `AgbMain`, link the full event/game logic or ARM7TDMI assembly, render the GBA PPU, run game audio, or reach the title screen. The app's current play path remains mGBA/libretro until the complete engine and its platform dependencies are ported and verified.

Next work: compile the adapted startup translation unit, resolve its BIOS/service dependencies, and build a deterministic VBlank/frame scheduler before wiring it into Android's render loop. Never package a ROM or proprietary game assets in CI.
