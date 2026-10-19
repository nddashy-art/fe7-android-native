#include "gba_memory.h"
#include "gba_ppu.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>

namespace {
constexpr std::uint32_t kDisplayControl = 0x04000000u;
constexpr std::uint32_t kBg0Control = 0x04000008u;
constexpr std::uint32_t kPalette = 0x05000000u;
constexpr std::uint32_t kVram = 0x06000000u;
constexpr std::uint32_t kOam = 0x07000000u;

void Write16(fe7::gba::MemoryBus& memory, std::uint32_t address, std::uint16_t value) {
    assert(memory.Write16(address, value));
}

std::size_t Pixel(unsigned x, unsigned y) {
    return static_cast<std::size_t>(y) * fe7::gba::kPpuFrameWidth + x;
}

void TestTextBackgroundAndForcedBlank() {
    fe7::gba::MemoryBus memory;
    fe7::gba::PpuRenderer ppu(memory);

    Write16(memory, kDisplayControl, 1u << 8);  // Enable BG0 in mode 0.
    Write16(memory, kBg0Control, 31u << 8);      // Screen base block 31.
    Write16(memory, kPalette + 2u, 0x001Fu);     // Palette index 1 = red.
    Write16(memory, kVram + 31u * 0x800u, 0);    // First tile-map entry = tile 0.
    for (unsigned i = 0; i < 32; ++i)
        assert(memory.Write8(kVram + i, 0x11));  // 4bpp tile, every pixel index 1.

    std::array<std::uint32_t, fe7::gba::kPpuFramePixels> frame{};
    assert(ppu.RenderFrame(frame.data(), frame.size()));
    assert(frame[Pixel(0, 0)] == 0xFFFF0000u);
    assert(frame[Pixel(239, 159)] == 0xFFFF0000u);

    Write16(memory, kDisplayControl, (1u << 8) | (1u << 7));
    assert(ppu.RenderFrame(frame.data(), frame.size()));
    assert(frame[0] == 0xFFFFFFFFu);
    assert(frame.back() == 0xFFFFFFFFu);
}

void TestRegularObjectOverTextBackground() {
    fe7::gba::MemoryBus memory;
    fe7::gba::PpuRenderer ppu(memory);

    Write16(memory, kDisplayControl, (1u << 8) | (1u << 6) | (1u << 12));
    Write16(memory, kBg0Control, 31u << 8);
    Write16(memory, kPalette + 2u, 0x001Fu);             // BG red.
    Write16(memory, kPalette + 2u * (256u + 2u), 0x03E0u); // OBJ palette index 2 = green.
    Write16(memory, kVram + 31u * 0x800u, 0);
    for (unsigned i = 0; i < 32; ++i) assert(memory.Write8(kVram + i, 0x11));
    for (unsigned i = 0; i < 32; ++i) assert(memory.Write8(kVram + 0x10000u + i, 0x22));

    Write16(memory, kOam, 20u);     // Y=20, square regular OBJ, 4bpp.
    Write16(memory, kOam + 2u, 10u);  // X=10, size 8x8.
    Write16(memory, kOam + 4u, 0u);   // Tile 0, priority 0, palette bank 0.

    std::array<std::uint32_t, fe7::gba::kPpuFramePixels> frame{};
    assert(ppu.RenderFrame(frame.data(), frame.size()));
    assert(frame[Pixel(10, 20)] == 0xFF00FF00u);
    assert(frame[Pixel(0, 0)] == 0xFFFF0000u);
}

void TestRejectsShortOutputBuffer() {
    fe7::gba::MemoryBus memory;
    fe7::gba::PpuRenderer ppu(memory);
    std::array<std::uint32_t, 2> too_small{};
    assert(!ppu.RenderFrame(too_small.data(), too_small.size()));
}
}  // namespace

int main() {
    TestTextBackgroundAndForcedBlank();
    TestRegularObjectOverTextBackground();
    TestRejectsShortOutputBuffer();
    std::cout << "GBA PPU tests passed\n";
    return 0;
}
