#include "WinKeyTranslation.h"
#include <windows.h>

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

using avalang::ui::events::Key;

Key TranslateVirtualKey(int vk) {
    if (vk >= 'A' && vk <= 'Z') {
        return static_cast<Key>(static_cast<int>(Key::A) + (vk - 'A'));
    }
    if (vk >= '0' && vk <= '9') {
        return static_cast<Key>(static_cast<int>(Key::Digit0) + (vk - '0'));
    }
    if (vk >= VK_F1 && vk <= VK_F12) {
        return static_cast<Key>(static_cast<int>(Key::F1) + (vk - VK_F1));
    }
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        return static_cast<Key>(static_cast<int>(Key::Numpad0) + (vk - VK_NUMPAD0));
    }

    switch (vk) {
        case VK_ESCAPE: return Key::Escape;
        case VK_TAB: return Key::Tab;
        case VK_CAPITAL: return Key::CapsLock;
        case VK_SPACE: return Key::Space;
        case VK_RETURN: return Key::Enter;
        case VK_BACK: return Key::Backspace;
        case VK_DELETE: return Key::Delete;
        case VK_INSERT: return Key::Insert;
        case VK_LEFT: return Key::ArrowLeft;
        case VK_RIGHT: return Key::ArrowRight;
        case VK_UP: return Key::ArrowUp;
        case VK_DOWN: return Key::ArrowDown;
        case VK_HOME: return Key::Home;
        case VK_END: return Key::End;
        case VK_PRIOR: return Key::PageUp;
        case VK_NEXT: return Key::PageDown;
        case VK_LSHIFT: return Key::ShiftLeft;
        case VK_RSHIFT: return Key::ShiftRight;
        case VK_SHIFT: return Key::ShiftLeft;
        case VK_LCONTROL: return Key::ControlLeft;
        case VK_RCONTROL: return Key::ControlRight;
        case VK_CONTROL: return Key::ControlLeft;
        case VK_LMENU: return Key::AltLeft;
        case VK_RMENU: return Key::AltRight;
        case VK_MENU: return Key::AltLeft;
        case VK_LWIN: return Key::MetaLeft;
        case VK_RWIN: return Key::MetaRight;
        case VK_OEM_MINUS: return Key::Minus;
        case VK_OEM_PLUS: return Key::Equal;
        case VK_OEM_COMMA: return Key::Comma;
        case VK_OEM_PERIOD: return Key::Period;
        case VK_OEM_2: return Key::Slash;
        case VK_OEM_1: return Key::Semicolon;
        case VK_OEM_7: return Key::Quote;
        case VK_OEM_4: return Key::BracketLeft;
        case VK_OEM_6: return Key::BracketRight;
        case VK_OEM_5: return Key::Backslash;
        case VK_OEM_3: return Key::Backtick;
        case VK_ADD: return Key::NumpadAdd;
        case VK_SUBTRACT: return Key::NumpadSubtract;
        case VK_MULTIPLY: return Key::NumpadMultiply;
        case VK_DIVIDE: return Key::NumpadDivide;
        case VK_DECIMAL: return Key::NumpadDecimal;
        case VK_SNAPSHOT: return Key::PrintScreen;
        case VK_SCROLL: return Key::ScrollLock;
        case VK_PAUSE: return Key::Pause;
        case VK_APPS: return Key::ContextMenu;
        default: return Key::Unknown;
    }
}

}
}
}
}
