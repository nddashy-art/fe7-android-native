#include "fe7_engine_startup.h"
#include "gba_header_parser.h"
#include "gba_memory.h"
#include "gba_port.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

int main() {
    using fe7::gba::MemoryBus;

    // Check bounded RAM access and ROM mapping independently of Android/mGBA.
    MemoryBus memory;
    assert(memory.Write32(fe7::gba::kEwramBase, 0x12345678u));
    std::uint32_t value = 0;
    assert(memory.Read32(fe7::gba::kEwramBase, &value));
    assert(value == 0x12345678u);
    assert(!memory.Read32(fe7::gba::kEwramBase + fe7::gba::kEwramSize - 2, &value));

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
    assert(fe7_gba_render_frame(nullptr, 0) == 0);

    std::array<std::uint32_t, fe7::gba::kPpuFramePixels> frame{};
    assert(fe7_gba_render_frame(frame.data(), frame.size()) == 1);
    fe7_engine_shutdown();
    assert(fe7_engine_boot_state() == FE7_ENGINE_BOOT_NO_ROM);

    std::cout << "Native GBA runtime smoke tests passed (preparation only; no FE7 CPU/game loop).\n";
    return 0;
}
