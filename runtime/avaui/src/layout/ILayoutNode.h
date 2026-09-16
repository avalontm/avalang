#pragma once

#include <vector>

#include "Fwd.h"
#include "LayoutTypes.h"

namespace avalang {
namespace ui {

class ILayoutNode {
public:
    virtual ~ILayoutNode() = default;

    virtual ComponentId Id() const = 0;

    virtual const LayoutRect& Rect() const = 0;

    virtual ILayoutNode* Parent() const = 0;

    virtual const std::vector<ILayoutNode*>& Children() const = 0;
};

}
}