#include "gba_port.h"

#include <cstdint>

namespace {
constexpr std::uint32_t kDispStatAddress = 0x04000004u;
constexpr std::uint32_t kVCountAddress = 0x04000006u;
constexpr unsigned kVisibleScanlines = 160;
constexpr unsigned kTotalScanlines = 228;
constexpr std::uint16_t kVBlankFlag = 1u << 0;
constexpr std::uint16_t kHBlankFlag = 1u << 1;
constexpr std::uint16_t kVCountFlag = 1u << 2;
constexpr std::uint16_t kVBlankIrqEnable = 1u << 3;
constexpr std::uint16_t kHBlankIrqEnable = 1u << 4;
constexpr std::uint16_t kVCountIrqEnable = 1u << 5;
constexpr std::uint16_t kVCountCompareMask = 0xFF00u;
constexpr std::uint16_t kStatusMask = kVBlankFlag | kHBlankFlag | kVCountFlag;

volatile std::uint16_t* Io16(std::uint32_t address) {
    return static_cast<volatile std::uint16_t*>(fe7_gba_io_pointer(address, sizeof(std::uint16_t)));
}

void SetLineState(volatile std::uint16_t* dispstat, volatile std::uint16_t* vcount,
                  std::uint16_t configuration, unsigned line) {
    const auto compare = static_cast<unsigned>((configuration & kVCountCompareMask) >> 8);
    const bool vblank = line >= kVisibleScanlines;
    const bool match = line == compare;
    std::uint16_t status = configuration;
    if (vblank) status |= kVBlankFlag;
    if (match) status |= kVCountFlag;
    *vcount = static_cast<std::uint16_t>(line);
    *dispstat = status;
}
} // namespace

// Advances one logical GBA frame without sleeping. This is deterministic host
// timing, not cycle-accurate emulation: it delivers the visible HBlank DMA/IRQ
// events and one VBlank event so an eventual native FE7 frame loop can progress.
extern "C" void fe7_gba_advance_frame(void) {
    auto* dispstat = Io16(kDispStatAddress);
    auto* vcount = Io16(kVCountAddress);
    if (dispstat == nullptr || vcount == nullptr) return;

    const std::uint16_t configuration =
        static_cast<std::uint16_t>(*dispstat & ~kStatusMask);

    for (unsigned line = 0; line < kTotalScanlines; ++line) {
        SetLineState(dispstat, vcount, configuration, line);

        if (line < kVisibleScanlines) {
            *dispstat = static_cast<std::uint16_t>(configuration |
                (line == 0 ? 0 : 0) |
                (((configuration & kVCountCompareMask) >> 8) == line ? kVCountFlag : 0));
            *dispstat = static_cast<std::uint16_t>(*dispstat | kHBlankFlag);
            fe7_gba_dma_on_hblank(line);
            if ((configuration & kHBlankIrqEnable) != 0)
                fe7_gba_irq_raise(1u << 1);
            *dispstat = static_cast<std::uint16_t>(*dispstat & ~kHBlankFlag);
        } else {
            *dispstat = static_cast<std::uint16_t>(*dispstat | kHBlankFlag);
            if ((configuration & kHBlankIrqEnable) != 0)
                fe7_gba_irq_raise(1u << 1);
            *dispstat = static_cast<std::uint16_t>(*dispstat & ~kHBlankFlag);
        }

        const auto compare = static_cast<unsigned>((configuration & kVCountCompareMask) >> 8);
        if (line == compare && (configuration & kVCountIrqEnable) != 0)
            fe7_gba_irq_raise(1u << 2);

        if (line == kVisibleScanlines) {
            fe7_gba_dma_on_vblank();
            if ((configuration & kVBlankIrqEnable) != 0)
                fe7_gba_irq_raise(1u << 0);
        }
    }

    // Return to the beginning-of-frame state. VCOUNT=0's compare event is
    // delivered at the next frame's line-0 phase, avoiding a duplicate IRQ.
    *vcount = 0;
    *dispstat = configuration;
}
