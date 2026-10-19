# FE7 source archive inventory — first pass

## Input archive

- Library file: `fe7_us-main.zip`
- Archive SHA-256: `6b8682699ed8007ee2a0c3ae05fd53ca4e79106941eb099d3011585d629bee37`
- Archive size: 1,215,317 bytes
- Extracted file listing reports 83 entries and 8,399,140 bytes uncompressed.
- The archive contains source/decompilation files, but no `baserom.gba` is included. The README says a user-provided FE7 US dump is required for the original ROM-rebuild comparison target.

## Source layout observed

- `src/`: 20 C files plus GBA assembly/runtime files.
- `asm/`: decompiled game assembly and data/event assembly.
- `include/`: GBA hardware, BIOS/service, video, SRAM, audio, and game headers.
- `data/`: includes `fe6_link_loader.bin` (a small support binary, not a game ROM).
- `tools/`: agbcc installation and project configuration.
- `pending/`: one pending C translation unit.

## Build assumptions

The Makefile is specifically a GBA ROM build:

- Uses `arm-none-eabi-*` binutils and the old `tools/agbcc/bin/old_agbcc` compiler.
- Compiles with `-mthumb-interwork`, targets ARM7TDMI assembly (`-mcpu=arm7tdmi`), and links through `fe7_us.lds`.
- Produces `fe7_us.gba` and checks its SHA-1 against `fe7_us.sha1`.
- This build recipe is not an Android NDK build recipe; Android integration needs a separate target and replacements for GBA-only services.

## Concrete startup dependencies found

`src/main.c:AgbMain` performs GBA memory initialization, IRQ setup, key initialization, RAM-function setup, SRAM initialization, process/sprite initialization, music initialization, RNG initialization, and `m4aSoundInit()`, then enters the game's runtime. `src/interrupt.c` copies the interrupt handler into IWRAM and writes the interrupt vector. These operations cannot be called directly as ordinary Android C functions without a compatible GBA runtime model.

A first scan of `src/` and `include/` finds references to DMA registers, WAITCNT, VCOUNT, sound registers, BIOS calls such as `CpuSet`/`CpuFastSet` and `VBlankIntrWait`, and LZ77 decompression. That confirms the port boundary must include memory/register services, timing/interrupts, graphics, audio, input, and save handling.

## Recommended next implementation step

Do not bulk-add all `src/*.c` files to Android CMake yet. First define a small platform abstraction for frame timing, key state, decompression/copy helpers, display output, audio output, and save storage. Then choose the smallest startup slice that can be compiled/tested without the GBA linker script or ARM7TDMI assembly. Preserve the existing Android shell in parallel until a real title-screen boot is verified.

## TMC / Project Picori reference

The TMC audio audit is useful for considering a software MP2K backend behind FE7's game-facing sound API. It does not remove the need to audit FE7's own `m4a` driver, sound tables, instrument data, calling conventions, or license compatibility before reusing any code.

Reference: https://github.com/nddashy-art/tmc/blob/master/docs/audio-pipeline-audit-2026-05-30.md
