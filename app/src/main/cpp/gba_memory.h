#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace fe7::gba {

constexpr std::uint32_t kEwramBase = 0x02000000u;
constexpr std::size_t kEwramSize = 0x40000u;
constexpr std::uint32_t kIwramBase = 0x03000000u;
constexpr std::size_t kIwramSize = 0x8000u;
constexpr std::uint32_t kIoBase = 0x04000000u;
constexpr std::size_t kIoSize = 0x400u;
constexpr std::uint32_t kPaletteBase = 0x05000000u;
constexpr std::size_t kPaletteSize = 0x400u;
constexpr std::uint32_t kVramBase = 0x06000000u;
constexpr std::size_t kVramSize = 0x18000u;
constexpr std::uint32_t kOamBase = 0x07000000u;
constexpr std::size_t kOamSize = 0x400u;
constexpr std::uint32_t kRomBase = 0x08000000u;
constexpr std::uint32_t kRomLimit = 0x0E000000u;
constexpr std::uint32_t kSramBase = 0x0E000000u;
constexpr std::size_t kSramSize = 0x10000u;

// This memory bus is a portability foundation, not a complete GBA emulator.
// Register side effects, DMA, interrupts, timing and PPU behavior are separate layers.
class MemoryBus final {
public:
    MemoryBus();
    bool AttachRom(const std::uint8_t* data, std::size_t size);
    void DetachRom();
    std::size_t RomSize() const { return rom_size_; }

    // The ROM is borrowed; the caller owns its lifetime. No ROM is bundled/copied.
    const std::uint8_t* ResolveRead(std::uint32_t address, std::size_t length) const;
    std::uint8_t* ResolveWrite(std::uint32_t address, std::size_t length);
    volatile void* ResolveIoRegister(std::uint32_t address, std::size_t length);

    bool Read8(std::uint32_t address, std::uint8_t* out) const;
    bool Read16(std::uint32_t address, std::uint16_t* out) const;
    bool Read32(std::uint32_t address, std::uint32_t* out) const;
    bool Write8(std::uint32_t address, std::uint8_t value);
    bool Write16(std::uint32_t address, std::uint16_t value);
    bool Write32(std::uint32_t address, std::uint32_t value);
    void Reset(bool clear_ewram);

    const std::uint8_t* SramData() const { return sram_.data(); }
    std::uint8_t* SramData() { return sram_.data(); }

private:
    template <typename T, std::size_t N>
    static T* ResolveArray(std::array<T, N>& bytes, std::uint32_t address,
                           std::uint32_t base, std::size_t length);
    template <typename T, std::size_t N>
    static const T* ResolveArray(const std::array<T, N>& bytes, std::uint32_t address,
                                 std::uint32_t base, std::size_t length);

    std::array<std::uint8_t, kEwramSize> ewram_{};
    std::array<std::uint8_t, kIwramSize> iwram_{};
    alignas(8) std::array<std::uint8_t, kIoSize> io_{};
    std::array<std::uint8_t, kPaletteSize> palette_{};
    std::array<std::uint8_t, kVramSize> vram_{};
    std::array<std::uint8_t, kOamSize> oam_{};
    std::array<std::uint8_t, kSramSize> sram_{};
    const std::uint8_t* rom_ = nullptr;
    std::size_t rom_size_ = 0;
};

}  // namespace fe7::gba
