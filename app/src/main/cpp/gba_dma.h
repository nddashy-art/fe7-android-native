#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Called by the patched FE7 DmaSet macro. Control contains the original GBA
// DMA count in the low halfword and DMA control flags in the high halfword.
void fe7_gba_dma_set(unsigned channel, const void* source, void* destination, uint32_t control);

// Platform frame/scanline integration points. VBlankIntrWait calls the VBlank hook;
// the renderer must later call the HBlank hook for each visible scanline.
void fe7_gba_dma_on_vblank(void);
void fe7_gba_dma_on_hblank(unsigned scanline);

#ifdef __cplusplus
}
#endif
