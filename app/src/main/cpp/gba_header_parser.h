#pragma once

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>

namespace fe7 {

constexpr std::size_t kGbaHeaderRequiredSize = 0xB0;

struct GbaHeader {
    std::string title;
    std::string game_code;
    bool looks_like_fire_emblem = false;
};

inline std::string ReadHeaderField(const std::uint8_t* bytes,
                                   std::size_t offset,
                                   std::size_t length) {
    std::string value;
    value.reserve(length);

    for (std::size_t i = 0; i < length; ++i) {
        const std::uint8_t byte = bytes[offset + i];
        if (byte == 0) {
            break;
        }

        value.push_back(byte >= 0x20 && byte <= 0x7E
                            ? static_cast<char>(byte)
                            : '?');
    }

    while (!value.empty() && value.back() == ' ') {
        value.pop_back();
    }

    return value;
}

inline std::string NormalizeTitle(const std::string& title) {
    std::string normalized;
    normalized.reserve(title.size());

    for (unsigned char ch : title) {
        if (std::isalnum(ch) != 0) {
            normalized.push_back(
                static_cast<char>(std::toupper(ch)));
        }
    }

    return normalized;
}

// This is a conservative header/title check, not proof that the file is a
// complete, unmodified, or playable ROM.
inline bool ParseGbaHeader(const std::uint8_t* bytes,
                           std::size_t length,
                           GbaHeader* output) {
    if (bytes == nullptr || output == nullptr ||
        length < kGbaHeaderRequiredSize) {
        return false;
    }

    output->title = ReadHeaderField(bytes, 0xA0, 12);
    output->game_code = ReadHeaderField(bytes, 0xAC, 4);
    output->looks_like_fire_emblem =
        NormalizeTitle(output->title).find("FIREEMBLEM") !=
        std::string::npos;

    return true;
}

}  // namespace fe7
