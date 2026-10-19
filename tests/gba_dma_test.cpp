#include "gba_dma.h"
#include "gba_port.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>

int main() {
    fe7_gba_reset(1);
    std::array<std::uint32_t, 8> source = {11, 22, 33, 44, 55, 66, 77, 88};
    auto* dst = static_cast<std::uint32_t*>(fe7_gba_resolve_pointer(0x02001000u, 32, 1));
    assert(dst != nullptr);

    // Immediate 32-bit copy: count 4, enable + word-transfer flags in cnt_h.
    fe7_gba_dma_set(3, source.data(), reinterpret_cast<void*>(0x02001000u),
                    (static_cast<std::uint32_t>(0x8400u) << 16) | 4u);
    for (std::size_t i = 0; i < 4; ++i) assert(dst[i] == source[i]);

    // VBlank-triggered repeating transfer; destination reloads for each frame.
    std::array<std::uint16_t, 3> vblank_src = {0x1234, 0x2345, 0x3456};
    auto* vblank_dst = static_cast<std::uint16_t*>(fe7_gba_resolve_pointer(0x02002000u, 8, 1));
    assert(vblank_dst != nullptr);
    fe7_gba_dma_set(0, vblank_src.data(), reinterpret_cast<void*>(0x02002000u),
                    (static_cast<std::uint32_t>(0xA260u) << 16) | 3u);
    assert(vblank_dst[0] == 0 && vblank_dst[1] == 0);
    fe7_gba_dma_on_vblank();
    for (std::size_t i = 0; i < 3; ++i) assert(vblank_dst[i] == vblank_src[i]);
    vblank_dst[0] = 0;
    fe7_gba_dma_on_vblank();
    assert(vblank_dst[0] == vblank_src[0]);

    // HBlank copies are deferred until the scanline callback.
    auto* hblank_dst = static_cast<std::uint16_t*>(fe7_gba_resolve_pointer(0x02003000u, 4, 1));
    assert(hblank_dst != nullptr);
    fe7_gba_dma_set(1, vblank_src.data(), reinterpret_cast<void*>(0x02003000u),
                    (static_cast<std::uint32_t>(0xA200u) << 16) | 2u);
    assert(hblank_dst[0] == 0);
    fe7_gba_dma_on_hblank(0);
    assert(hblank_dst[0] == vblank_src[0] && hblank_dst[1] == vblank_src[1]);

    // Invalid channel is safely ignored.
    fe7_gba_dma_set(4, source.data(), dst, (static_cast<std::uint32_t>(0x8400u) << 16) | 2u);
    std::puts("GBA DMA tests passed");
    return 0;
}
