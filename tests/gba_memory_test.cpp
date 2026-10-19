#include "gba_memory.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>

int main() {
    using fe7::gba::MemoryBus;
    MemoryBus bus;

    assert(bus.Write8(fe7::gba::kEwramBase, 0x12));
    assert(bus.Write16(fe7::gba::kEwramBase + 1, 0xABCD));
    std::uint16_t half = 0;
    assert(bus.Read16(fe7::gba::kEwramBase + 1, &half) && half == 0xABCD);

    assert(bus.Write32(fe7::gba::kIwramBase + 3, 0x12345678));
    std::uint32_t word = 0;
    assert(bus.Read32(fe7::gba::kIwramBase, &word) && word == 0x12345678);
    assert(bus.Read32(fe7::gba::kIwramBase + 1, &word) && word == 0x78123456);

    assert(bus.Write8(fe7::gba::kVramBase + 1, 0x5A));
    std::uint16_t vram = 0;
    assert(bus.Read16(fe7::gba::kVramBase, &vram) && vram == 0x5A5A);
    assert(bus.Write8(fe7::gba::kOamBase, 0xFF));
    std::uint8_t oam = 1;
    assert(bus.Read8(fe7::gba::kOamBase, &oam) && oam == 0);

    std::array<std::uint8_t, 16> rom{};
    for (std::size_t i = 0; i < rom.size(); ++i) rom[i] = static_cast<std::uint8_t>(i);
    assert(bus.AttachRom(rom.data(), rom.size()));
    std::uint8_t byte = 0xFF;
    assert(bus.Read8(fe7::gba::kRomBase + 5, &byte) && byte == 5);
    assert(bus.Read8(fe7::gba::kRomBase + 18, &byte) && byte == 2);
    assert(!bus.Write8(fe7::gba::kRomBase, 1));

    assert(bus.Write8(fe7::gba::kSramBase, 0xA5));
    bus.Reset(true);
    assert(bus.Read8(fe7::gba::kSramBase, &byte) && byte == 0xA5);
    assert(bus.Read8(fe7::gba::kEwramBase, &byte) && byte == 0);
    assert(bus.Read8(fe7::gba::kRomBase + 5, &byte) && byte == 5);

    assert(bus.ResolveRead(fe7::gba::kVramBase + fe7::gba::kVramSize - 2, 2) != nullptr);
    assert(bus.ResolveRead(fe7::gba::kVramBase + fe7::gba::kVramSize - 2, 4) == nullptr);
    std::puts("GBA memory bus tests passed");
    return 0;
}
