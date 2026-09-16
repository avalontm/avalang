#pragma once

#include <cstdint>
#include <functional>

namespace avalang {
namespace ui {
namespace events {

enum class Key : uint16_t {
    Unknown = 0,

    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    Digit0, Digit1, Digit2, Digit3, Digit4,
    Digit5, Digit6, Digit7, Digit8, Digit9,

    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    Escape, Tab, CapsLock, Space, Enter, Backspace, Delete, Insert,

    ArrowLeft, ArrowRight, ArrowUp, ArrowDown,
    Home, End, PageUp, PageDown,

    ShiftLeft, ShiftRight,
    ControlLeft, ControlRight,
    AltLeft, AltRight,
    MetaLeft, MetaRight,

    Minus, Equal, Comma, Period, Slash, Semicolon, Quote,
    BracketLeft, BracketRight, Backslash, Backtick,

    NumpadAdd, NumpadSubtract, NumpadMultiply, NumpadDivide,
    NumpadEnter, NumpadDecimal,
    Numpad0, Numpad1, Numpad2, Numpad3, Numpad4,
    Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,

    PrintScreen, ScrollLock, Pause, ContextMenu,
};

enum KeyModifier : uint8_t {
    KeyModifierNone  = 0,
    KeyModifierShift = 1 << 0,
    KeyModifierCtrl  = 1 << 1,
    KeyModifierAlt   = 1 << 2,
    KeyModifierMeta  = 1 << 3,
};

using NativeKeyTranslator = std::function<Key(int)>;

}
}
}
