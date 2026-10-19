#include "gba_port.h"
#include "gba_memory.h"
#include "gba_ppu.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>

int main() {
    fe7_gba_reset(1);
    // The public runtime bridge exposes the software PPU to a future native frame loop.
    auto* display_control = static_cast<volatile std::uint16_t*>(
        fe7_gba_io_pointer(0x04000000u, 2));
    assert(display_control != nullptr);
    *display_control = 1u << 7;  // Forced blank must render white.
    std::array<std::uint32_t, fe7::gba::kPpuFramePixels> rendered{};
    assert(fe7_gba_render_frame(rendered.data(), rendered.size()) == 1);
    assert(rendered.front() == 0xFFFFFFFFu && rendered.back() == 0xFFFFFFFFu);

    auto* io = static_cast<volatile std::uint16_t*>(fe7_gba_io_pointer(0x04000130u, 2));
    assert(io != nullptr && *io == 0x03FFu);
    const std::uint16_t pressed_keys = (1u << 0) | (1u << 4) | (1u << 9);
    fe7_gba_set_keys(pressed_keys);
    assert(*io == static_cast<std::uint16_t>(0x03FFu & ~pressed_keys));
    fe7_gba_set_keys(0);
    assert(*io == 0x03FFu);
    *io = 0x02FFu;
    assert(static_cast<volatile std::uint8_t*>(fe7_gba_io_pointer(0x04000131u, 1)) != nullptr);

    auto* destination = static_cast<std::uint32_t*>(fe7_gba_resolve_pointer(0x02000000u, 32, 1));
    assert(destination != nullptr);
    const std::uint32_t fill = 0xA1B2C3D4u;
    CpuSet(&fill, reinterpret_cast<void*>(0x02000000u), 4u | (1u << 26) | (1u << 24));
    for (std::size_t i = 0; i < 4; ++i) assert(destination[i] == fill);

    std::array<std::uint32_t, 8> source{};
    for (std::size_t i = 0; i < source.size(); ++i) source[i] = static_cast<std::uint32_t>(i + 20);
    CpuFastSet(source.data(), reinterpret_cast<void*>(0x02000010u), 8u);
    auto* copied = static_cast<const std::uint32_t*>(fe7_gba_resolve_pointer(0x02000010u, 32, 0));
    assert(copied != nullptr);
    for (std::size_t i = 0; i < source.size(); ++i) assert(copied[i] == source[i]);

    const std::array<std::uint8_t, 4> rom = {1, 2, 3, 4};
    assert(fe7_gba_attach_rom(rom.data(), rom.size()) == 1);
    auto* rom_byte = static_cast<const std::uint8_t*>(fe7_gba_resolve_pointer(0x08000002u, 1, 0));
    assert(rom_byte != nullptr && *rom_byte == 3);
    assert(fe7_gba_resolve_pointer(0x08000000u, 1, 1) == nullptr);

    const std::array<std::uint8_t, 10> compressed = {
        0x10, 6, 0, 0, 0x10, 'A', 'B', 'C', 0x00, 0x02
    };
    assert(fe7_gba_attach_rom(compressed.data(), compressed.size()) == 1);
    LZ77UnComp8bit(reinterpret_cast<const void*>(0x08000000u),
                   reinterpret_cast<void*>(0x02000100u));
    const auto* output = static_cast<const std::uint8_t*>(
        fe7_gba_resolve_pointer(0x02000100u, 6, 0));
    assert(output != nullptr);
    for (std::size_t i = 0; i < 6; ++i) assert(output[i] == "ABCABC"[i]);

    const std::array<std::uint8_t, 8> odd_size = {0x10, 3, 0, 0, 0x00, 'X', 'Y', 'Z'};
    assert(fe7_gba_attach_rom(odd_size.data(), odd_size.size()) == 1);
    LZ77UnComp16bit(reinterpret_cast<const void*>(0x08000000u),
                    reinterpret_cast<void*>(0x02000200u));
    const auto* odd_output = static_cast<const std::uint8_t*>(
        fe7_gba_resolve_pointer(0x02000200u, 3, 0));
    assert(odd_output != nullptr && odd_output[0] == 0 && odd_output[1] == 0 && odd_output[2] == 0);

    fe7_gba_detach_rom();
    std::puts("GBA port adapter tests passed");
    return 0;
}
