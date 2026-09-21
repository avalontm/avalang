#pragma once

#include <functional>
#include <memory>
#include <string>

#include "Export.h"
#include "Fwd.h"
#include "layout/ILayoutNode.h"

namespace avalang {
namespace ui {

class AVA_UI_API LayoutEngine {
public:
    static std::unique_ptr<LayoutEngine> Create();

    virtual ~LayoutEngine() = default;

    virtual ILayoutNode* Compute(IComponent* componentRoot, const LayoutRect& available) = 0;

    virtual ILayoutNode* ComputeFromPhysicalPixels(IComponent* componentRoot, const LayoutRect& physicalAvailable) = 0;

    virtual LayoutSize Measure(IComponent* componentRoot, const LayoutConstraints& constraints) = 0;

    virtual ILayoutNode* Arrange(IComponent* componentRoot, const LayoutRect& finalRect) = 0;

    virtual void SetTextEvaluator(std::function<std::string(const std::string&)> eval) = 0;

    virtual void SetEmptyContainerMinSize(double width, double height) = 0;

    virtual ILayoutNode* FindNode(ComponentId id) const = 0;

    virtual ILayoutNode* Root() const = 0;
};

}
}