# FE7 Android Native (experimental)

An experimental effort to bring a **user-supplied, legally obtained Fire Emblem: The Blazing Blade (US)** build to Android, with touch controls, controller support, and eventually optional local-network multiplayer and mod support.

> **Current status: mGBA-backed Android gameplay prototype plus an incomplete native runtime.** The app selects a user-supplied `.gba` file and mGBA executes it. The experimental software GBA bus now attaches the same ROM buffer and has isolated host tests, and a real FE7 C source slice compiles in CI. However, the original FE7 engine entrypoint (`AgbMain`), CPU execution path, full timing, renderer and audio are not integrated; this is not yet a native FE7 port.

## Goals

- Native Android app suitable for lower-end devices.
- User selects their own ROM through Android's system document picker; no ROM is bundled.
- Touchscreen controls and physical game-controller support.
- Save data in app-private storage.
- A build process that can run in GitHub Actions, so a developer does not need a PC just to trigger a build.
- Later research: local-network co-op/versus modes and a documented mod/character extension system.

## Legal and source-content policy

- **Do not commit a commercial ROM, BIOS, copyrighted game assets, or extracted proprietary data to this repository or CI artifacts.**
- The app reads the selected ROM locally and passes it to the bundled mGBA core for gameplay; it does not upload the ROM. The software native runtime borrows the in-memory buffer for diagnostics and detaches before that buffer is released.
- This project is unofficial and is not affiliated with or endorsed by Nintendo.
- The presence of a decompilation does not itself make it an Android port; platform integration and testing remain necessary.

## Current state

- [x] Android launcher shell
- [x] Basic system file picker for user-supplied `.gba` files
- [x] Local remembered URI/name for a selected file
- [x] C++/JNI bridge and CMake integration for local header inspection
- [x] Diagnostic 240×160 ARGB frame generated in C++ and displayed in Android
- [x] Host-side synthetic tests for the portable header parser (runs in CI)
- [x] GitHub Actions workflow configured for Android SDK, NDK, and CMake
- [x] Verify the Android debug APK build in GitHub Actions (native FE7 gameplay remains unverified)
- [ ] Verify the frame-test screen on the user’s physical device
- [ ] Link and execute the actual FE7 game engine/decompilation against the selected ROM
- [ ] Boot to title screen on a real device
- [ ] Touch controls and controller mapping
- [ ] Persistent game saves and lifecycle handling
- [ ] Multiplayer design and implementation
- [ ] Mod/OC extension design and implementation

## Emulator and licenses

The Android build script pins mGBA to upstream commit `9660a668f2093cb4aba5983f52bebf8878dc5b8b` and builds the libretro core without bundling any ROM or BIOS. mGBA is licensed under MPL-2.0; its license and pinned commit are included in the app assets. The vendored `libretro.h` API header retains its separate MIT license notice.

## Development notes

See [PORTING_PLAN.md](docs/PORTING_PLAN.md) for the staged technical plan and [DECOMPILATION_AUDIT.md](docs/DECOMPILATION_AUDIT.md) for findings from the supplied source archive. The priority is to get a real, reproducible single-player boot first. Multiplayer and character mods depend on understanding and adapting the original game state and should not be represented as available before they work.

## Contributions

Please open an issue with device model, Android version, exact build logs, and steps to reproduce any failure. Do not attach ROMs or copyrighted game assets.


### Native engine link audit

The Android CI workflow now collects undefined-symbol and relocation inventories for the real FE7 compile-probe object files. The `fe7-native-link-audit` artifact is diagnostic evidence for the next integration step; it is not a successful engine link and does not mean the game is executing natively. See the [Android workflow](https://github.com/nddashy-art/fe7-android-native/actions/workflows/android-debug.yml).
