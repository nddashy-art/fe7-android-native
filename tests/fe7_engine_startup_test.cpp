#include "fe7_engine_startup.h"
#include "gba_memory.h"
#include "gba_port.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>

int main() {
    fe7_engine_shutdown();
    assert(fe7_engine_boot_state() == FE7_ENGINE_BOOT_NO_ROM);

    std::array<std::uint8_t, 0xC0> invalid{};
    assert(fe7_engine_prepare(invalid.data(), invalid.size()) == FE7_ENGINE_BOOT_INVALID_ROM);

    // Minimal synthetic header for a FE7-style ROM. No commercial ROM is needed.
    std::array<std::uint8_t, 0xC0> rom{};
    std::memcpy(rom.data() + 0xA0, "FIRE EMBLEM", 11);
    std::memcpy(rom.data() + 0xAC, "AE7E", 4);
    assert(fe7_engine_prepare(rom.data(), rom.size()) == FE7_ENGINE_BOOT_RUNTIME_PREPARED);
    assert(fe7_engine_boot_state() == FE7_ENGINE_BOOT_RUNTIME_PREPARED);
    assert(std::strstr(fe7_engine_boot_message(), "AgbMain") != nullptr);

    const auto* attached = static_cast<const std::uint8_t*>(
        fe7_gba_resolve_pointer(0x08000000u, 1, 0));
    assert(attached != nullptr && *attached == 0);

    fe7_engine_shutdown();
    assert(fe7_engine_boot_state() == FE7_ENGINE_BOOT_NO_ROM);
    assert(fe7_gba_resolve_pointer(0x08000000u, 1, 0) == nullptr);
    std::puts("FE7 engine startup preparation tests passed");
    return 0;
}
