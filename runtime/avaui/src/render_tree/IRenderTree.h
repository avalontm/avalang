#pragma once

#include "render_tree/IRenderNode.h"
#include "Export.h"

#include <functional>
#include <memory>
#include <string>

namespace avalang {
namespace ui {

class IComponent;
class LayoutEngine;

namespace render {

class AVA_UI_API IRenderTree {
public:
    virtual ~IRenderTree() = default;

    static IRenderTree* Create();

    virtual void Build(IComponent* componentRoot, LayoutEngine* layoutEngine) = 0;

    virtual std::shared_ptr<IRenderNode> Root() const = 0;
    virtual std::shared_ptr<IRenderNode> FindNode(ComponentId componentId) const = 0;

    virtual void Invalidate() = 0;
    virtual bool IsDirty() const = 0;

    virtual void ForEach(std::function<void(const std::shared_ptr<IRenderNode>&)> visitor) = 0;

    virtual void SetEvalText(std::function<std::string(const std::string&)> evalText) = 0;
};

}
}
}