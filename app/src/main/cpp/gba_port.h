#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Connects user-selected ROM bytes to the portable bus without copying them.
// The caller retains ownership and must keep the ROM alive until detached.
int fe7_gba_attach_rom(const uint8_t* data, size_t size);
void fe7_gba_detach_rom(void);
void fe7_gba_reset(int clear_ewram);

// Resolve GBA virtual addresses to backing storage. The IO pointer preserves
// the lvalue semantics expected by FE7's REG(type, address) macro.
volatile void* fe7_gba_io_pointer(uint32_t address, size_t length);
void* fe7_gba_resolve_pointer(uintptr_t address_or_host_pointer,
                              size_t length, int write_access);

// FE7-compatible BIOS memory-service entry points. Native builds should use
// these instead of the ARM7TDMI assembly SWI wrappers.
void CpuSet(const void* source, void* destination, uint32_t control);
void CpuFastSet(const void* source, void* destination, uint32_t control);

#ifdef __cplusplus
}
#endif
