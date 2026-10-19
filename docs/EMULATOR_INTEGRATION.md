# Emulator integration decision: first playable boot

This document records the proposed route from the working Android shell to the first actual FE7 title-screen boot. It is an engineering decision record, not a claim that emulation has been integrated.

## Why the decompilation is not the first runtime

The supplied FE7 decompilation targets GBA hardware and its ARM7TDMI environment. Making its C/assembly compile for Android does not provide the GBA CPU, memory map, video, audio, timers, interrupts, DMA, cartridge-save behavior, or BIOS services. Reimplementing that hardware surface would be a substantial emulator project in itself.

## Proposed first milestone: embed an existing GBA core

Evaluate embedding the mGBA core behind a narrow JNI boundary, rather than attempting to port all FE7-specific code and GBA hardware assumptions at once. mGBA is an established GBA emulator project and is distributed under the Mozilla Public License 2.0; review the exact upstream version and all bundled third-party licenses before copying or linking code.

Upstream references:
- Project and source: https://github.com/mgba-emu/mgba
- Feature/compatibility overview: https://mgba.io/faq.html
- Libretro core documentation: https://docs.libretro.com/library/mgba/

This is a proposed technical direction, not a promise that mGBA can simply be dropped into the current CMake target. Android packaging, dependencies, APIs, and licensing still need to be verified in a small proof of concept.

## Smallest useful proof of concept

1. Pin an upstream release/commit and record the license and third-party notices.
2. Build a minimal Android-compatible core for the project's supported ABIs in CI, without including any ROM or BIOS.
3. Define a JNI lifecycle API: initialize, load user-selected content, run one frame, read video pixels, submit button state, and close cleanly.
4. Start with a test pattern or generated frame to validate pixel-buffer transfer and surface rendering before loading game content.
5. Add a frame loop and pause/resume handling; keep emulator work off the Android UI thread.
6. Only after the generated-frame path is stable, test the user's locally selected ROM and confirm the actual FE7 title screen on a device.
7. Keep ROM data and saves out of GitHub Actions logs and artifacts. If a CI integration test needs game content, use synthetic fixtures only.

## Architecture boundaries

- Kotlin/Android: document picker, lifecycle, screen/surface, touch and controller input, app-private save location.
- JNI adapter: validates arguments, owns native core lifecycle, exposes bounded operations, reports errors without crashing the process.
- GBA core: CPU and hardware emulation, cartridge reads, frame generation, audio and save-memory behavior.
- FE7-specific decompilation: a later optional path for game-aware extensions/mods, only after its data formats and licensing boundaries are understood.

## Not part of the first milestone

- Local co-op/versus or network play.
- Character/portrait/voice/dialogue mod loading.
- Patching the ROM or injecting arbitrary native code.
- Claiming full save compatibility, speed, or correctness before testing on the target device.

## Exit criteria

The emulator milestone is complete only when a CI-built APK installs, loads user-selected content locally, renders the actual FE7 title screen on a physical Android device, responds to at least one mapped input, and closes/reopens without a blocking crash. A successful native-library build alone does not meet these criteria.
