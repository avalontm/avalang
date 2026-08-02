#include "common/ColorParse.h"

#include <cctype>
#include <cstdlib>

namespace avalang {
namespace ui {
namespace common {

namespace {

bool IsHexDigit(char c) {
    return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

std::uint8_t HexPairToByte(char hi, char lo) {
    const char buf[3] = {hi, lo, '\0'};
    return static_cast<std::uint8_t>(std::strtoul(buf, nullptr, 16));
}

std::uint8_t HexNibbleToByte(char c) {
    // "#RGB" shorthand: each nibble is duplicated, same convention as CSS
    // (e.g. "#0F0" -> "#00FF00").
    return HexPairToByte(c, c);
}

} // namespace

Color ParseColor(const std::string& hex) {
    const Color fallback{0, 0, 0, 255};

    if (hex.empty()) {
        return fallback;
    }

    const std::string digits = (hex[0] == '#') ? hex.substr(1) : hex;
    if (digits.empty()) {
        return fallback;
    }

    for (char c : digits) {
        if (!IsHexDigit(c)) {
            return fallback;
        }
    }

    switch (digits.size()) {
        case 3: // #RGB
            return Color{
                HexNibbleToByte(digits[0]),
                HexNibbleToByte(digits[1]),
                HexNibbleToByte(digits[2]),
                255,
            };
        case 6: // #RRGGBB
            return Color{
                HexPairToByte(digits[0], digits[1]),
                HexPairToByte(digits[2], digits[3]),
                HexPairToByte(digits[4], digits[5]),
                255,
            };
        case 8: // #RRGGBBAA
            return Color{
                HexPairToByte(digits[0], digits[1]),
                HexPairToByte(digits[2], digits[3]),
                HexPairToByte(digits[4], digits[5]),
                HexPairToByte(digits[6], digits[7]),
            };
        default:
            return fallback;
    }
}

} // namespace common
} // namespace ui
} // namespace avalang
