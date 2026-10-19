# GBA memory bus foundation

This change takes architectural guidance from Project Picori's platform port, especially its separate GBA memory backing arrays and address resolver in [port/port_gba_mem.c](https://github.com/nddashy-art/tmc/blob/master/port/port_gba_mem.c) and [port/port_gba_mem.h](https://github.com/nddashy-art/tmc/blob/master/port/port_gba_mem.h). FE7 has a different source tree and uses fixed-address macros in `include/gbaio.h`, so this is an independently authored host-side layer rather than a drop-in copy of Picori's code.

## What is implemented

- Separate byte-backed regions for EWRAM, IWRAM, I/O registers, palette RAM, VRAM, OAM and battery-backed SRAM.
- A borrowed ROM view at `0x08000000–0x0DFFFFFF`, with simple modulo mirroring to support common cartridge ROM mappings.
- Range-checked read and write mapping; writes to ROM are rejected.
- Little-endian 8/16/32-bit access helpers, with ARM7TDMI-style alignment behavior for halfword and word accesses.
- GBA-like byte-write behavior for palette/VRAM and ignored byte writes to OAM.
- Volatile-memory reset that can preserve EWRAM and always preserves SRAM/ROM.

## Deliberate limits

This is not a complete GBA emulator and cannot boot FE7 by itself. The I/O region is currently passive storage; write-one-to-clear status registers, DMA activation, timer progression, IRQ dispatch, display timing, and video composition are not yet modeled. The existing FE7 `REG(type, addr)` macro still dereferences literal hardware addresses, so a later port boundary must route those accesses into the bus or replace those macros and relevant assembly. Do not connect the full game engine until those paths are accounted for.

The native app's ROM picker remains user-supplied. This layer does not bundle ROM data.


## Native BIOS services

The Android native library now has a C ABI replacement layer for commonly used BIOS entry points: `CpuSet`, `CpuFastSet`, `LZ77UnComp8bit`, `LZ77UnComp16bit`, `Div`, `DivRem`, `Sqrt`, `ArcTan2`, `BgAffineSet`, `ObjAffineSet`, `RegisterRamReset`, `SoftReset`, `VBlankIntrWait`, `SoundBiasReset` and `SoundBiasSet`. VBlank and reset callbacks let the Android host eventually schedule the game and re-enter initialization. In isolated host tests, VBlank defaults to a 16.7 ms wait if no host callback is registered.

This remains a partial BIOS layer: Huffman/RL decoding, the full IRQ/timer/DMA model, register side effects, audio generation, and replacing all ARM7TDMI assembly sources are still open. The tests verify helper-level behavior only; they do not indicate the game boots.


## Software DMA bridge (initial)

Under `FE7_NATIVE_PORT`, the source adapter replaces the `DmaSet` register-write macro with `fe7_gba_dma_set`. That preserves the callsites (`DmaCopy16/32`, `DmaFill16/32`, etc.) while making immediate DMA perform a bounded software copy/fill through the virtual memory bus. The module also stores VBlank- and HBlank-triggered channel state and exposes `fe7_gba_dma_on_vblank` and `fe7_gba_dma_on_hblank` for the runtime/renderer. `VBlankIntrWait` invokes the VBlank DMA hook; HBlank DMA still requires the not-yet-integrated per-scanline renderer to call its hook. Audio/FIFO special DMA, channel-specific zero-count transfer lengths, DMA IRQ signaling, and exact I/O register readback remain unimplemented.


## Native interrupt controller (initial)

The port layer now has a software IRQ dispatcher with GBA IE/IF/IME masks, 14 callback slots, write-one-to-clear behavior for pending flags, and host replacements for IntrWait/Halt/Stop. VBlank wait advances deferred VBlank DMA and raises IRQ bit 0. The source adapter conditionally replaces src/interrupt.c's ARM7TDMI IntrMain copy path with a C dispatcher and native callback table.

This is an integration foundation, not a complete timing model: timers are not advanced, HBlank IRQ/DMA is not driven by a scanline renderer yet, DMA IRQ flags are not raised, and other functions in src/crt0.s and src/gbasvc.s remain assembly dependencies. The app currently has no call site that starts AgbMain.
