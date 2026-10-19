#include "gba_memory.h"

namespace fe7::gba {
namespace {
bool RangeInside(std::uint32_t address, std::size_t length,
                 std::uint32_t base, std::size_t region_size) {
    if (address < base) return false;
    const std::uint64_t offset = static_cast<std::uint64_t>(address) - base;
    return offset <= region_size && length <= region_size - static_cast<std::size_t>(offset);
}
std::uint32_t RotateRight32(std::uint32_t value, unsigned bits) {
    bits &= 31u;
    if (bits == 0) return value;
    return (value >> bits) | (value << (32u - bits));
}
}

bool MemoryBus::AttachRom(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size == 0) return false;
    rom_ = data;
    rom_size_ = size;
    return true;
}
void MemoryBus::DetachRom() {
    rom_ = nullptr;
    rom_size_ = 0;
}

template <typename T, std::size_t N>
T* MemoryBus::ResolveArray(std::array<T, N>& bytes, std::uint32_t address,
                           std::uint32_t base, std::size_t length) {
    if (!RangeInside(address, length, base, N)) return nullptr;
    return bytes.data() + (address - base);
}
template <typename T, std::size_t N>
const T* MemoryBus::ResolveArray(const std::array<T, N>& bytes, std::uint32_t address,
                                 std::uint32_t base, std::size_t length) {
    if (!RangeInside(address, length, base, N)) return nullptr;
    return bytes.data() + (address - base);
}

const std::uint8_t* MemoryBus::ResolveRead(std::uint32_t address, std::size_t length) const {
    if (const auto* p = ResolveArray(ewram_, address, kEwramBase, length)) return p;
    if (const auto* p = ResolveArray(iwram_, address, kIwramBase, length)) return p;
    if (const auto* p = ResolveArray(io_, address, kIoBase, length)) return p;
    if (const auto* p = ResolveArray(palette_, address, kPaletteBase, length)) return p;
    if (const auto* p = ResolveArray(vram_, address, kVramBase, length)) return p;
    if (const auto* p = ResolveArray(oam_, address, kOamBase, length)) return p;
    if (address >= kSramBase && static_cast<std::uint64_t>(address) + length <=
        static_cast<std::uint64_t>(kSramBase) + kSramSize)
        return sram_.data() + (address - kSramBase);
    if (rom_ != nullptr && address >= kRomBase && address < kRomLimit) {
        const std::size_t offset = static_cast<std::size_t>(address - kRomBase) % rom_size_;
        if (length <= rom_size_ - offset) return rom_ + offset;
    }
    return nullptr;
}

std::uint8_t* MemoryBus::ResolveWrite(std::uint32_t address, std::size_t length) {
    if (auto* p = ResolveArray(ewram_, address, kEwramBase, length)) return p;
    if (auto* p = ResolveArray(iwram_, address, kIwramBase, length)) return p;
    if (auto* p = ResolveArray(io_, address, kIoBase, length)) return p;
    if (auto* p = ResolveArray(palette_, address, kPaletteBase, length)) return p;
    if (auto* p = ResolveArray(vram_, address, kVramBase, length)) return p;
    if (auto* p = ResolveArray(oam_, address, kOamBase, length)) return p;
    if (address >= kSramBase && static_cast<std::uint64_t>(address) + length <=
        static_cast<std::uint64_t>(kSramBase) + kSramSize)
        return sram_.data() + (address - kSramBase);
    return nullptr;
}

bool MemoryBus::Read8(std::uint32_t address, std::uint8_t* out) const {
    if (out == nullptr) return false;
    const auto* p = ResolveRead(address, 1);
    if (p == nullptr) return false;
    *out = *p;
    return true;
}
bool MemoryBus::Read16(std::uint32_t address, std::uint16_t* out) const {
    if (out == nullptr) return false;
    address &= ~std::uint32_t(1);
    std::uint8_t b0 = 0, b1 = 0;
    if (!Read8(address, &b0) || !Read8(address + 1, &b1)) return false;
    *out = static_cast<std::uint16_t>(b0 | (static_cast<std::uint16_t>(b1) << 8));
    return true;
}
bool MemoryBus::Read32(std::uint32_t address, std::uint32_t* out) const {
    if (out == nullptr) return false;
    const unsigned rotate = (address & 3u) * 8u;
    address &= ~std::uint32_t(3);
    std::uint8_t b[4]{};
    for (unsigned i = 0; i < 4; ++i) if (!Read8(address + i, &b[i])) return false;
    const std::uint32_t aligned = static_cast<std::uint32_t>(b[0]) |
        (static_cast<std::uint32_t>(b[1]) << 8) |
        (static_cast<std::uint32_t>(b[2]) << 16) |
        (static_cast<std::uint32_t>(b[3]) << 24);
    *out = RotateRight32(aligned, rotate);
    return true;
}

bool MemoryBus::Write8(std::uint32_t address, std::uint8_t value) {
    if (address >= kOamBase && address < kOamBase + kOamSize) return true;
    if ((address >= kPaletteBase && address < kPaletteBase + kPaletteSize) ||
        (address >= kVramBase && address < kVramBase + kVramSize)) {
        auto* p = ResolveWrite(address & ~std::uint32_t(1), 2);
        if (p == nullptr) return false;
        p[0] = value;
        p[1] = value;
        return true;
    }
    auto* p = ResolveWrite(address, 1);
    if (p == nullptr) return false;
    *p = value;
    return true;
}
bool MemoryBus::Write16(std::uint32_t address, std::uint16_t value) {
    address &= ~std::uint32_t(1);
    auto* p = ResolveWrite(address, 2);
    if (p == nullptr) return false;
    p[0] = static_cast<std::uint8_t>(value);
    p[1] = static_cast<std::uint8_t>(value >> 8);
    return true;
}
bool MemoryBus::Write32(std::uint32_t address, std::uint32_t value) {
    address &= ~std::uint32_t(3);
    auto* p = ResolveWrite(address, 4);
    if (p == nullptr) return false;
    for (unsigned i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(value >> (i * 8));
    return true;
}

void MemoryBus::Reset(bool clear_ewram) {
    if (clear_ewram) ewram_.fill(0);
    iwram_.fill(0);
    io_.fill(0);
    palette_.fill(0);
    vram_.fill(0);
    oam_.fill(0);
    // SRAM and attached ROM intentionally survive reset.
}

}  // namespace fe7::gba
