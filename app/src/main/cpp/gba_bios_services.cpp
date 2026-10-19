#include "gba_port.h"
#include "gba_memory.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <thread>

namespace {
using Callback = void (*)(void);
std::atomic<Callback> g_vblank_callback{nullptr};
std::atomic<Callback> g_soft_reset_callback{nullptr};

struct BgAffineSetSrc {
    std::int32_t tex_x, tex_y;
    std::int16_t scr_x, scr_y, sx, sy;
    std::uint16_t alpha;
};
struct BgAffineSetDst {
    std::int16_t pa, pb, pc, pd;
    std::int32_t dx, dy;
};
struct ObjAffineSetSrc {
    std::int16_t x_scale, y_scale;
    std::uint16_t rotation;
};

const std::array<std::int16_t, 256>& SinTable() {
    static const auto table = [] {
        std::array<std::int16_t, 256> values{};
        constexpr double kTau = 6.283185307179586476925286766559;
        for (std::size_t i = 0; i < values.size(); ++i) {
            const double scaled = std::sin(static_cast<double>(i) * kTau / 256.0) * 16384.0;
            values[i] = static_cast<std::int16_t>(scaled >= 0.0 ? scaled + 0.5 : scaled - 0.5);
        }
        return values;
    }();
    return table;
}

std::uint8_t* ResolveWritable(std::uintptr_t pointer, std::size_t length) {
    return static_cast<std::uint8_t*>(fe7_gba_resolve_pointer(pointer, length, 1));
}
const std::uint8_t* ResolveReadable(std::uintptr_t pointer, std::size_t length) {
    return static_cast<const std::uint8_t*>(fe7_gba_resolve_pointer(pointer, length, 0));
}
void ClearRange(std::uint32_t address, std::size_t size) {
    auto* ptr = ResolveWritable(address, size);
    if (ptr != nullptr) std::memset(ptr, 0, size);
}
}

extern "C" void fe7_gba_set_vblank_callback(Callback callback) {
    g_vblank_callback.store(callback, std::memory_order_release);
}
extern "C" void fe7_gba_set_soft_reset_callback(Callback callback) {
    g_soft_reset_callback.store(callback, std::memory_order_release);
}

extern "C" std::int32_t Div(std::int32_t numerator, std::int32_t denominator) {
    if (denominator == 0) return 0;
    if (numerator == std::numeric_limits<std::int32_t>::min() && denominator == -1)
        return std::numeric_limits<std::int32_t>::min();
    return numerator / denominator;
}
extern "C" std::int32_t DivRem(std::int32_t numerator, std::int32_t denominator) {
    if (denominator == 0) return 0;
    if (numerator == std::numeric_limits<std::int32_t>::min() && denominator == -1) return 0;
    return numerator % denominator;
}
extern "C" std::uint16_t Sqrt(std::uint32_t value) {
    std::uint32_t result = 0;
    std::uint32_t bit = 1u << 30;
    while (bit > value) bit >>= 2;
    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return static_cast<std::uint16_t>(result);
}
extern "C" std::uint16_t ArcTan2(std::int16_t x, std::int16_t y) {
    if (x == 0 && y == 0) return 0;
    constexpr double kAngleScale = 32768.0 / 3.1415926535897932384626433832795;
    const double angle = std::atan2(static_cast<double>(x), static_cast<double>(y)) * kAngleScale;
    return static_cast<std::uint16_t>(static_cast<std::int32_t>(std::lround(angle)));
}

extern "C" void BgAffineSet(const void* source, void* destination, std::int32_t count) {
    if (count <= 0 || source == nullptr || destination == nullptr) return;
    const auto src_address = reinterpret_cast<std::uintptr_t>(source);
    const auto dst_address = reinterpret_cast<std::uintptr_t>(destination);
    if (static_cast<std::size_t>(count) > std::numeric_limits<std::size_t>::max() / sizeof(BgAffineSetSrc) ||
        static_cast<std::size_t>(count) > std::numeric_limits<std::size_t>::max() / sizeof(BgAffineSetDst)) return;
    const std::size_t src_bytes = static_cast<std::size_t>(count) * sizeof(BgAffineSetSrc);
    const std::size_t dst_bytes = static_cast<std::size_t>(count) * sizeof(BgAffineSetDst);
    const auto* src = reinterpret_cast<const BgAffineSetSrc*>(ResolveReadable(src_address, src_bytes));
    auto* dst = reinterpret_cast<BgAffineSetDst*>(ResolveWritable(dst_address, dst_bytes));
    if (src == nullptr || dst == nullptr) return;

    const auto& sin_table = SinTable();
    for (std::int32_t i = 0; i < count; ++i) {
        const std::uint32_t index = (src[i].alpha >> 8) & 0xFFu;
        const std::int32_t sinv = sin_table[index];
        const std::int32_t cosv = sin_table[(index + 0x40u) & 0xFFu];
        const std::int32_t pa = (static_cast<std::int32_t>(src[i].sx) * cosv) >> 14;
        const std::int32_t pb = (-static_cast<std::int32_t>(src[i].sx) * sinv) >> 14;
        const std::int32_t pc = (static_cast<std::int32_t>(src[i].sy) * sinv) >> 14;
        const std::int32_t pd = (static_cast<std::int32_t>(src[i].sy) * cosv) >> 14;
        dst[i].pa = static_cast<std::int16_t>(pa);
        dst[i].pb = static_cast<std::int16_t>(pb);
        dst[i].pc = static_cast<std::int16_t>(pc);
        dst[i].pd = static_cast<std::int16_t>(pd);
        const std::int64_t dx = static_cast<std::int64_t>(src[i].tex_x) -
            static_cast<std::int64_t>(pa) * src[i].scr_x - static_cast<std::int64_t>(pb) * src[i].scr_y;
        const std::int64_t dy = static_cast<std::int64_t>(src[i].tex_y) -
            static_cast<std::int64_t>(pc) * src[i].scr_x - static_cast<std::int64_t>(pd) * src[i].scr_y;
        dst[i].dx = static_cast<std::int32_t>(dx);
        dst[i].dy = static_cast<std::int32_t>(dy);
    }
}

extern "C" void ObjAffineSet(const void* source, void* destination, std::int32_t count, std::int32_t offset) {
    if (count <= 0 || offset <= 0 || source == nullptr || destination == nullptr) return;
    const std::size_t src_count = static_cast<std::size_t>(count);
    const std::size_t step = static_cast<std::size_t>(offset);
    if (src_count > std::numeric_limits<std::size_t>::max() / sizeof(ObjAffineSetSrc) ||
        src_count > (std::numeric_limits<std::size_t>::max() / 4u) / step) return;
    const std::size_t src_bytes = src_count * sizeof(ObjAffineSetSrc);
    const std::size_t dst_bytes = src_count * 4u * step;
    const auto* src = reinterpret_cast<const ObjAffineSetSrc*>(ResolveReadable(reinterpret_cast<std::uintptr_t>(source), src_bytes));
    auto* dst = ResolveWritable(reinterpret_cast<std::uintptr_t>(destination), dst_bytes);
    if (src == nullptr || dst == nullptr) return;

    const auto& sin_table = SinTable();
    for (std::int32_t i = 0; i < count; ++i) {
        const std::uint32_t index = (src[i].rotation >> 8) & 0xFFu;
        const std::int32_t sinv = sin_table[index];
        const std::int32_t cosv = sin_table[(index + 0x40u) & 0xFFu];
        const std::int16_t values[4] = {
            static_cast<std::int16_t>((static_cast<std::int32_t>(src[i].x_scale) * cosv) >> 14),
            static_cast<std::int16_t>((-static_cast<std::int32_t>(src[i].x_scale) * sinv) >> 14),
            static_cast<std::int16_t>((static_cast<std::int32_t>(src[i].y_scale) * sinv) >> 14),
            static_cast<std::int16_t>((static_cast<std::int32_t>(src[i].y_scale) * cosv) >> 14),
        };
        for (std::size_t j = 0; j < 4; ++j) {
            const std::size_t target = static_cast<std::size_t>(i) * 4u * step + j * step;
            dst[target] = static_cast<std::uint8_t>(values[j]);
            dst[target + 1] = static_cast<std::uint8_t>(static_cast<std::uint16_t>(values[j]) >> 8);
        }
    }
}

extern "C" void RegisterRamReset(std::uint32_t flags) {
    constexpr std::uint32_t kResetEwram = 1u << 0;
    constexpr std::uint32_t kResetIwram = 1u << 1;
    constexpr std::uint32_t kResetPalette = 1u << 2;
    constexpr std::uint32_t kResetVram = 1u << 3;
    constexpr std::uint32_t kResetOam = 1u << 4;
    constexpr std::uint32_t kResetSio = 1u << 5;
    constexpr std::uint32_t kResetSound = 1u << 6;
    constexpr std::uint32_t kResetIo = 1u << 7;
    if (flags & kResetEwram) ClearRange(fe7::gba::kEwramBase, fe7::gba::kEwramSize);
    if (flags & kResetIwram) ClearRange(fe7::gba::kIwramBase, fe7::gba::kIwramSize);
    if (flags & kResetPalette) ClearRange(fe7::gba::kPaletteBase, fe7::gba::kPaletteSize);
    if (flags & kResetVram) ClearRange(fe7::gba::kVramBase, fe7::gba::kVramSize);
    if (flags & kResetOam) ClearRange(fe7::gba::kOamBase, fe7::gba::kOamSize);
    if (flags & kResetSio) ClearRange(fe7::gba::kIoBase + 0x120u, 0x0Cu);
    if (flags & kResetSound) {
        ClearRange(fe7::gba::kIoBase + 0x060u, 0x028u);
        ClearRange(fe7::gba::kIoBase + 0x090u, 0x018u);
    }
    if (flags & kResetIo) {
        ClearRange(fe7::gba::kIoBase, fe7::gba::kIoSize);
        if (auto* keyinput = static_cast<volatile std::uint16_t*>(fe7_gba_io_pointer(0x04000130u, 2)))
            *keyinput = 0x03FFu;
    }
}

extern "C" void SoftReset(std::uint32_t flags) {
    (void)flags;
    const auto callback = g_soft_reset_callback.load(std::memory_order_acquire);
    if (callback != nullptr) {
        callback();
        return;
    }
    fe7_gba_reset(0); // preserve EWRAM/save data if no host re-entry callback exists
}
extern "C" void VBlankIntrWait(void) {
    const auto callback = g_vblank_callback.load(std::memory_order_acquire);
    if (callback != nullptr) {
        callback();
    } else {
        // Host-test fallback only; Android should install the real frame callback.
        std::this_thread::sleep_for(std::chrono::microseconds(16667));
    }
}
extern "C" void SoundBiasReset(void) {
    if (auto* bias = static_cast<volatile std::uint16_t*>(fe7_gba_io_pointer(0x04000088u, 2))) *bias = 0;
}
extern "C" void SoundBiasSet(void) {
    if (auto* bias = static_cast<volatile std::uint16_t*>(fe7_gba_io_pointer(0x04000088u, 2))) *bias = 0x0200u;
}
extern "C" void MultiBoot(const void* info) {
    (void)info; // Link-cable multiboot is unsupported by this Android runtime for now.
}
