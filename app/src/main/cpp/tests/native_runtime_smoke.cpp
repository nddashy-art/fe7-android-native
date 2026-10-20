#include "fe7_engine_startup.h"
#include "gba_header_parser.h"
#include "gba_memory.h"
#include "gba_ppu.h"
#include "gba_port.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

extern "C" void InitSpriteAnims(void);

int main() {
    using fe7::gba::MemoryBus;

    // Check bounded RAM access and ROM mapping independently of Android/mGBA.
    MemoryBus memory;
    assert(memory.Write32(fe7::gba::kEwramBase, 0x12345678u));
    std::uint32_t value = 0;
    assert(memory.Read32(fe7::gba::kEwramBase, &value));
    assert(value == 0x12345678u);
    assert(!memory.Read32(0x01000000u, &value));

    std::vector<std::uint8_t> rom(0xC0, 0);
    constexpr char kTitle[] = "FIRE EMBLEM";
    constexpr char kGameCode[] = "AE7E";
    std::memcpy(rom.data() + 0xA0, kTitle, sizeof(kTitle) - 1);
    std::memcpy(rom.data() + 0xAC, kGameCode, sizeof(kGameCode) - 1);

    fe7::GbaHeader header;
    assert(fe7::ParseGbaHeader(rom.data(), rom.size(), &header));
    assert(header.looks_like_fire_emblem);
    assert(header.game_code == kGameCode);

    // The current native startup contract is deliberately preparation-only:
    // it validates the header and attaches ROM memory, but does not run AgbMain.
    assert(fe7_engine_prepare(rom.data(), rom.size()) == FE7_ENGINE_BOOT_RUNTIME_PREPARED);
    assert(fe7_engine_boot_state() == FE7_ENGINE_BOOT_RUNTIME_PREPARED);
    const auto* mapped_rom = static_cast<const std::uint8_t*>(
        fe7_gba_resolve_pointer(0x08000000u, 4, 0));
    assert(mapped_rom == rom.data());
    assert(mapped_rom[0xA0] == 'F');

    // Validate the first real FE7 startup routine ported from ARM assembly:
    // InitSpriteAnims clears one 32-bit field every 0x28 bytes in IWRAM.
    auto* animation_entries = static_cast<std::uint32_t*>(
        fe7_gba_resolve_pointer(0x03000110u, 0x2FCu, 1));
    assert(animation_entries != nullptr);
    std::memset(animation_entries, 0xA5, 0x2FCu);
    InitSpriteAnims();
    for (std::size_t offset = 0; offset <= 0x2F8u; offset += 0x28u) {
        assert(animation_entries[offset / sizeof(std::uint32_t)] == 0u);
        if (offset + sizeof(std::uint32_t) < 0x2FCu) {
            assert(animation_entries[(offset + sizeof(std::uint32_t)) /
                                     sizeof(std::uint32_t)] == 0xA5A5A5A5u);
        }
    }

    assert(fe7_gba_render_frame(nullptr, 0) == 0);

    // Exercise the software PPU with synthetic VRAM/palette data. This
    // proves rendering from GBA memory works; it does not emulate a CPU.
    fe7_gba_reset(1);
    auto* display_control = static_cast<volatile std::uint16_t*>(
        fe7_gba_io_pointer(0x04000000u, sizeof(std::uint16_t)));
    auto* bg0_control = static_cast<volatile std::uint16_t*>(
        fe7_gba_io_pointer(0x04000008u, sizeof(std::uint16_t)));
    auto* palette = static_cast<std::uint16_t*>(
        fe7_gba_resolve_pointer(0x05000000u, 0x400u, 1));
    auto* vram = static_cast<std::uint8_t*>(
        fe7_gba_resolve_pointer(0x06000000u, 0x18000u, 1));
    auto* oam = static_cast<std::uint16_t*>(
        fe7_gba_resolve_pointer(0x07000000u, 0x400u, 1));
    assert(display_control != nullptr && bg0_control != nullptr);
    assert(palette != nullptr && vram != nullptr && oam != nullptr);
    *display_control = static_cast<std::uint16_t>(1u << 8); // BG0 enabled, mode 0.
    *bg0_control = static_cast<std::uint16_t>(31u << 8);   // Map in screen block 31.
    palette[1] = 0x001Fu;  // Red in BGR555.
    palette[2] = 0x7C00u; // Blue in BGR555.
    std::fill(vram, vram + 32, 0x11u);
    std::fill(vram + 32, vram + 64, 0x22u);
    auto* tile_map = reinterpret_cast<std::uint16_t*>(vram + 31u * 0x800u);
    for (unsigned y = 0; y < 32; ++y) {
        for (unsigned x = 0; x < 32; ++x) {
            tile_map[y * 32u + x] =
                static_cast<std::uint16_t>(((x / 4u + y / 4u) & 1u) ? 1u : 0u);
        }
    }
    std::fill(oam, oam + 128u * 4u, 0u);
    for (unsigned i = 0; i < 128; ++i) oam[i * 4u] = 0x0200u;

    std::array<std::uint32_t, fe7::gba::kPpuFramePixels> frame{};
    assert(fe7_gba_render_frame(frame.data(), frame.size()) == 1);
    assert(frame[0] == 0xFFFF0000u);
    assert(frame[32] == 0xFF0000FFu);

    fe7_engine_shutdown();
    assert(fe7_engine_boot_state() == FE7_ENGINE_BOOT_NO_ROM);
    assert(fe7_gba_resolve_pointer(0x08000000u, 4, 0) == nullptr);

    std::vector<std::uint8_t> invalid_rom(0xC0, 0);
    std::memcpy(invalid_rom.data() + 0xA0, "NOT A GAME", 10);
    assert(fe7_engine_prepare(invalid_rom.data(), invalid_rom.size()) == FE7_ENGINE_BOOT_INVALID_ROM);
    fe7_engine_shutdown();

    std::cout << "Native GBA runtime smoke tests passed (header, memory, InitSpriteAnims, PPU; no FE7 CPU/game loop).\\n";
    return 0;
}
