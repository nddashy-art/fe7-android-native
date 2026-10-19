#pragma once

#include <cstddef>
#include <cstdint>

namespace fe7::bios {

// GBA BIOS CpuSet control flags.
constexpr std::uint32_t kCpuSetFill = 1u << 24;
constexpr std::uint32_t kCpuSetWord = 1u << 26;
constexpr std::uint32_t kCpuSetCountMask = 0x001FFFFFu;

// Portable replacements for the memory-copy/fill BIOS services used by FE7.
// These functions operate on host buffers only; they do not emulate GBA hardware.
bool CpuSet(void* destination, const void* source, std::uint32_t control);
bool CpuFastSet(void* destination, const void* source, std::uint32_t control);

// Decode a GBA BIOS 0x10 LZ77-compressed stream into caller-owned memory.
// Returns false for malformed/truncated input or insufficient output capacity.
bool Lz77UnCompWram(const std::uint8_t* input, std::size_t input_size,
                    std::uint8_t* output, std::size_t output_capacity,
                    std::size_t* output_size);

}  // namespace fe7::bios
