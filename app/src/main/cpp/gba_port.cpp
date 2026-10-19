#include "gba_port.h"

#include "gba_bios_compat.h"
#include "gba_memory.h"

#include <cstdint>
#include <limits>

namespace {
fe7::gba::MemoryBus g_memory;

void* ResolveLegacyPointer(const void* pointer, std::size_t length, bool write_access) {
    if (pointer == nullptr) return nullptr;
    const std::uintptr_t value = reinterpret_cast<std::uintptr_t>(pointer);
    if (value >= 0x02000000u && value < 0x0F000000u) {
        if (write_access)
            return g_memory.ResolveWrite(static_cast<std::uint32_t>(value), length);
        return const_cast<std::uint8_t*>(g_memory.ResolveRead(static_cast<std::uint32_t>(value), length));
    }
    return const_cast<void*>(pointer);
}

bool CopySize(std::uint32_t control, std::size_t unit, std::size_t* bytes) {
    const std::size_t count = control & fe7::bios::kTransferCountMask;
    if (count > std::numeric_limits<std::size_t>::max() / unit) return false;
    *bytes = count * unit;
    return true;
}
}

extern "C" int fe7_gba_attach_rom(const uint8_t* data, size_t size) {
    return g_memory.AttachRom(data, size) ? 1 : 0;
}
extern "C" void fe7_gba_detach_rom(void) { g_memory.DetachRom(); }
extern "C" void fe7_gba_reset(int clear_ewram) { g_memory.Reset(clear_ewram != 0); }
extern "C" volatile void* fe7_gba_io_pointer(uint32_t address, size_t length) {
    return g_memory.ResolveIoRegister(address, length);
}
extern "C" void* fe7_gba_resolve_pointer(uintptr_t value, size_t length, int write_access) {
    if (value >= 0x02000000u && value < 0x0F000000u) {
        if (write_access)
            return g_memory.ResolveWrite(static_cast<std::uint32_t>(value), length);
        return const_cast<std::uint8_t*>(g_memory.ResolveRead(static_cast<std::uint32_t>(value), length));
    }
    return reinterpret_cast<void*>(value);
}

extern "C" void CpuSet(const void* source, void* destination, uint32_t control) {
    const std::size_t unit = (control & fe7::bios::kTransfer32Bit) ? 4u : 2u;
    std::size_t bytes = 0;
    if (!CopySize(control, unit, &bytes)) return;
    const void* resolved_source = ResolveLegacyPointer(source, bytes, false);
    void* resolved_destination = ResolveLegacyPointer(destination, bytes, true);
    if (bytes != 0 && (resolved_source == nullptr || resolved_destination == nullptr)) return;
    (void)fe7::bios::CpuSet(resolved_source, resolved_destination, control);
}

extern "C" void CpuFastSet(const void* source, void* destination, uint32_t control) {
    std::size_t bytes = 0;
    if (!CopySize(control, sizeof(std::uint32_t), &bytes)) return;
    const void* resolved_source = ResolveLegacyPointer(source, bytes, false);
    void* resolved_destination = ResolveLegacyPointer(destination, bytes, true);
    if (bytes != 0 && (resolved_source == nullptr || resolved_destination == nullptr)) return;
    (void)fe7::bios::CpuFastSet(resolved_source, resolved_destination, control);
}
