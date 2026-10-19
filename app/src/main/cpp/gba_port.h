#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int fe7_gba_attach_rom(const uint8_t* data, size_t size);
void fe7_gba_detach_rom(void);
void fe7_gba_reset(int clear_ewram);
int fe7_gba_render_frame(uint32_t* output_rgba, size_t pixel_count);
void fe7_gba_set_vblank_callback(void (*callback)(void));
void fe7_gba_set_soft_reset_callback(void (*callback)(void));
volatile void* fe7_gba_io_pointer(uint32_t address, size_t length);
void* fe7_gba_resolve_pointer(uintptr_t address_or_host_pointer, size_t length, int write_access);
void fe7_gba_dma_set(unsigned channel, const void* source, void* destination, uint32_t control);
void fe7_gba_dma_on_vblank(void);
void fe7_gba_dma_on_hblank(unsigned scanline);
void fe7_gba_set_irq_handler(unsigned index, void (*handler)(void));
void fe7_gba_irq_clear(uint16_t flags);
void fe7_gba_irq_raise(uint16_t flags);
void fe7_gba_dispatch_pending_irqs(void);
void IntrWait(int8_t wait_next, uint16_t intr_flags);
void Halt(void);
void Stop(void);
void** fe7_gba_interrupt_vector_slot(void);

struct BgAffineSetSrc;
struct BgAffineSetDst;
struct ObjAffineSetSrc;
struct MultiBootInfo;

void CpuSet(const void* source, void* destination, uint32_t control);
void CpuFastSet(const void* source, void* destination, uint32_t control);
void LZ77UnComp8bit(const void* source, void* destination);
void LZ77UnComp16bit(const void* source, void* destination);
int32_t Div(int32_t numerator, int32_t denominator);
int32_t DivRem(int32_t numerator, int32_t denominator);
uint16_t Sqrt(uint32_t value);
uint16_t ArcTan2(int16_t x, int16_t y);
void BgAffineSet(const struct BgAffineSetSrc* source, struct BgAffineSetDst* destination, int32_t count);
void ObjAffineSet(const struct ObjAffineSetSrc* source, void* destination, int32_t count, int32_t offset);
void RegisterRamReset(uint32_t flags);
void SoftReset(uint32_t flags);
void VBlankIntrWait(void);
void SoundBiasReset(void);
void SoundBiasSet(void);
void MultiBoot(const struct MultiBootInfo* info);

#ifdef __cplusplus
}
#endif

#ifndef FE7_GBA_VRAM_PTR
#define FE7_GBA_VRAM_PTR ((void*)fe7_gba_resolve_pointer(0x06000000u, 0x18000u, 1))
#define FE7_GBA_PLTT_PTR ((void*)fe7_gba_resolve_pointer(0x05000000u, 0x400u, 1))
#define FE7_GBA_OAM_PTR ((void*)fe7_gba_resolve_pointer(0x07000000u, 0x400u, 1))
#endif
