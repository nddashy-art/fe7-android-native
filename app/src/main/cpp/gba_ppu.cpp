#include "gba_ppu.h"

#include <algorithm>
#include <cstdint>

namespace fe7::gba {
namespace {
constexpr std::uint32_t kDisplayControl = 0x04000000u;
constexpr std::uint32_t kBgControlBase = 0x04000008u;
constexpr std::uint32_t kBgHofsBase = 0x04000010u;
constexpr std::uint32_t kBgVofsBase = 0x04000012u;
constexpr std::uint32_t kPaletteBase = 0x05000000u;
constexpr std::uint32_t kVramBase = 0x06000000u;
constexpr std::uint32_t kOamBase = 0x07000000u;
constexpr std::uint16_t kForcedBlank = 1u << 7;
constexpr std::uint16_t kObjOneDimensional = 1u << 6;
constexpr std::uint16_t kBgEnableBase = 1u << 8;
constexpr std::uint16_t kObjEnable = 1u << 12;
constexpr std::uint16_t kOamAttr0Affine = 1u << 8;
constexpr std::uint16_t kOamAttr0Disable = 1u << 9;
constexpr std::uint16_t kOamAttr0ModeMask = 3u << 10;
constexpr std::uint16_t kOamAttr0Color256 = 1u << 13;
constexpr std::uint16_t kOamAttr0ShapeMask = 3u << 14;
constexpr std::uint16_t kOamAttr1HFlip = 1u << 12;
constexpr std::uint16_t kOamAttr1VFlip = 1u << 13;

bool Read16(const MemoryBus& memory, std::uint32_t address, std::uint16_t* value) {
    return memory.Read16(address, value);
}

bool ShapeSize(unsigned shape, unsigned size, unsigned* width, unsigned* height) {
    static constexpr unsigned kSizes[3][4][2] = {
        {{8, 8}, {16, 16}, {32, 32}, {64, 64}},
        {{16, 8}, {32, 8}, {32, 16}, {64, 32}},
        {{8, 16}, {8, 32}, {16, 32}, {32, 64}},
    };
    if (shape >= 3 || width == nullptr || height == nullptr) return false;
    *width = kSizes[shape][size][0];
    *height = kSizes[shape][size][1];
    return true;
}
}  // namespace

std::uint32_t PpuRenderer::ConvertBgr555(std::uint16_t color) {
    const std::uint32_t r5 = color & 0x1Fu;
    const std::uint32_t g5 = (color >> 5) & 0x1Fu;
    const std::uint32_t b5 = (color >> 10) & 0x1Fu;
    const std::uint32_t r = (r5 << 3) | (r5 >> 2);
    const std::uint32_t g = (g5 << 3) | (g5 >> 2);
    const std::uint32_t b = (b5 << 3) | (b5 >> 2);
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

std::uint32_t PpuRenderer::ReadPaletteColor(unsigned palette_index) const {
    std::uint16_t bgr555 = 0;
    if (palette_index >= 512 ||
        !Read16(memory_, kPaletteBase + palette_index * 2u, &bgr555)) {
        return 0xFF000000u;
    }
    return ConvertBgr555(bgr555);
}

bool PpuRenderer::ReadTextTilePixel(unsigned, std::uint16_t control,
                                    unsigned screen_x, unsigned screen_y,
                                    std::uint8_t* palette_index) const {
    if (palette_index == nullptr) return false;

    const unsigned size = (control >> 14) & 3u;
    const unsigned width_tiles = (size & 1u) ? 64u : 32u;
    const unsigned height_tiles = (size & 2u) ? 64u : 32u;
    screen_x %= width_tiles * 8u;
    screen_y %= height_tiles * 8u;
    const unsigned tile_x = screen_x >> 3;
    const unsigned tile_y = screen_y >> 3;

    const unsigned char_base = (control >> 2) & 3u;
    const unsigned screen_base = (control >> 8) & 31u;
    const bool color256 = (control & (1u << 7)) != 0;
    const unsigned tile_size = color256 ? 64u : 32u;
    const unsigned blocks_wide = width_tiles / 32u;
    const unsigned block_x = tile_x / 32u;
    const unsigned block_y = tile_y / 32u;
    const unsigned screen_block = block_y * blocks_wide + block_x;
    const std::uint32_t map_address = kVramBase + screen_base * 0x800u +
        screen_block * 0x800u + ((tile_y & 31u) * 32u + (tile_x & 31u)) * 2u;

    std::uint16_t entry = 0;
    if (!Read16(memory_, map_address, &entry)) return false;
    unsigned pixel_x = screen_x & 7u;
    unsigned pixel_y = screen_y & 7u;
    if ((entry & (1u << 10)) != 0) pixel_x = 7u - pixel_x;
    if ((entry & (1u << 11)) != 0) pixel_y = 7u - pixel_y;

    const unsigned tile_number = entry & 0x3FFu;
    const std::uint32_t tile_address = kVramBase + char_base * 0x4000u +
        tile_number * tile_size + (color256 ? pixel_y * 8u + pixel_x
                                            : pixel_y * 4u + pixel_x / 2u);
    // BG character data occupies the first 64 KiB of VRAM; OBJ data starts at 0x10000.
    if (tile_address < kVramBase || tile_address >= kVramBase + 0x10000u) return false;

    std::uint8_t value = 0;
    if (!memory_.Read8(tile_address, &value)) return false;
    if (color256) {
        *palette_index = value;
    } else {
        const unsigned nibble = (pixel_x & 1u) ? (value >> 4) : (value & 0x0Fu);
        const unsigned palette_bank = (entry >> 12) & 0x0Fu;
        *palette_index = static_cast<std::uint8_t>(palette_bank * 16u + nibble);
    }
    return true;
}

bool PpuRenderer::RenderTextBackgroundPixel(unsigned bg, unsigned x, unsigned y,
                                            std::uint32_t* color,
                                            std::uint16_t* rank) const {
    if (color == nullptr || rank == nullptr || bg > 3) return false;
    std::uint16_t control = 0, hofs = 0, vofs = 0;
    if (!Read16(memory_, kBgControlBase + bg * 2u, &control) ||
        !Read16(memory_, kBgHofsBase + bg * 4u, &hofs) ||
        !Read16(memory_, kBgVofsBase + bg * 4u, &vofs)) {
        return false;
    }

    std::uint8_t palette_index = 0;
    if (!ReadTextTilePixel(bg, control, x + (hofs & 0x1FFu),
                          y + (vofs & 0x1FFu), &palette_index)) {
        return false;
    }
    *color = ReadPaletteColor(palette_index);
    // Lower priority wins. For equal priority OBJ beats BG, then BG0 beats BG1, etc.
    *rank = static_cast<std::uint16_t>((control & 3u) * 1000u + 256u + bg);
    return true;
}

void PpuRenderer::RenderObject(unsigned oam_index) {
    std::uint16_t display_control = 0;
    if (!Read16(memory_, kDisplayControl, &display_control) ||
        (display_control & kObjEnable) == 0) return;
    const bool one_dimensional = (display_control & kObjOneDimensional) != 0;

    std::uint16_t attr0 = 0, attr1 = 0, attr2 = 0;
    const std::uint32_t entry = kOamBase + oam_index * 8u;
    if (!Read16(memory_, entry, &attr0) ||
        !Read16(memory_, entry + 2u, &attr1) ||
        !Read16(memory_, entry + 4u, &attr2)) return;

    const bool affine = (attr0 & kOamAttr0Affine) != 0;
    if (affine) return;  // Affine OBJ matrices are not implemented in this baseline.
    if ((attr0 & kOamAttr0Disable) != 0) return;
    const unsigned object_mode = (attr0 & kOamAttr0ModeMask) >> 10;
    if (object_mode != 0) return;  // Blending/OBJ-window behavior needs later renderer work.

    const unsigned shape = (attr0 & kOamAttr0ShapeMask) >> 14;
    const unsigned size = (attr1 >> 14) & 3u;
    unsigned width = 0, height = 0;
    if (!ShapeSize(shape, size, &width, &height)) return;

    int object_x = static_cast<int>(attr1 & 0x1FFu);
    int object_y = static_cast<int>(attr0 & 0xFFu);
    if (object_x >= 256) object_x -= 512;
    if (object_y >= 160) object_y -= 256;

    const bool color256 = (attr0 & kOamAttr0Color256) != 0;
    const unsigned tile_base = color256 ? ((attr2 & 0x3FFu) & ~1u) : (attr2 & 0x3FFu);
    const unsigned tile_bytes = color256 ? 64u : 32u;
    const unsigned tiles_wide = width / 8u;
    const unsigned object_priority = (attr2 >> 10) & 3u;
    const std::uint16_t object_rank =
        static_cast<std::uint16_t>(object_priority * 1000u + oam_index);

    for (unsigned dy = 0; dy < height; ++dy) {
        const int screen_y = object_y + static_cast<int>(dy);
        if (screen_y < 0 || screen_y >= static_cast<int>(kPpuFrameHeight)) continue;
        for (unsigned dx = 0; dx < width; ++dx) {
            const int screen_x = object_x + static_cast<int>(dx);
            if (screen_x < 0 || screen_x >= static_cast<int>(kPpuFrameWidth)) continue;

            unsigned source_x = dx;
            unsigned source_y = dy;
            if ((attr1 & kOamAttr1HFlip) != 0) source_x = width - 1u - source_x;
            if ((attr1 & kOamAttr1VFlip) != 0) source_y = height - 1u - source_y;

            const unsigned tile_x = source_x >> 3;
            const unsigned tile_y = source_y >> 3;
            const unsigned tile_offset = one_dimensional
                ? (tile_y * tiles_wide + tile_x) * tile_bytes
                : tile_y * 0x400u + tile_x * tile_bytes;
            const std::uint32_t tile_address = kVramBase + 0x10000u +
                tile_base * 32u + tile_offset;
            if (tile_address < kVramBase + 0x10000u ||
                tile_address >= kVramBase + 0x18000u) continue;

            const unsigned within_x = source_x & 7u;
            const unsigned within_y = source_y & 7u;
            const std::uint32_t pixel_address = tile_address +
                (color256 ? within_y * 8u + within_x
                          : within_y * 4u + within_x / 2u);
            std::uint8_t packed = 0;
            if (!memory_.Read8(pixel_address, &packed)) continue;

            unsigned palette_index = 0;
            if (color256) {
                palette_index = packed;
            } else {
                const unsigned nibble = (within_x & 1u) ? (packed >> 4) : (packed & 0x0Fu);
                if (nibble == 0) continue;  // OBJ palette index zero is transparent.
                palette_index = ((attr2 >> 12) & 0x0Fu) * 16u + nibble;
            }
            if (color256 && palette_index == 0) continue;

            const std::size_t pixel =
                static_cast<std::size_t>(screen_y) * kPpuFrameWidth +
                static_cast<unsigned>(screen_x);
            if (object_rank < pixels_[pixel].rank) {
                pixels_[pixel].rank = object_rank;
                pixels_[pixel].color = ReadPaletteColor(256u + palette_index);
            }
        }
    }
}

void PpuRenderer::RenderObjects() {
    std::uint16_t display_control = 0;
    if (!Read16(memory_, kDisplayControl, &display_control) ||
        (display_control & kObjEnable) == 0) return;
    for (unsigned i = 0; i < 128; ++i)
        RenderObject(i);
}

bool PpuRenderer::RenderFrame(std::uint32_t* output_argb, std::size_t pixel_count) {
    if (output_argb == nullptr || pixel_count < kPpuFramePixels) return false;
    std::uint16_t display_control = 0;
    if (!Read16(memory_, kDisplayControl, &display_control)) return false;

    if ((display_control & kForcedBlank) != 0) {
        std::fill(output_argb, output_argb + kPpuFramePixels, 0xFFFFFFFFu);
        return true;
    }

    const unsigned mode = display_control & 7u;
    const std::uint32_t backdrop = ReadPaletteColor(0);
    for (auto& pixel : pixels_) {
        pixel.color = backdrop;
        pixel.rank = 4000u;
    }

    // Mode 0 is the common text-background mode; mode 1 still has two text BGs.
    const unsigned bg_count = mode == 0 ? 4u : (mode == 1 ? 2u : 0u);
    for (unsigned bg = 0; bg < bg_count; ++bg) {
        if ((display_control & (kBgEnableBase << bg)) == 0) continue;
        for (unsigned y = 0; y < kPpuFrameHeight; ++y) {
            for (unsigned x = 0; x < kPpuFrameWidth; ++x) {
                std::uint32_t color = 0;
                std::uint16_t rank = 0;
                if (!RenderTextBackgroundPixel(bg, x, y, &color, &rank)) continue;
                const std::size_t index = static_cast<std::size_t>(y) * kPpuFrameWidth + x;
                if (rank < pixels_[index].rank) {
                    pixels_[index].rank = rank;
                    pixels_[index].color = color;
                }
            }
        }
    }

    if (mode <= 1) RenderObjects();
    for (std::size_t i = 0; i < kPpuFramePixels; ++i)
        output_argb[i] = pixels_[i].color;
    return true;
}

}  // namespace fe7::gba
