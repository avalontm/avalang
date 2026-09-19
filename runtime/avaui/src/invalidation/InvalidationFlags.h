#pragma once

#include <cstdint>

namespace avalang {
namespace ui {

enum class InvalidationFlag : unsigned char {
    None = 0,
    Layout = 1u << 0,
    Paint = 1u << 1,
    Scene = 1u << 2,
};

inline InvalidationFlag operator|(InvalidationFlag a, InvalidationFlag b) {
    return static_cast<InvalidationFlag>(static_cast<unsigned char>(a) | static_cast<unsigned char>(b));
}

inline InvalidationFlag operator&(InvalidationFlag a, InvalidationFlag b) {
    return static_cast<InvalidationFlag>(static_cast<unsigned char>(a) & static_cast<unsigned char>(b));
}

inline InvalidationFlag& operator|=(InvalidationFlag& a, InvalidationFlag b) {
    a = a | b;
    return a;
}

inline bool HasInvalidationFlag(InvalidationFlag value, InvalidationFlag flag) {
    return (static_cast<unsigned char>(value) & static_cast<unsigned char>(flag)) != 0;
}

constexpr InvalidationFlag kInvalidationAll =
    static_cast<InvalidationFlag>(static_cast<unsigned char>(InvalidationFlag::Layout) |
                                   static_cast<unsigned char>(InvalidationFlag::Paint) |
                                   static_cast<unsigned char>(InvalidationFlag::Scene));

}
}
