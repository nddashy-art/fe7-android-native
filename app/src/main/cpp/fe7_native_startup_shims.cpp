#include "gba_port.h"

#include <cstddef>
#include <cstdint>

// Native C equivalent of FE7's InitSpriteAnims routine in asm/code.s.
// The original clears the first word of each 0x28-byte animation entry in
// IWRAM beginning at 0x03000110. It does not require ARM/Thumb instructions,
// copied executable RAM, or the original GBA linker layout.
extern "C" void InitSpriteAnims(void) {
    constexpr std::uintptr_t kEntriesAddress = 0x03000110u;
    constexpr std::size_t kEntriesBytes = 0x2FCu;
    constexpr std::size_t kEntryStride = 0x28u;
    constexpr std::size_t kFirstWordOffset = 0u;

    auto* entries = static_cast<std::uint32_t*>(
        fe7_gba_resolve_pointer(kEntriesAddress, kEntriesBytes, 1));
    if (entries == nullptr) {
        return;
    }

    for (std::size_t offset = 0x2F8u;; offset -= kEntryStride) {
        entries[(offset + kFirstWordOffset) / sizeof(std::uint32_t)] = 0;
        if (offset == 0) {
            break;
        }
    }
}
