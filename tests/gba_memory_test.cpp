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
    assert(bus.ReadableBytesFrom(fe7::gba::kEwramBase + 8) == fe7::gba::kEwramSize - 8);
    assert(bus.WritableBytesFrom(fe7::gba::kVramBase + 16) == fe7::gba::kVramSize - 16);
    assert(bus.WritableBytesFrom(fe7::gba::kRomBase) == 0);
    assert(bus.ReadableBytesFrom(0x01000000u) == 0);

    assert(bus.AttachRom(rom.data(), rom.size()));
    std::uint8_t byte = 0xFF;
    assert(bus.ReadableBytesFrom(fe7::gba::kRomBase + 5) == rom.size() - 5);
    assert(bus.WritableBytesFrom(fe7::gba::kRomBase + 5) == 0);
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

#if defined(__linux__) || defined(__ANDROID__)
    // Verify that original GBA pointer constants can safely target real
    // virtual addresses when the optional ARM32-native mode requests it.
    {
        MemoryBus mapped_bus;
        std::array<std::uint8_t, 16> mapped_rom{};
        for (std::size_t i = 0; i < mapped_rom.size(); ++i)
            mapped_rom[i] = static_cast<std::uint8_t>(0x80u + i);
        assert(mapped_bus.AttachRom(mapped_rom.data(), mapped_rom.size()));
        assert(mapped_bus.EnableFixedAddressMapping());
        assert(mapped_bus.IsFixedAddressMapped());

        auto* ewram = mapped_bus.ResolveWrite(fe7::gba::kEwramBase + 0x20u, 1);
        assert(reinterpret_cast<std::uintptr_t>(ewram) == fe7::gba::kEwramBase + 0x20u);
        *ewram = 0x6Cu;
        assert(*reinterpret_cast<volatile std::uint8_t*>(fe7::gba::kEwramBase + 0x20u) == 0x6Cu);

        const auto* rom_ptr = mapped_bus.ResolveRead(fe7::gba::kRomBase + 5u, 1);
        assert(reinterpret_cast<std::uintptr_t>(rom_ptr) == fe7::gba::kRomBase + 5u);
        assert(*reinterpret_cast<const volatile std::uint8_t*>(fe7::gba::kRomBase + 5u) == 0x85u);

        auto* io = static_cast<volatile std::uint16_t*>(
            mapped_bus.ResolveIoRegister(0x04000000u, sizeof(std::uint16_t)));
        assert(reinterpret_cast<std::uintptr_t>(const_cast<std::uint16_t*>(io)) == 0x04000000u);
        *io = 0x1234u;
        assert(*reinterpret_cast<volatile std::uint16_t*>(0x04000000u) == 0x1234u);

        auto* sram = mapped_bus.SramData();
        sram[0] = 0xA5u;
        mapped_bus.Reset(true);
        assert(*reinterpret_cast<volatile std::uint8_t*>(fe7::gba::kEwramBase + 0x20u) == 0u);
        assert(*reinterpret_cast<volatile std::uint8_t*>(fe7::gba::kSramBase) == 0xA5u);
        auto* keyinput = reinterpret_cast<volatile std::uint16_t*>(0x04000130u);
        assert(*keyinput == 0x03FFu);
    }
#endif
    std::puts("GBA memory bus tests passed");
    return 0;
}
