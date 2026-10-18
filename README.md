# FE7 Android Native (experimental)

An experimental effort to bring a **user-supplied, legally obtained Fire Emblem: The Blazing Blade (US)** build to Android, with touch controls, controller support, and eventually optional local-network multiplayer and mod support.

> **Current status: launcher and native-diagnostics prototype only.** The app selects and remembers a user-supplied `.gba` file and passes its header bytes to a small C++/JNI library. The original FE7 engine is not integrated, so the ROM is not executed and this is not a playable port.

## Goals

- Native Android app suitable for lower-end devices.
- User selects their own ROM through Android's system document picker; no ROM is bundled.
- Touchscreen controls and physical game-controller support.
- Save data in app-private storage.
- A build process that can run in GitHub Actions, so a developer does not need a PC just to trigger a build.
- Later research: local-network co-op/versus modes and a documented mod/character extension system.

## Legal and source-content policy

- **Do not commit a commercial ROM, BIOS, copyrighted game assets, or extracted proprietary data to this repository or CI artifacts.**
- The app's ROM picker remembers a document URI on the device. The current prototype does not upload the selected file or pass it to a game engine.
- This project is unofficial and is not affiliated with or endorsed by Nintendo.
- The presence of a decompilation does not itself make it an Android port; platform integration and testing remain necessary.

## Current state

- [x] Android launcher shell
- [x] Basic system file picker for user-supplied `.gba` files
- [x] Local remembered URI/name for a selected file
- [x] C++/JNI bridge and CMake integration for local header inspection
- [x] Host-side synthetic tests for the portable header parser (runs in CI)
- [x] GitHub Actions workflow configured for Android SDK, NDK, and CMake
- [ ] Verify the updated native APK build in CI and on the user's device
- [ ] Integrate the actual game engine/decompilation with the selected ROM
- [ ] Boot to title screen on a real device
- [ ] Touch controls and controller mapping
- [ ] Persistent game saves and lifecycle handling
- [ ] Multiplayer design and implementation
- [ ] Mod/OC extension design and implementation

## Development notes

See [PORTING_PLAN.md](docs/PORTING_PLAN.md) for the staged technical plan and [DECOMPILATION_AUDIT.md](docs/DECOMPILATION_AUDIT.md) for findings from the supplied source archive. The priority is to get a real, reproducible single-player boot first. Multiplayer and character mods depend on understanding and adapting the original game state and should not be represented as available before they work.

## Contributions

Please open an issue with device model, Android version, exact build logs, and steps to reproduce any failure. Do not attach ROMs or copyrighted game assets.
