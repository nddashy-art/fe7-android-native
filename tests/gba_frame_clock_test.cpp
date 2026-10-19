#include "gba_port.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>

namespace {
int g_vblank_count = 0;
int g_hblank_count = 0;
int g_vcount_count = 0;

void OnVBlank() { ++g_vblank_count; }
void OnHBlank() { ++g_hblank_count; }
void OnVCount() { ++g_vcount_count; }

volatile std::uint16_t* Io16(std::uint32_t address) {
    return static_cast<volatile std::uint16_t*>(fe7_gba_io_pointer(address, 2));
}
} // namespace

int main() {
    fe7_gba_reset(1);
    fe7_gba_set_irq_handler(0, OnVBlank);
    fe7_gba_set_irq_handler(1, OnHBlank);
    fe7_gba_set_irq_handler(2, OnVCount);

    auto* dispstat = Io16(0x04000004u);
    auto* vcount = Io16(0x04000006u);
    auto* ie = Io16(0x04000200u);
    auto* flags = Io16(0x04000202u);
    auto* ime = Io16(0x04000208u);
    assert(dispstat && vcount && ie && flags && ime);

    // Enable VBlank/HBlank/VCOUNT IRQs, with a match on scanline 5.
    *dispstat = static_cast<std::uint16_t>((1u << 3) | (1u << 4) | (1u << 5) | (5u << 8));
    *ie = (1u << 0) | (1u << 1) | (1u << 2);
    *ime = 1;

    // HBlank DMA must fire once per visible scanline, then stop during VBlank.
    std::array<std::uint16_t, 160> source{};
    for (unsigned i = 0; i < source.size(); ++i)
        source[i] = static_cast<std::uint16_t>(0x300u + i);
    auto* destination = static_cast<std::uint16_t*>(
        fe7_gba_resolve_pointer(0x02001000u, source.size() * sizeof(std::uint16_t), 1));
    assert(destination != nullptr);
    fe7_gba_dma_set(0, source.data(), reinterpret_cast<void*>(0x02001000u),
                    (static_cast<std::uint32_t>(0xA200u) << 16) | 1u);

    fe7_gba_advance_frame();

    assert(g_vblank_count == 1);
    assert(g_hblank_count == 228);
    assert(g_vcount_count == 1);
    assert(*vcount == 0);
    assert((*dispstat & 0x0007u) == 0);
    assert(*flags == 0);
    for (unsigned i = 0; i < source.size(); ++i)
        assert(destination[i] == source[i]);

    std::puts("GBA frame clock tests passed");
    return 0;
}
