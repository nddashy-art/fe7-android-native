#include "gba_dma.h"

#include "gba_port.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace {
constexpr unsigned kDmaChannelCount = 4;
constexpr std::uint32_t kDmaCountMask = 0xFFFFu;
constexpr std::uint16_t kDestModeMask = 0x0060u;
constexpr std::uint16_t kSourceModeMask = 0x0180u;
constexpr std::uint16_t kRepeat = 0x0200u;
constexpr std::uint16_t kWordTransfer = 0x0400u;
constexpr std::uint16_t kStartMask = 0x3000u;
constexpr std::uint16_t kStartVBlank = 0x1000u;
constexpr std::uint16_t kStartHBlank = 0x2000u;
constexpr std::uint16_t kStartSpecial = 0x3000u;
constexpr std::uint16_t kEnable = 0x8000u;

struct Channel {
    bool active = false;
    std::uintptr_t source = 0;
    std::uintptr_t destination = 0;
    std::uintptr_t original_destination = 0;
    std::uint32_t control = 0;
    std::size_t count = 0;
    std::size_t unit = 2;
};
Channel g_channels[kDmaChannelCount];

std::uint16_t ControlHigh(const Channel& c) {
    return static_cast<std::uint16_t>(c.control >> 16);
}
unsigned StartMode(const Channel& c) {
    return ControlHigh(c) & kStartMask;
}
unsigned SourceMode(const Channel& c) {
    return ControlHigh(c) & kSourceModeMask;
}
unsigned DestinationMode(const Channel& c) {
    return ControlHigh(c) & kDestModeMask;
}

void* Resolve(std::uintptr_t address, std::size_t length, bool write_access) {
    return fe7_gba_resolve_pointer(address, length, write_access ? 1 : 0);
}

bool Transfer(Channel& c) {
    if (!c.active || c.count == 0) return false;
    const unsigned src_mode = SourceMode(c);
    const unsigned dst_mode = DestinationMode(c);
    // Source mode 3 is prohibited by the GBA hardware; do not dereference it.
    if (src_mode == 0x0180u) return false;

    for (std::size_t i = 0; i < c.count; ++i) {
        auto* src = static_cast<const std::uint8_t*>(Resolve(c.source, c.unit, false));
        auto* dst = static_cast<std::uint8_t*>(Resolve(c.destination, c.unit, true));
        if (src == nullptr || dst == nullptr) return false;
        std::uint8_t value[4]{};
        std::memcpy(value, src, c.unit);
        std::memcpy(dst, value, c.unit);

        if (src_mode == 0x0000u) c.source += c.unit;        // increment
        else if (src_mode == 0x0080u) c.source -= c.unit;   // decrement
        // 0x0100 = fixed. Source mode 3 was rejected above.
        if (dst_mode == 0x0000u || dst_mode == 0x0060u) c.destination += c.unit;
        else if (dst_mode == 0x0020u) c.destination -= c.unit;
        // 0x0040 = fixed.
    }
    return true;
}

void Trigger(unsigned start_mode) {
    for (auto& c : g_channels) {
        if (!c.active || StartMode(c) != start_mode) continue;
        const bool repeated = (ControlHigh(c) & kRepeat) != 0 &&
                              (start_mode == kStartVBlank || start_mode == kStartHBlank);
        const auto original_destination = c.original_destination;
        const bool ok = Transfer(c);
        if (!ok || !repeated) {
            c.active = false;
        } else if (DestinationMode(c) == kDestModeMask) {
            // DEST_RELOAD increments during a transfer, then reloads at the
            // next trigger; source continues from its post-transfer address.
            c.destination = original_destination;
        }
    }
}
}

extern "C" void fe7_gba_dma_set(unsigned channel, const void* source,
                                  void* destination, std::uint32_t control) {
    if (channel >= kDmaChannelCount) return;
    Channel& c = g_channels[channel];
    const std::uint16_t flags = static_cast<std::uint16_t>(control >> 16);
    if ((flags & kEnable) == 0) {
        c.active = false;
        return;
    }

    c.control = control;
    c.source = reinterpret_cast<std::uintptr_t>(source);
    c.destination = reinterpret_cast<std::uintptr_t>(destination);
    c.original_destination = c.destination;
    c.count = control & kDmaCountMask;
    c.unit = (flags & kWordTransfer) != 0 ? 4u : 2u;
    c.active = c.count != 0;

    if (!c.active) return;  // Count 0 is left as a no-op until channel-specific limits are modeled.
    switch (flags & kStartMask) {
        case 0x0000u:
            (void)Transfer(c);
            c.active = false; // immediate transfers are one-shot
            break;
        case kStartVBlank:
        case kStartHBlank:
            // Deferred until the host frame/scanline layer delivers this event.
            break;
        case kStartSpecial:
            // FIFO/audio special timing needs the audio backend and is not modeled yet.
            c.active = false;
            break;
        default:
            c.active = false;
            break;
    }
}

extern "C" void fe7_gba_dma_on_vblank(void) { Trigger(kStartVBlank); }
extern "C" void fe7_gba_dma_on_hblank(unsigned scanline) {
    (void)scanline;
    Trigger(kStartHBlank);
}
