#pragma once

#include <cstddef>
#include "navigation/Route.h"

namespace avalang {
namespace ui {
namespace navigation {

enum class NavigationType : unsigned char {
    Push,
    Pop,
    Replace,
};

struct NavigationContext {
    Route route;
    NavigationType type = NavigationType::Push;
    size_t depth = 0;
};

}
}
}