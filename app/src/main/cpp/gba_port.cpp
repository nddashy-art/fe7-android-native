#include "gba_port.h"

#include "gba_bios_compat.h"
#include "gba_memory.h"
#include "gba_ppu.h"

#include <cstdint>
#include <limits>

namespace {
fe7::gba::MemoryBus g_memory;
fe7::gba::PpuRenderer g_ppu(g_memory);
void* g_interrupt_vector = nullptr;

void* ResolveLegacyPointer(const void* pointer, std::size_t length, bool write_access) {
    if (pointer == nullptr) return nullptr;
    const std::uintptr_t value = reinterpret_cast<std::uintptr_t>(pointer);
    // In FE7, fixed memory addresses are passed as pointers. Translate those
    // numeric addresses before native code dereferences them.
    if (value >= 0x02000000u && value < 0x0F000000u) {
        if (write_access)
            return g_memory.ResolveWrite(static_cast<std::uint32_t>(value), length);
        return const_cast<std::uint8_t*>(g_memory.ResolveRead(static_cast<std::uint32_t>(value), length));
    }
    // Host pointers are valid for ordinary native arrays and asset buffers.
    return const_cast<void*>(pointer);
}

bool CopySize(std::uint32_t control, std::size_t unit, std::size_t* bytes) {
    const std::size_t count = control & fe7::bios::kTransferCountMask;
    if (count > std::numeric_limits<std::size_t>::max() / unit) return false;
    *bytes = count * unit;
    return true;
}

std::size_t SourceBytesAvailable(const void* source) {
    if (source == nullptr) return 0;
    const std::uintptr_t value = reinterpret_cast<std::uintptr_t>(source);
    if (value >= 0x02000000u && value < 0x0F000000u)
        return g_memory.ReadableBytesFrom(static_cast<std::uint32_t>(value));

    const auto* rom = g_memory.RomData();
    const std::uintptr_t rom_begin = reinterpret_cast<std::uintptr_t>(rom);
    const std::uintptr_t rom_end = rom_begin + g_memory.RomSize();
    if (rom != nullptr && value >= rom_begin && value < rom_end)
        return static_cast<std::size_t>(rom_end - value);
    return 0; // Unknown allocations need an explicit-size decompression API.
}

void Lz77Legacy(const void* source, void* destination, bool require_halfword_size) {
    const auto* header = static_cast<const std::uint8_t*>(ResolveLegacyPointer(source, 4, false));
    if (header == nullptr || header[0] != 0x10) return;

    const std::size_t output_size = static_cast<std::size_t>(header[1]) |
        (static_cast<std::size_t>(header[2]) << 8) |
        (static_cast<std::size_t>(header[3]) << 16);
    if (require_halfword_size && (output_size & 1u) != 0) return;

    const std::uintptr_t dst_value = reinterpret_cast<std::uintptr_t>(destination);
    const bool dst_is_gba_address = dst_value >= 0x02000000u && dst_value < 0x0F000000u;
    const std::size_t output_capacity = dst_is_gba_address
        ? g_memory.WritableBytesFrom(static_cast<std::uint32_t>(dst_value))
        : output_size;
    if (output_size > output_capacity) return;

    const std::size_t input_size = SourceBytesAvailable(source);
    if (input_size < 4) return;
    auto* output = static_cast<std::uint8_t*>(ResolveLegacyPointer(destination, output_size, true));
    if (output == nullptr && output_size != 0) return;
    std::size_t written = 0;
    (void)fe7::bios::Lz77UnCompWram(header, input_size, output, output_capacity, &written);
}
}  // namespace

extern "C" int fe7_gba_attach_rom(const uint8_t* data, size_t size) {
    return g_memory.AttachRom(data, size) ? 1 : 0;
}
extern "C" void fe7_gba_detach_rom(void) { g_memory.DetachRom(); }
extern "C" void fe7_gba_reset(int clear_ewram) { g_memory.Reset(clear_ewram != 0); }
extern "C" void fe7_gba_set_keys(std::uint16_t pressed_keys) {
    constexpr std::uint16_t kGbaKeyMask = 0x03FFu;
    auto* keyinput = static_cast<volatile std::uint16_t*>(
        g_memory.ResolveIoRegister(0x04000130u, sizeof(std::uint16_t)));
    if (keyinput == nullptr) return;
    *keyinput = static_cast<std::uint16_t>((~pressed_keys) & kGbaKeyMask);
}
extern "C" int fe7_gba_render_frame(uint32_t* output_argb, size_t pixel_count) {
    return g_ppu.RenderFrame(output_argb, pixel_count) ? 1 : 0;
}
extern "C" void** fe7_gba_interrupt_vector_slot(void) { return &g_interrupt_vector; }
extern "C" volatile void* fe7_gba_io_pointer(uint32_t address, size_t length) {
    return g_memory.ResolveIoRegister(address, length);
}
extern "C" void* fe7_gba_resolve_pointer(uintptr_t address_or_host_pointer,
                                           size_t length, int write_access) {
    const auto value = address_or_host_pointer;
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
    const std::size_t source_bytes = (control & fe7::bios::kSourceFixed) ? unit : bytes;
    const void* resolved_source = ResolveLegacyPointer(source, source_bytes, false);
    void* resolved_destination = ResolveLegacyPointer(destination, bytes, true);
    if (bytes != 0 && (resolved_source == nullptr || resolved_destination == nullptr)) return;
    (void)fe7::bios::CpuSet(resolved_source, resolved_destination, control);
}

extern "C" void CpuFastSet(const void* source, void* destination, uint32_t control) {
    std::size_t bytes = 0;
    if (!CopySize(control, sizeof(std::uint32_t), &bytes)) return;
    const std::size_t source_bytes = (control & fe7::bios::kSourceFixed) ? sizeof(std::uint32_t) : bytes;
    const void* resolved_source = ResolveLegacyPointer(source, source_bytes, false);
    void* resolved_destination = ResolveLegacyPointer(destination, bytes, true);
    if (bytes != 0 && (resolved_source == nullptr || resolved_destination == nullptr)) return;
    (void)fe7::bios::CpuFastSet(resolved_source, resolved_destination, control);
}

extern "C" void LZ77UnComp8bit(const void* source, void* destination) {
    Lz77Legacy(source, destination, false);
}
extern "C" void LZ77UnComp16bit(const void* source, void* destination) {
    Lz77Legacy(source, destination, true);
}
