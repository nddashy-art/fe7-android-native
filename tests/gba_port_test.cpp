#include "gba_port.h"
#include "gba_memory.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

int main() {
    fe7_gba_reset(1);
    auto* io = static_cast<volatile std::uint16_t*>(fe7_gba_io_pointer(0x04000130u, 2));
    assert(io != nullptr && *io == 0x03FFu);
    *io = 0x02FFu;
    assert(fe7_gba_io_pointer(0x04000131u, 1) != nullptr);

    const std::uint32_t fill = 0xA1B2C3D4u;
    CpuSet(&fill, reinterpret_cast<void*>(0x02000000u), 4u | (1u << 26) | (1u << 24));
    auto* destination = static_cast<const std::uint8_t*>(fe7_gba_resolve_pointer(0x02000000u, 16, 0));
    assert(destination != nullptr);
    for (std::size_t i = 0; i < 4; ++i) {
        std::uint32_t value = 0;
        std::memcpy(&value, destination + i * 4, sizeof(value));
        assert(value == fill);
    }

    std::array<std::uint32_t, 8> source{};
    for (std::size_t i = 0; i < source.size(); ++i) source[i] = static_cast<std::uint32_t>(i + 20);
    CpuFastSet(source.data(), reinterpret_cast<void*>(0x02000010u), 8u);
    const auto* copied = static_cast<const std::uint8_t*>(fe7_gba_resolve_pointer(0x02000010u, 32, 0));
    assert(copied != nullptr);
    for (std::size_t i = 0; i < source.size(); ++i) {
        std::uint32_t value = 0;
        std::memcpy(&value, copied + i * 4, sizeof(value));
        assert(value == source[i]);
    }

    const std::array<std::uint8_t, 4> rom = {1, 2, 3, 4};
    assert(fe7_gba_attach_rom(rom.data(), rom.size()) == 1);
    const auto* rom_byte = static_cast<const std::uint8_t*>(fe7_gba_resolve_pointer(0x08000002u, 1, 0));
    assert(rom_byte != nullptr && *rom_byte == 3);
    assert(fe7_gba_resolve_pointer(0x08000000u, 1, 1) == nullptr);
    fe7_gba_detach_rom();
    std::puts("GBA port adapter tests passed");
    return 0;
}
