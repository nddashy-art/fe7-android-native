#include "gba_bios_compat.h"

#include <cstring>
#include <limits>

namespace fe7::bios {
namespace {
bool ValidBuffers(const void* destination, const void* source, std::size_t bytes) {
    return bytes == 0 || (destination != nullptr && source != nullptr);
}
}

bool CpuSet(void* destination, const void* source, std::uint32_t control) {
    const std::size_t count = control & kCpuSetCountMask;
    const std::size_t unit = (control & kCpuSetWord) ? sizeof(std::uint32_t) : sizeof(std::uint16_t);
    if (count > std::numeric_limits<std::size_t>::max() / unit) return false;
    const std::size_t bytes = count * unit;
    if (!ValidBuffers(destination, source, bytes)) return false;

    auto* dst = static_cast<std::uint8_t*>(destination);
    const auto* src = static_cast<const std::uint8_t*>(source);
    if (control & kCpuSetFill) {
        // BIOS fill repeats the first halfword/word from source.
        if (bytes == 0) return true;
        if (src == nullptr) return false;
        for (std::size_t offset = 0; offset < bytes; offset += unit)
            std::memcpy(dst + offset, src, unit);
    } else if (bytes != 0) {
        std::memmove(dst, src, bytes);
    }
    return true;
}

bool CpuFastSet(void* destination, const void* source, std::uint32_t control) {
    // CpuFastSet transfers words in groups of eight. The count is rounded up
    // to a multiple of eight words by the BIOS contract.
    const std::size_t words = control & kCpuSetCountMask;
    if (words > std::numeric_limits<std::size_t>::max() - 7) return false;
    const std::size_t rounded_words = (words + 7u) & ~std::size_t(7u);
    if (rounded_words > std::numeric_limits<std::size_t>::max() / sizeof(std::uint32_t)) return false;
    const std::size_t bytes = rounded_words * sizeof(std::uint32_t);
    if (!ValidBuffers(destination, source, bytes)) return false;

    auto* dst = static_cast<std::uint8_t*>(destination);
    const auto* src = static_cast<const std::uint8_t*>(source);
    if (control & kCpuSetFill) {
        if (bytes == 0) return true;
        for (std::size_t offset = 0; offset < bytes; offset += sizeof(std::uint32_t))
            std::memcpy(dst + offset, src, sizeof(std::uint32_t));
    } else if (bytes != 0) {
        std::memmove(dst, src, bytes);
    }
    return true;
}

bool Lz77UnCompWram(const std::uint8_t* input, std::size_t input_size,
                    std::uint8_t* output, std::size_t output_capacity,
                    std::size_t* output_size) {
    if (output_size != nullptr) *output_size = 0;
    if (input == nullptr || output == nullptr || input_size < 4 || input[0] != 0x10) return false;

    const std::size_t decoded_size = static_cast<std::size_t>(input[1]) |
        (static_cast<std::size_t>(input[2]) << 8) |
        (static_cast<std::size_t>(input[3]) << 16);
    if (decoded_size > output_capacity) return false;

    std::size_t in = 4;
    std::size_t out = 0;
    while (out < decoded_size) {
        if (in >= input_size) return false;
        const std::uint8_t flags = input[in++];
        for (unsigned bit = 0; bit < 8 && out < decoded_size; ++bit) {
            if ((flags & (0x80u >> bit)) == 0) {
                if (in >= input_size) return false;
                output[out++] = input[in++];
                continue;
            }
            if (in + 1 >= input_size) return false;
            const std::uint8_t a = input[in++];
            const std::uint8_t b = input[in++];
            const std::size_t length = static_cast<std::size_t>((a >> 4) + 3);
            const std::size_t displacement =
                (static_cast<std::size_t>(a & 0x0Fu) << 8 | b) + 1;
            if (displacement > out || length > decoded_size - out) return false;
            for (std::size_t i = 0; i < length; ++i) {
                output[out] = output[out - displacement];
                ++out;
            }
        }
    }
    if (output_size != nullptr) *output_size = out;
    return true;
}

}  // namespace fe7::bios
