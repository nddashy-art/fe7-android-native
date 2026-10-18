#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>

#include "gba_header_parser.h"

int main() {
    std::array<std::uint8_t, fe7::kGbaHeaderRequiredSize> bytes{};
    std::memcpy(bytes.data() + 0xA0, "FIRE EMBLEM", 11);
    std::memcpy(bytes.data() + 0xAC, "AFE7", 4);

    fe7::GbaHeader header;
    if (!fe7::ParseGbaHeader(bytes.data(), bytes.size(), &header)) {
        std::cerr << "FAIL: valid-size header was rejected\n";
        return 1;
    }
    if (header.title != "FIRE EMBLEM" || header.game_code != "AFE7" ||
        !header.looks_like_fire_emblem) {
        std::cerr << "FAIL: expected FE7-style title/code to be recognized\n";
        return 1;
    }

    std::array<std::uint8_t, fe7::kGbaHeaderRequiredSize - 1> short_bytes{};
    if (fe7::ParseGbaHeader(short_bytes.data(), short_bytes.size(), &header)) {
        std::cerr << "FAIL: truncated header was accepted\n";
        return 1;
    }

    std::memset(bytes.data() + 0xA0, 0, 12);
    std::memcpy(bytes.data() + 0xA0, "NOT A GAME", 10);
    if (!fe7::ParseGbaHeader(bytes.data(), bytes.size(), &header) ||
        header.looks_like_fire_emblem) {
        std::cerr << "FAIL: unknown title handling is incorrect\n";
        return 1;
    }

    std::cout << "PASS: header size, title, game code, and unknown-title cases\n";
    return 0;
}
