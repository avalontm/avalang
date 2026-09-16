#pragma once

#include <cstddef>
#include <vector>

#include "Export.h"
#include "navigation/Route.h"

namespace avalang {
namespace ui {
namespace navigation {

class AVA_UI_API RouteStack {
public:
    void Push(Route route);
    void Replace(Route route);
    bool Pop();
    bool CanPop() const;
    size_t Depth() const;
    const Route* Top() const;
    void Clear();

private:
    std::vector<Route> entries_;
};

}
}
}