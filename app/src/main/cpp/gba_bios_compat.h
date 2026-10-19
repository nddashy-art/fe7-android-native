#pragma once

#include <cstddef>
#include <cstdint>

namespace fe7::bios {

// Control flags defined by the GBA BIOS CpuSet/CpuFastSet services.
constexpr std::uint32_t kSourceFixed = 1u << 24;
constexpr std::uint32_t kTransfer32Bit = 1u << 26;
constexpr std::uint32_t kTransferCountMask = 0x001FFFFFu;

// Portable host-buffer implementations of BIOS memory services. Arguments
// follow the original FE7 API order: source, destination, control.
// Return false on invalid buffers or a size overflow rather than crashing.
bool CpuSet(const void* source, void* destination, std::uint32_t control);
bool CpuFastSet(const void* source, void* destination, std::uint32_t control);

// Bounded implementation of BIOS 0x10 LZ77 decompression (8-bit output).
bool Lz77UnCompWram(const std::uint8_t* input, std::size_t input_size,
                    std::uint8_t* output, std::size_t output_capacity,
                    std::size_t* output_size);

}  // namespace fe7::bios
