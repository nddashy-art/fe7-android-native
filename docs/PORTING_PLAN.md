# Porting plan

This document tracks the work needed to turn the GBA decompilation into an Android application. It is a plan, not a claim that these features already work.

## Stage 0 — Establish a clean baseline

- Record the upstream decompilation revision and its build requirements.
- Confirm which source files are portable C and which depend on ARM/GBA hardware or assembler.
- Keep ROMs, proprietary assets, generated game data, signing keys, and secrets out of Git.
- Decide on an Android rendering/audio/input backend after a proof-of-concept build.

**Exit criteria:** documented toolchain, reproducible source baseline, and a legal asset-input strategy.

## Stage 1 — Minimal Android shell

- Add an Android Gradle project and native build integration (NDK/CMake or the upstream-compatible toolchain).
- Add application lifecycle handling, orientation/aspect-ratio handling, audio output, and app-private save paths.
- Add a diagnostic screen or small native test before integrating game code.

**Exit criteria:** debug APK builds in CI and installs/launches on a real Android device.

## Stage 2 — Port the game code

- Build a dependency inventory from the decompilation's Makefile and linker configuration.
- Isolate hardware-specific code (GBA registers, DMA, interrupts, BIOS calls, ARM assembly) behind portable interfaces or replace it with Android-compatible equivalents.
- Resolve compiler/linker issues incrementally and keep patches small.
- Load only user-supplied game data where required; never package commercial ROM data in the repository or CI artifacts.

**Exit criteria:** reproducible APK boots into the title screen and can start a chapter on a physical device.

## Stage 3 — Controls, saves, and low-end performance

- Add multitouch buttons and D-pad with adjustable layout/opacity.
- Map Android game controllers and keyboard input to the same abstract controls.
- Store saves/settings in app-private storage and handle pause/resume safely.
- Profile memory, startup time, frame pacing, and battery use on lower-end hardware.

**Exit criteria:** controls and saves work across app restarts; no blocking crashes in basic playtesting.

## Stage 4 — Multiplayer research

FE7 is a single-player GBA game; co-op and versus are not built-in modes. First document what each mode means and prototype a protocol outside the game loop. A robust approach may require deterministic turn synchronization, state serialization, host migration/reconnect behavior, and handling divergent RNG/state. Avoid promising netplay until the original game state can be synchronized reliably.

Potential first prototype: pass-and-play or shared tactical-map experiment, followed by local-network turn synchronization. Keep multiplayer opt-in and separate from normal campaign saves.

**Exit criteria:** written mode specification, repeatable two-device test, and explicit handling for disconnects/desyncs.

## Stage 5 — Mod and original-character support

- First map the game's existing data tables and event system.
- Prefer data-driven, validated mod packages over arbitrary native code execution.
- Define a versioned manifest for character stats, portraits supplied by the user, class/weapon references, dialogue/event hooks, and compatibility checks.
- Validate bounds and references before loading a mod; keep original campaign data untouched.

**Exit criteria:** documented schema and a test mod that loads without modifying the base game files.

## CI strategy

GitHub Actions can compile source code, but it cannot supply proprietary game data. The first workflow should build and test the Android shell without any ROM. A later private/local build path may accept a user-provided file without uploading it to public artifacts. Any APK artifact must be described accurately as a shell/test build or a playable build according to its verified state.

## Current blockers

- The actual decompilation source tree and its build configuration must be reviewed before selecting the final native integration.
- No Android project, Android build, or on-device test has been completed yet.
- Multiplayer and modding are future milestones, not existing functionality.
