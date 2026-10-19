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

void TestAffineBackgroundAndWraparound() {
    fe7::gba::MemoryBus memory;
    fe7::gba::PpuRenderer ppu(memory);

    // Mode 1: BG2 is affine; a 256x256 tile map is sufficient for this frame.
    Write16(memory, kDisplayControl, 1u | (1u << 10));
    Write16(memory, kBg0Control + 4u, (31u << 8) | (1u << 14));
    Write16(memory, kPalette + 2u, 0x001Fu); // Palette index 1 = red.
    for (unsigned i = 0; i < 32u * 32u; ++i)
        assert(memory.Write8(kVram + 31u * 0x800u + i, 1u));
    for (unsigned i = 0; i < 64u; ++i)
        assert(memory.Write8(kVram + 64u + i, 1u)); // Affine tile 1, 8bpp.
    Write16(memory, 0x04000020u, 0x0100u); // PA=1.0
    Write16(memory, 0x04000022u, 0x0000u); // PB=0
    Write16(memory, 0x04000024u, 0x0000u); // PC=0
    Write16(memory, 0x04000026u, 0x0100u); // PD=1.0
    assert(memory.Write32(0x04000028u, 0u));
    assert(memory.Write32(0x0400002Cu, 0u));

    std::array<std::uint32_t, fe7::gba::kPpuFramePixels> frame{};
    assert(ppu.RenderFrame(frame.data(), frame.size()));
    assert(frame[Pixel(0, 0)] == 0xFFFF0000u);
    assert(frame[Pixel(239, 159)] == 0xFFFF0000u);

    // Reference X=256 pixels lies outside an unwrapped 256px BG.
    assert(memory.Write32(0x04000028u, 0x00010000u));
    assert(ppu.RenderFrame(frame.data(), frame.size()));
    assert(frame[Pixel(0, 0)] == 0xFF000000u);

    // With the hardware wrap flag, the same coordinate wraps to X=0.
    auto* bg2_control = static_cast<volatile std::uint16_t*>(
        memory.ResolveIoRegister(kBg0Control + 4u, 2u));
    assert(bg2_control != nullptr);
    *bg2_control = static_cast<std::uint16_t>(*bg2_control | (1u << 13));
    assert(ppu.RenderFrame(frame.data(), frame.size()));
    assert(frame[Pixel(0, 0)] == 0xFFFF0000u);
}

void TestBitmapModes345() {
    fe7::gba::MemoryBus memory;
    fe7::gba::PpuRenderer ppu(memory);
    std::array<std::uint32_t, fe7::gba::kPpuFramePixels> frame{};

    // Modes 3-5 use the BG2 affine matrix. Identity leaves the reference at (0,0).
    Write16(memory, 0x04000020u, 0x0100u);
    Write16(memory, 0x04000022u, 0u);
    Write16(memory, 0x04000024u, 0u);
    Write16(memory, 0x04000026u, 0x0100u);
    assert(memory.Write32(0x04000028u, 0u));
    assert(memory.Write32(0x0400002Cu, 0u));
    Write16(memory, kBg0Control + 4u, 0u);
    Write16(memory, kPalette + 2u, 0x001Fu); // Red.
    Write16(memory, kPalette + 4u, 0x03E0u); // Green.

    // Mode 3: 240x160 direct-color framebuffer.
    Write16(memory, kDisplayControl, 3u | (1u << 10));
    Write16(memory, kVram, 0x001Fu);
    Write16(memory, kVram + (159u * 240u + 239u) * 2u, 0x03E0u);
    assert(ppu.RenderFrame(frame.data(), frame.size()));
    assert(frame[Pixel(0, 0)] == 0xFFFF0000u);
    assert(frame[Pixel(239, 159)] == 0xFF00FF00u);

    // Mode 4: 240x160 palette-indexed framebuffer, with page select at 0xA000.
    Write16(memory, kDisplayControl, 4u | (1u << 4) | (1u << 10));
    assert(memory.Write8(kVram + 0xA000u, 1u));
    assert(memory.Write8(kVram + 0xA000u + 159u * 240u + 239u, 2u));
    assert(ppu.RenderFrame(frame.data(), frame.size()));
    assert(frame[Pixel(0, 0)] == 0xFFFF0000u);
    assert(frame[Pixel(239, 159)] == 0xFF00FF00u);

    // Mode 5: 160x128 direct-color framebuffer. Pixels outside it show backdrop.
    Write16(memory, kDisplayControl, 5u | (1u << 4) | (1u << 10));
    Write16(memory, kVram + 0xA000u, 0x03E0u);
    Write16(memory, kVram + 0xA000u + (127u * 160u + 159u) * 2u, 0x001Fu);
    assert(ppu.RenderFrame(frame.data(), frame.size()));
    assert(frame[Pixel(0, 0)] == 0xFF00FF00u);
    assert(frame[Pixel(159, 127)] == 0xFFFF0000u);
    assert(frame[Pixel(160, 127)] == 0xFF000000u);
    assert(frame[Pixel(239, 159)] == 0xFF000000u);

    // Bitmap modes reserve VRAM through 0x13FFF for the framebuffer;
    // OBJ tile data starts at 0x14000 rather than the tiled-mode 0x10000.
    Write16(memory, kDisplayControl,
        4u | (1u << 4) | (1u << 6) | (1u << 10) | (1u << 12));
    Write16(memory, kPalette + 2u * (256u + 2u), 0x03E0u);
    for (unsigned i = 0; i < 32u; ++i)
        assert(memory.Write8(kVram + 0x14000u + i, 0x22u));
    Write16(memory, kOam, 20u);
    Write16(memory, kOam + 2u, 10u);
    Write16(memory, kOam + 4u, 0u);
    assert(ppu.RenderFrame(frame.data(), frame.size()));
    assert(frame[Pixel(10, 20)] == 0xFF00FF00u);

    // If BG2 is disabled in a bitmap mode, the framebuffer does not draw.
    Write16(memory, kDisplayControl, 5u | (1u << 4));
    assert(ppu.RenderFrame(frame.data(), frame.size()));
    assert(frame[Pixel(0, 0)] == 0xFF000000u);
}

void TestRegularObjectOverTextBackground() {
    fe7::gba::MemoryBus memory;
    fe7::gba::PpuRenderer ppu(memory);

    Write16(memory, kDisplayControl, (1u << 8) | (1u << 6) | (1u << 12));
    Write16(memory, kBg0Control, 31u << 8);
    for (unsigned i = 0; i < 128; ++i)
        Write16(memory, kOam + i * 8u, 0x0200u);  // Hide unused objects.
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

    // A priority-1 object sits behind priority-0 background pixels.
    Write16(memory, kOam + 4u, 1u << 10);
    assert(ppu.RenderFrame(frame.data(), frame.size()));
    assert(frame[Pixel(10, 20)] == 0xFFFF0000u);
}

void TestAffineObjectIdentityAndDoubleSize() {
    fe7::gba::MemoryBus memory;
    fe7::gba::PpuRenderer ppu(memory);

    Write16(memory, kDisplayControl, (1u << 6) | (1u << 12)); // 1D OBJ tiles + OBJ enable.
    Write16(memory, kPalette + 2u * (256u + 2u), 0x03E0u); // OBJ palette index 2 = green.
    for (unsigned i = 0; i < 128; ++i)
        Write16(memory, kOam + i * 8u, 0x0200u); // Hide every unused non-affine OBJ.
    for (unsigned i = 0; i < 32; ++i)
        assert(memory.Write8(kVram + 0x10000u + i, 0x22u)); // Tile 0, 4bpp index 2.

    // Affine OBJ at (10,20), 8x8, using matrix 0. Matrix entries are the attr3
    // halfwords of OAM entries 0, 1, 2, and 3.
    Write16(memory, kOam, 20u | (1u << 8));
    Write16(memory, kOam + 2u, 10u);
    Write16(memory, kOam + 4u, 0u);
    Write16(memory, kOam + 6u, 0x0100u);  // PA=1.0
    Write16(memory, kOam + 14u, 0u);      // PB=0
    Write16(memory, kOam + 22u, 0u);      // PC=0
    Write16(memory, kOam + 30u, 0x0100u); // PD=1.0

    std::array<std::uint32_t, fe7::gba::kPpuFramePixels> frame{};
    assert(ppu.RenderFrame(frame.data(), frame.size()));
    assert(frame[Pixel(10, 20)] == 0xFF00FF00u);
    assert(frame[Pixel(17, 27)] == 0xFF00FF00u);
    assert(frame[Pixel(18, 27)] == 0xFF000000u);

    // Double-size bounds plus a 0.5 inverse matrix scale produce a 16x16 OBJ.
    Write16(memory, kOam, 20u | (1u << 8) | (1u << 9));
    Write16(memory, kOam + 6u, 0x0080u);
    Write16(memory, kOam + 30u, 0x0080u);
    assert(ppu.RenderFrame(frame.data(), frame.size()));
    assert(frame[Pixel(10, 20)] == 0xFF00FF00u);
    assert(frame[Pixel(25, 35)] == 0xFF00FF00u);
    assert(frame[Pixel(26, 35)] == 0xFF000000u);
    assert(frame[Pixel(9, 20)] == 0xFF000000u);
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
    TestAffineBackgroundAndWraparound();
    TestBitmapModes345();
    TestRegularObjectOverTextBackground();
    TestAffineObjectIdentityAndDoubleSize();
    TestRejectsShortOutputBuffer();
    std::cout << "GBA PPU tests passed\n";
    return 0;
}
