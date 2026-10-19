# FE7 Android native-port audit and next steps

## Purpose

The target is a native Android port of the FE7 decompilation, not an emulator front-end. The existing app is useful as an Android shell and CI baseline, but it does not yet execute FE7 engine code. Keep the ROM picker and the no-ROM-in-repository rule.

## Verified repository state

- app/src/main/cpp/CMakeLists.txt currently imports a prebuilt mGBA libretro core and links it into fe7native.
- docs/DECOMPILATION_AUDIT.md says the supplied fe7_us-main archive targets ARM7TDMI and directly uses GBA MMIO, DMA, interrupts, BIOS assumptions, and ARM assembly.
- docs/PORTING_PLAN.md correctly lists engine integration and title-screen boot as incomplete.
- The decompilation archive itself is not present in the public repository tree inspected for this audit. The existing audit references a previously supplied archive, so the next implementation pass must work from that exact source snapshot and record its revision/checksum without committing ROM data or proprietary extracted assets.

## Architecture decision to validate

A decompilation port is not achieved by compiling the original C files unchanged for Android. GBA-specific interfaces need explicit replacements. Before choosing a full hardware-compatibility layer or a more direct platform port, build a source inventory and a minimal vertical slice.

Suggested layers:

1. Game logic: portable decompiled C and game-facing APIs, kept separate from Android UI.
2. GBA compatibility: memory layout assumptions, BIOS helpers, interrupt/timer scheduling, DMA, save-memory behavior, and any remaining assembly. Each dependency should have a documented implementation or a deliberate exclusion.
3. Graphics: translate the game's tile/map/OAM/palette operations into a software-rendered 240x160 frame first; optimize only after correctness.
4. Audio: preserve FE7's MP2K song/voice data semantics. Separate sequence/control APIs from the output device, then render to PCM and resample for Android.
5. Input and timing: map Android touch/controller input into GBA key state and establish a deterministic frame/timer scheduler.
6. Android adapter: lifecycle, surface presentation, audio device, document picker, and app-private saves. No game rules should live in Kotlin UI code.

## What Project Picori / TMC can teach us

The TMC PC port is a useful architectural reference, not a drop-in FE7 engine. Its audio audit describes a split between the decompiled game's sound-facing API and a software MP2K implementation (agbplay_core) that reads song data from the ROM and outputs PCM. That suggests investigating a similar seam for FE7: keep FE7's game-facing sound calls stable while isolating synthesis and Android PCM output behind a narrow interface.

Do not copy TMC-specific song IDs, structs, offsets, asset tables, or game assumptions. FE7's tables and version/layout must be audited independently. Also do not assume the TMC software synth is automatically more accurate or compatible without comparing FE7 playback.

Reference: https://github.com/nddashy-art/tmc/blob/master/docs/audio-pipeline-audit-2026-05-30.md

## Ordered implementation milestones

### Milestone A — Source baseline and dependency inventory
- Locate the exact supplied FE7 source archive used by the previous audit.
- Record upstream origin/revision, source archive checksum, build commands, generated-file requirements, and which files are generated from game data.
- Categorize source files: portable game logic, GBA hardware/MMIO, BIOS, assembly, linker/startup, audio, graphics, input, saves.
- Identify whether required generated game data/assets already exist in the source snapshot. Never put a commercial ROM or proprietary extracted assets in public Git or CI artifacts.

### Milestone B — Portable build proof
- Create a separate native target for portable FE7 modules; do not remove the current Android shell while the new path is experimental.
- Compile one small, genuinely portable module and its tests with the Android NDK and a host compiler.
- Add compatibility interfaces rather than scattering Android-specific conditionals through game logic.
- Keep CI green without requiring a ROM.

### Milestone C — First vertical slice
- Implement the minimum startup/runtime services required to call the decompiled initialization safely.
- Start with deterministic timing and input state, then a software framebuffer path.
- Use synthetic fixtures for tests; user-selected game data stays on-device and is never uploaded to CI.
- Do not label an APK playable until it demonstrably reaches the FE7 title screen on a device.

### Milestone D — Audio and remaining runtime
- Trace FE7's exact MP2K API, sound tables, and driver dependencies.
- Prototype a narrow backend interface that can later feed Android PCM output.
- Use TMC's agbplay_core only as a reference candidate; verify license compatibility, FE7 data compatibility, and audio behavior before deciding whether to reuse or independently implement anything.
- Add graphics, save, pause/resume, and controller validation after the startup path is stable.

### Milestone E — Device verification
- Build and install the APK on a physical Android device.
- Verify title screen, input, frame pacing, audio, save persistence, and pause/resume separately.
- Keep emulator experiments and any native-port builds clearly labeled as different architectures.

## Immediate blocker

The public FE7 Android repository does not currently contain the decompilation source tree itself; it contains an audit referring to a source archive supplied earlier. Implementation beyond the Android shell needs that exact source tree available to the coding workflow. Do not silently substitute mGBA for the native-port goal.
