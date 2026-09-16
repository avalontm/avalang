#pragma once

#include <vector>

#include "layout/ILayoutNode.h"
#include "common/NonCopyable.h"

namespace avalang {
namespace ui {
namespace layout {

class LayoutNode final : public ILayoutNode, private common::NonCopyable {
public:
    explicit LayoutNode(ComponentId id);

    ComponentId Id() const override;
    const LayoutRect& Rect() const override;
    ILayoutNode* Parent() const override;
    const std::vector<ILayoutNode*>& Children() const override;

    void SetRect(const LayoutRect& rect);
    void SetParent(LayoutNode* parent);
    void AddChild(LayoutNode* child);

private:
    ComponentId id_;
    LayoutRect rect_;
    LayoutNode* parent_ = nullptr;
    std::vector<ILayoutNode*> children_;
};

}
}
}