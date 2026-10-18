# FE7 Android Native (experimental)

An experimental effort to bring a **user-supplied, legally obtained Fire Emblem: The Blazing Blade (US)** build to Android, with touch controls, controller support, and eventually optional local-network multiplayer and mod support.

> **Current status: Android launcher prototype only.** The app has a screen to select and remember a user-supplied `.gba` file, but the game engine is not integrated yet and the ROM is not executed. Do not treat this as a playable port.

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
- [x] GitHub Actions workflow to build a prototype debug APK
- [ ] Integrate the actual game engine/decompilation with the selected ROM
- [ ] Boot to title screen on a real device
- [ ] Touch controls and controller mapping
- [ ] Persistent game saves and lifecycle handling
- [ ] Multiplayer design and implementation
- [ ] Mod/OC extension design and implementation

## Development notes

See [PORTING_PLAN.md](docs/PORTING_PLAN.md) for the staged technical plan. The priority is to get a minimal, reproducible single-player build first. Multiplayer and character mods depend on understanding and adapting the original game state and should not be represented as available before they work.

## Contributions

Please open an issue with device model, Android version, exact build logs, and steps to reproduce any failure. Do not attach ROMs or copyrighted game assets.
