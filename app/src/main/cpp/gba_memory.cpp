#include "gba_memory.h"

#include <algorithm>
#include <cstring>
#include <utility>

#if defined(__unix__) || defined(__ANDROID__)
#include <sys/mman.h>
#include <unistd.h>
#ifndef MAP_FIXED_NOREPLACE
// Linux kernels that do not recognize this flag treat it as a hint; we verify
// the returned address below and unmap a non-matching result.
#define MAP_FIXED_NOREPLACE 0x100000
#endif
#endif

namespace fe7::gba {
namespace {
constexpr std::size_t kGbaRomBankSize = 0x02000000u;
constexpr std::size_t kGbaRomMappedSize = 0x06000000u;
constexpr std::size_t kIoPageSize = 0x1000u;
constexpr std::size_t kPalettePageSize = 0x1000u;
constexpr std::size_t kOamPageSize = 0x1000u;

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

#if defined(__unix__) || defined(__ANDROID__)
struct FixedRegion {
    std::uintptr_t address;
    std::size_t size;
    void* mapping = nullptr;
};
std::array<FixedRegion, 8> g_fixed_regions{{
    {kEwramBase, kEwramSize, nullptr},
    {kIwramBase, kIwramSize, nullptr},
    {kIoBase, kIoPageSize, nullptr},
    {kPaletteBase, kPalettePageSize, nullptr},
    {kVramBase, kVramSize, nullptr},
    {kOamBase, kOamPageSize, nullptr},
    {kRomBase, kGbaRomMappedSize, nullptr},
    {kSramBase, kSramSize, nullptr},
}};
MemoryBus* g_fixed_map_owner = nullptr;

void UnmapFixedRegions() {
    for (auto& region : g_fixed_regions) {
        if (region.mapping != nullptr) {
            (void)munmap(region.mapping, region.size);
            region.mapping = nullptr;
        }
    }
}
#endif
}  // namespace

MemoryBus::MemoryBus() {
    Reset(true);
}

MemoryBus::~MemoryBus() {
#if defined(__unix__) || defined(__ANDROID__)
    if (g_fixed_map_owner == this) {
        UnmapFixedRegions();
        g_fixed_map_owner = nullptr;
    }
#endif
}

bool MemoryBus::EnableFixedAddressMapping() {
    if (fixed_address_mapping_) return true;
#if defined(__unix__) || defined(__ANDROID__)
    // The original FE7 ARM routines dereference GBA addresses directly.
    // Mapping those addresses is optional and is deliberately fail-closed:
    // never overwrite an address range already used by the Android process.
    if (g_fixed_map_owner != nullptr || rom_size_ > kGbaRomBankSize) return false;

    for (auto& region : g_fixed_regions) {
        void* wanted = reinterpret_cast<void*>(region.address);
        void* mapped = mmap(wanted, region.size, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (mapped == MAP_FAILED || mapped != wanted) {
            if (mapped != MAP_FAILED) (void)munmap(mapped, region.size);
            UnmapFixedRegions();
            return false;
        }
        region.mapping = mapped;
    }

    // Preserve this MemoryBus' existing state when switching representations.
    std::memcpy(reinterpret_cast<void*>(kEwramBase), ewram_.data(), ewram_.size());
    std::memcpy(reinterpret_cast<void*>(kIwramBase), iwram_.data(), iwram_.size());
    std::memcpy(reinterpret_cast<void*>(kIoBase), io_.data(), io_.size());
    std::memcpy(reinterpret_cast<void*>(kPaletteBase), palette_.data(), palette_.size());
    std::memcpy(reinterpret_cast<void*>(kVramBase), vram_.data(), vram_.size());
    std::memcpy(reinterpret_cast<void*>(kOamBase), oam_.data(), oam_.size());
    std::memcpy(reinterpret_cast<void*>(kSramBase), sram_.data(), sram_.size());
    if (rom_ != nullptr && rom_size_ != 0) {
        for (std::size_t bank = 0; bank < kGbaRomMappedSize; bank += kGbaRomBankSize) {
            const auto copied = std::min(rom_size_, kGbaRomBankSize);
            std::memcpy(reinterpret_cast<std::uint8_t*>(kRomBase) + bank, rom_, copied);
        }
    }

    fixed_address_mapping_ = true;
    g_fixed_map_owner = this;
    return true;
#else
    return false;
#endif
}

bool MemoryBus::AttachRom(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size == 0) return false;
#if defined(__unix__) || defined(__ANDROID__)
    if (fixed_address_mapping_ && size > kGbaRomBankSize) return false;
#endif
    rom_ = data;
    rom_size_ = size;
#if defined(__unix__) || defined(__ANDROID__)
    if (fixed_address_mapping_) {
        for (std::size_t bank = 0; bank < kGbaRomMappedSize; bank += kGbaRomBankSize) {
            std::memcpy(reinterpret_cast<std::uint8_t*>(kRomBase) + bank, data, size);
        }
    }
#endif
    return true;
}

void MemoryBus::DetachRom() {
#if defined(__unix__) || defined(__ANDROID__)
    if (fixed_address_mapping_ && rom_size_ != 0 && rom_size_ <= kGbaRomBankSize) {
        for (std::size_t bank = 0; bank < kGbaRomMappedSize; bank += kGbaRomBankSize)
            std::memset(reinterpret_cast<std::uint8_t*>(kRomBase) + bank, 0, rom_size_);
    }
#endif
    rom_ = nullptr;
    rom_size_ = 0;
}

const std::uint8_t* MemoryBus::SramData() const {
    return fixed_address_mapping_ ? reinterpret_cast<const std::uint8_t*>(kSramBase) : sram_.data();
}
std::uint8_t* MemoryBus::SramData() {
    return fixed_address_mapping_ ? reinterpret_cast<std::uint8_t*>(kSramBase) : sram_.data();
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

std::size_t MemoryBus::ReadableBytesFrom(std::uint32_t address) const {
    const auto remaining = [address](std::uint32_t base, std::size_t size) -> std::size_t {
        if (address < base) return 0;
        const std::uint64_t offset = static_cast<std::uint64_t>(address) - base;
        return offset < size ? size - static_cast<std::size_t>(offset) : 0;
    };
    for (const auto& region : {std::pair<std::uint32_t, std::size_t>{kEwramBase, kEwramSize},
                              {kIwramBase, kIwramSize}, {kIoBase, kIoSize},
                              {kPaletteBase, kPaletteSize}, {kVramBase, kVramSize},
                              {kOamBase, kOamSize}, {kSramBase, kSramSize}}) {
        const auto bytes = remaining(region.first, region.second);
        if (bytes != 0) return bytes;
    }
    if (rom_ != nullptr && address >= kRomBase && address < kRomLimit) {
        const std::size_t offset = static_cast<std::size_t>(address - kRomBase) % rom_size_;
        return rom_size_ - offset;
    }
    return 0;
}

std::size_t MemoryBus::WritableBytesFrom(std::uint32_t address) const {
    const auto remaining = [address](std::uint32_t base, std::size_t size) -> std::size_t {
        if (address < base) return 0;
        const std::uint64_t offset = static_cast<std::uint64_t>(address) - base;
        return offset < size ? size - static_cast<std::size_t>(offset) : 0;
    };
    for (const auto& region : {std::pair<std::uint32_t, std::size_t>{kEwramBase, kEwramSize},
                              {kIwramBase, kIwramSize}, {kIoBase, kIoSize},
                              {kPaletteBase, kPaletteSize}, {kVramBase, kVramSize},
                              {kOamBase, kOamSize}, {kSramBase, kSramSize}}) {
        const auto bytes = remaining(region.first, region.second);
        if (bytes != 0) return bytes;
    }
    return 0;
}

const std::uint8_t* MemoryBus::ResolveRead(std::uint32_t address, std::size_t length) const {
    if (fixed_address_mapping_) {
        for (const auto& region : {std::pair<std::uint32_t, std::size_t>{kEwramBase, kEwramSize},
                                  {kIwramBase, kIwramSize}, {kIoBase, kIoSize},
                                  {kPaletteBase, kPaletteSize}, {kVramBase, kVramSize},
                                  {kOamBase, kOamSize}, {kSramBase, kSramSize}}) {
            if (RangeInside(address, length, region.first, region.second))
                return reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(address));
        }
    }
    if (const auto* p = ResolveArray(ewram_, address, kEwramBase, length)) return p;
    if (const auto* p = ResolveArray(iwram_, address, kIwramBase, length)) return p;
    if (const auto* p = ResolveArray(io_, address, kIoBase, length)) return p;
    if (const auto* p = ResolveArray(palette_, address, kPaletteBase, length)) return p;
    if (const auto* p = ResolveArray(vram_, address, kVramBase, length)) return p;
    if (const auto* p = ResolveArray(oam_, address, kOamBase, length)) return p;
    if (address >= kSramBase && static_cast<std::uint64_t>(address) + length <=
        static_cast<std::uint64_t>(kSramBase) + kSramSize) {
        if (fixed_address_mapping_)
            return reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(address));
        return sram_.data() + (address - kSramBase);
    }
    if (rom_ != nullptr && address >= kRomBase && address < kRomLimit) {
        const std::size_t offset = static_cast<std::size_t>(address - kRomBase) % rom_size_;
        if (length <= rom_size_ - offset) {
            if (fixed_address_mapping_)
                return reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(kRomBase + offset));
            return rom_ + offset;
        }
    }
    return nullptr;
}

std::uint8_t* MemoryBus::ResolveWrite(std::uint32_t address, std::size_t length) {
    if (fixed_address_mapping_) {
        for (const auto& region : {std::pair<std::uint32_t, std::size_t>{kEwramBase, kEwramSize},
                                  {kIwramBase, kIwramSize}, {kIoBase, kIoSize},
                                  {kPaletteBase, kPaletteSize}, {kVramBase, kVramSize},
                                  {kOamBase, kOamSize}, {kSramBase, kSramSize}}) {
            if (RangeInside(address, length, region.first, region.second))
                return reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(address));
        }
    }
    if (auto* p = ResolveArray(ewram_, address, kEwramBase, length)) return p;
    if (auto* p = ResolveArray(iwram_, address, kIwramBase, length)) return p;
    if (auto* p = ResolveArray(io_, address, kIoBase, length)) return p;
    if (auto* p = ResolveArray(palette_, address, kPaletteBase, length)) return p;
    if (auto* p = ResolveArray(vram_, address, kVramBase, length)) return p;
    if (auto* p = ResolveArray(oam_, address, kOamBase, length)) return p;
    if (address >= kSramBase && static_cast<std::uint64_t>(address) + length <=
        static_cast<std::uint64_t>(kSramBase) + kSramSize) {
        if (fixed_address_mapping_)
            return reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(address));
        return sram_.data() + (address - kSramBase);
    }
    return nullptr;
}

volatile void* MemoryBus::ResolveIoRegister(std::uint32_t address, std::size_t length) {
    if (!RangeInside(address, length, kIoBase, kIoSize)) return nullptr;
    if (fixed_address_mapping_)
        return reinterpret_cast<volatile void*>(static_cast<std::uintptr_t>(address));
    return static_cast<volatile void*>(io_.data() + (address - kIoBase));
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
    if (clear_ewram) {
        auto* ewram = ResolveWrite(kEwramBase, kEwramSize);
        if (ewram != nullptr) std::memset(ewram, 0, kEwramSize);
    }
    auto* iwram = ResolveWrite(kIwramBase, kIwramSize);
    auto* io = ResolveWrite(kIoBase, kIoSize);
    auto* palette = ResolveWrite(kPaletteBase, kPaletteSize);
    auto* vram = ResolveWrite(kVramBase, kVramSize);
    auto* oam = ResolveWrite(kOamBase, kOamSize);
    if (iwram != nullptr) std::memset(iwram, 0, kIwramSize);
    if (io != nullptr) {
        std::memset(io, 0, kIoSize);
        // KEYINPUT is active-low; 0x03FF means every key is released.
        io[0x130] = 0xFF;
        io[0x131] = 0x03;
    }
    if (palette != nullptr) std::memset(palette, 0, kPaletteSize);
    if (vram != nullptr) std::memset(vram, 0, kVramSize);
    if (oam != nullptr) {
        std::memset(oam, 0, kOamSize);
        // Start OBJ entries hidden until the game populates its OAM buffer.
        for (std::size_t offset = 1; offset < kOamSize; offset += 8)
            oam[offset] = 0x02;
    }
    // SRAM and attached ROM intentionally survive reset.
}

}  // namespace fe7::gba
