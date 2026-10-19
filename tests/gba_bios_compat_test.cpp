#include "gba_bios_compat.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>

int main() {
    using namespace fe7::bios;
    std::array<std::uint32_t, 16> src{};
    std::array<std::uint32_t, 16> dst{};
    for (std::size_t i = 0; i < src.size(); ++i) src[i] = static_cast<std::uint32_t>(i + 1);

    assert(CpuSet(dst.data(), src.data(), 4u | kCpuSetWord));
    for (std::size_t i = 0; i < 4; ++i) assert(dst[i] == src[i]);

    dst.fill(0);
    assert(CpuSet(dst.data(), src.data(), 5u | kCpuSetWord | kCpuSetFill));
    for (auto value : dst) assert(value == src[0]);

    dst.fill(0);
    assert(CpuFastSet(dst.data(), src.data(), 9u));
    for (std::size_t i = 0; i < 16; ++i) assert(dst[i] == src[i]);

    // LZ77 header + one flag byte + three literal bytes ("ABC").
    const std::uint8_t packed[] = {0x10, 3, 0, 0, 0x00, 'A', 'B', 'C'};
    std::array<std::uint8_t, 3> decoded{};
    std::size_t decoded_size = 0;
    assert(Lz77UnCompWram(packed, sizeof(packed), decoded.data(), decoded.size(), &decoded_size));
    assert(decoded_size == 3 && decoded[0] == 'A' && decoded[1] == 'B' && decoded[2] == 'C');

    // Back-reference: "ABCABC", literals A/B/C then length-3, displacement-3.
    const std::uint8_t repeated[] = {0x10, 6, 0, 0, 0x10, 'A', 'B', 'C', 0x00, 0x02};
    std::array<std::uint8_t, 6> decoded_repeated{};
    assert(Lz77UnCompWram(repeated, sizeof(repeated), decoded_repeated.data(),
                          decoded_repeated.size(), &decoded_size));
    assert(decoded_size == 6);
    for (std::size_t i = 0; i < 6; ++i) assert(decoded_repeated[i] == "ABCABC"[i]);

    assert(!Lz77UnCompWram(packed, sizeof(packed), decoded.data(), 2, nullptr));
    std::puts("GBA BIOS compatibility tests passed");
    return 0;
}
