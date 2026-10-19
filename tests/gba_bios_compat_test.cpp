#include "gba_bios_compat.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>

int main() {
    using namespace fe7::bios;
    std::array<std::uint32_t, 16> source{};
    std::array<std::uint32_t, 16> destination{};
    for (std::size_t i = 0; i < source.size(); ++i) source[i] = static_cast<std::uint32_t>(i + 1);

    assert(CpuSet(source.data(), destination.data(), 4u | kTransfer32Bit));
    for (std::size_t i = 0; i < 4; ++i) assert(destination[i] == source[i]);

    destination.fill(0);
    assert(CpuSet(source.data(), destination.data(), 5u | kTransfer32Bit | kSourceFixed));
    for (std::size_t i = 0; i < 5; ++i) assert(destination[i] == source[0]);

    destination.fill(0);
    assert(CpuFastSet(source.data(), destination.data(), 9u));
    for (std::size_t i = 0; i < 16; ++i) assert(destination[i] == source[i]);

    const std::uint8_t packed[] = {0x10, 3, 0, 0, 0x00, 'A', 'B', 'C'};
    std::array<std::uint8_t, 3> decoded{};
    std::size_t decoded_size = 0;
    assert(Lz77UnCompWram(packed, sizeof(packed), decoded.data(), decoded.size(), &decoded_size));
    assert(decoded_size == 3 && decoded[0] == 'A' && decoded[1] == 'B' && decoded[2] == 'C');

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
