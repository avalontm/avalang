#pragma once

#include "render_tree/IRenderTree.h"
#include "render_tree/RenderNode.h"

#include <functional>
#include <string>
#include <unordered_map>

namespace avalang {
namespace ui {

class IComponent;

namespace render {

class RenderTree : public IRenderTree {
public:
    RenderTree();
    ~RenderTree();

    void Build(IComponent* componentRoot, LayoutEngine* layoutEngine) override;

    std::shared_ptr<IRenderNode> Root() const override { return root_; }
    std::shared_ptr<IRenderNode> FindNode(ComponentId componentId) const override;

    void Invalidate() override { dirty_ = true; }
    bool IsDirty() const override { return dirty_; }

    void ForEach(std::function<void(const std::shared_ptr<IRenderNode>&)> visitor) override;

    void SetEvalText(std::function<std::string(const std::string&)> evalText) override {
        evalText_ = std::move(evalText);
    }

private:
    std::shared_ptr<IRenderNode> BuildComponent(IComponent* component, LayoutEngine* layoutEngine);

    std::string Eval(const std::string& raw) const {
        return evalText_ ? evalText_(raw) : raw;
    }

    bool EvalBool(IComponent* comp, const char* propName, bool defaultValue) const;
    double EvalNumber(IComponent* comp, const char* propName, double defaultValue) const;
    void CheckBindingWarning(IComponent* comp, std::shared_ptr<RenderNode> parent, const char* propName) const;

    void DecomposeButton(IComponent* comp, std::shared_ptr<RenderNode> parent, LayoutEngine* layout);
    void DecomposeContainer(IComponent* comp, std::shared_ptr<RenderNode> parent, LayoutEngine* layout);
    void DecomposeText(IComponent* comp, std::shared_ptr<RenderNode> parent, LayoutEngine* layout);
    void DecomposeLink(IComponent* comp, std::shared_ptr<RenderNode> parent, LayoutEngine* layout);
    void DecomposeImage(IComponent* comp, std::shared_ptr<RenderNode> parent, LayoutEngine* layout);
    void DecomposeTextBox(IComponent* comp, std::shared_ptr<RenderNode> parent, LayoutEngine* layout);
    void DecomposeCheckBox(IComponent* comp, std::shared_ptr<RenderNode> parent, LayoutEngine* layout);
    void DecomposeRadioButton(IComponent* comp, std::shared_ptr<RenderNode> parent, LayoutEngine* layout);
    void DecomposeComboBox(IComponent* comp, std::shared_ptr<RenderNode> parent, LayoutEngine* layout);
    void DecomposeIcon(IComponent* comp, std::shared_ptr<RenderNode> parent, LayoutEngine* layout);
    void DecomposeDialog(IComponent* comp, std::shared_ptr<RenderNode> parent, LayoutEngine* layout);
    void DecomposeScrollView(IComponent* comp, std::shared_ptr<RenderNode> parent, LayoutEngine* layout);

    void ForEachRecursive(const std::shared_ptr<IRenderNode>& node,
                          std::function<void(const std::shared_ptr<IRenderNode>&)> visitor);

    std::shared_ptr<IRenderNode> root_;
    std::unordered_map<ComponentId, std::shared_ptr<IRenderNode>> nodeMap_;
    bool dirty_ = true;
    std::function<std::string(const std::string&)> evalText_;
};

}
}
}