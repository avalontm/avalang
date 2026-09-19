#pragma once

#include "events/Key.h"

namespace avalang {
namespace ui {
namespace controls {

inline bool IsActivationKey(events::Key key) {
    return key == events::Key::Space || key == events::Key::Enter || key == events::Key::NumpadEnter;
}

}
}
}
