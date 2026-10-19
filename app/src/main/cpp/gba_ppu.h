#pragma once

#include "gba_memory.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace fe7::gba {

constexpr unsigned kPpuFrameWidth = 240;
constexpr unsigned kPpuFrameHeight = 160;
constexpr std::size_t kPpuFramePixels =
    static_cast<std::size_t>(kPpuFrameWidth) * kPpuFrameHeight;

// Software PPU baseline for GBA text backgrounds and regular (non-affine)
// objects. Bitmap/affine modes, windows, mosaic and color effects are not yet
// implemented. The class reads from the same bus written by FE7's C runtime.
class PpuRenderer final {
public:
    explicit PpuRenderer(const MemoryBus& memory) : memory_(memory) {}

    bool RenderFrame(std::uint32_t* output_argb, std::size_t pixel_count);

private:
    struct PixelState {
        std::uint32_t color = 0xFF000000u;
        std::uint16_t rank = 4000;
    };

    bool RenderTextBackgroundPixel(unsigned bg, unsigned x, unsigned y,
                                   std::uint32_t* color,
                                   std::uint16_t* rank) const;
    bool RenderAffineBackgroundPixel(unsigned bg, unsigned x, unsigned y,
                                     std::uint32_t* color,
                                     std::uint16_t* rank) const;
    void RenderObjects();
    void RenderObject(unsigned oam_index);
    bool ReadTextTilePixel(unsigned bg, std::uint16_t control,
                           unsigned screen_x, unsigned screen_y,
                           std::uint8_t* palette_index) const;
    std::uint32_t ReadPaletteColor(unsigned palette_index) const;
    static std::uint32_t ConvertBgr555(std::uint16_t color);

    const MemoryBus& memory_;
    std::array<PixelState, kPpuFramePixels> pixels_{};
};

}  // namespace fe7::gba
