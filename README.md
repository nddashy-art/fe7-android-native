# FE7 Android Native (experimental)

An experimental effort to bring a **user-supplied, legally obtained Fire Emblem: The Blazing Blade (US)** build to Android, with touch controls, controller support, and eventually optional local-network multiplayer and mod support.

> **Status: planning/scaffolding only. This repository does not contain a working port or an APK yet.** The original decompilation is GBA-oriented C and ARM assembly, not an Android app. A real port requires a compatible build system, platform-specific code changes, testing, and a user-provided game image/assets where required.

## Goals

- Native Android app suitable for lower-end devices.
- Touchscreen controls and physical game-controller support.
- Save data in app-private storage.
- A build process that can run in GitHub Actions, so a developer does not need a PC just to trigger a build.
- Later research: local-network co-op/versus modes and a documented mod/character extension system.

## Legal and source-content policy

- **Do not commit a commercial ROM, BIOS, copyrighted game assets, or extracted proprietary data to this repository or CI artifacts.**
- Supply any required game image only through a local/private workflow intended for that purpose.
- This project is unofficial and is not affiliated with or endorsed by Nintendo.
- The presence of a decompilation does not itself make it an Android port; platform integration and testing remain necessary.

## Current state

- [ ] Android project and native build integration
- [ ] Port/build the decompilation for Android
- [ ] Boot to title screen on a real device
- [ ] Touch controls and controller mapping
- [ ] Persistent saves and lifecycle handling
- [ ] Reproducible CI build and downloadable debug APK
- [ ] Multiplayer design and implementation
- [ ] Mod/OC extension design and implementation

## Development notes

See [PORTING_PLAN.md](docs/PORTING_PLAN.md) for the staged technical plan. The priority is to get a minimal, reproducible single-player Android build first. Multiplayer and character mods depend on understanding and adapting the original game state and should not be represented as available before they work.

## Contributions

Please open an issue with device model, Android version, exact build logs, and steps to reproduce any failure. Do not attach ROMs or copyrighted game assets.
