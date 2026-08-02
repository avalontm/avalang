#ifndef AVA_UI_RENDER_RENDER_TREE_H
#define AVA_UI_RENDER_RENDER_TREE_H

#include "render_tree/IRenderTree.h"
#include "render_tree/RenderNode.h"
#include <unordered_map>
#include <functional>
#include <string>

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

    // A boolean property bound to a bare state identifier (e.g.
    // `isOpen = showDialog`) parses as PropertyType::String holding the
    // literal text "showDialog" -- AvauiParser.cpp's InferValue()
    // doesn't evaluate expressions/state bindings itself, it just keeps
    // unrecognized text opaque (see its own comment). Every other
    // String property already resolves that through Eval() before use
    // (SetText, SetHref, SetBackgroundColor, ...); boolean properties
    // need the same step, just followed by a text->bool read instead of
    // a plain string assignment. Without this, isOpen/isChecked/
    // isSelected/disabled/overlay/backdrop only ever worked with a
    // literal `true`/`false` in the .avaui source and silently stayed
    // at `defaultValue` forever when bound to state -- exactly what
    // made a Dialog's `isOpen = showDialog` never open regardless of
    // how many times the state flipped. Defined in RenderTree.cpp, not
    // inline here: IComponent is only forward-declared in this header,
    // so calling GetProperty() on it needs the full definition that
    // .cpp already includes.
    bool EvalBool(IComponent* comp, const char* propName, bool defaultValue) const;

    // Same idea as EvalBool, for numeric properties (fontSize,
    // borderWidth, borderRadius, zIndex, ...): a bare state identifier
    // parses as opaque String text too, so these need the identical
    // Eval-then-convert step instead of a strict PropertyType::Number
    // check. Only covers properties RenderTree itself reads -- the
    // layout-stage properties (width/height/padding/margin/spacing/gap,
    // read via LayoutEngineImpl.cpp's own ReadNumber/TryReadNumber) run
    // in an earlier pass that has no access to the state evaluator at
    // all yet, so those still can't be state-bound; that's a separate,
    // larger change (threading SetEvalText-equivalent into LayoutEngine
    // itself), not something this helper can reach.
    double EvalNumber(IComponent* comp, const char* propName, double defaultValue) const;

    // Dev-time diagnostic: a TextBox/ComboBox with a `change` handler
    // wired is meant for two-way binding, so its value-carrying
    // property (`text`/`selectedValue`) needs to be a bare identifier
    // that resolves against a declared `state` variable -- see
    // ApplyPendingControlValue's comment in
    // ui_pipeline_dynamic_renderer.cpp. When that's not the case
    // (property missing entirely, or Eval() comes back unchanged --
    // the same "not a declared identifier" signal
    // RuntimeHost::EvalPropertyExpr's AVA_NIL/compile-fail fallback
    // produces), sets parent's BindingWarning() to a message the
    // renderer can surface inline instead of the value silently never
    // persisting. Best-effort heuristic, not a hard guarantee: a bound
    // identifier whose current value happens to equal its own name
    // would read as "still bound" and skip the warning, but that's an
    // acceptable false negative for a dev-facing hint.
    void CheckBindingWarning(IComponent* comp, std::shared_ptr<RenderNode> parent, const char* propName) const;

    // Component type -> decomposition logic
    void DecomposeButton(IComponent* comp, std::shared_ptr<RenderNode> parent, LayoutEngine* layout);
    void DecomposeContainer(IComponent* comp, std::shared_ptr<RenderNode> parent, LayoutEngine* layout);
    void DecomposeText(IComponent* comp, std::shared_ptr<RenderNode> parent, LayoutEngine* layout);
    void DecomposeLink(IComponent* comp, std::shared_ptr<RenderNode> parent, LayoutEngine* layout);
    void DecomposeImage(IComponent* comp, std::shared_ptr<RenderNode> parent, LayoutEngine* layout);
    // Fase 18 -- Controls: rest of base set
    void DecomposeTextBox(IComponent* comp, std::shared_ptr<RenderNode> parent, LayoutEngine* layout);
    void DecomposeCheckBox(IComponent* comp, std::shared_ptr<RenderNode> parent, LayoutEngine* layout);
    void DecomposeRadioButton(IComponent* comp, std::shared_ptr<RenderNode> parent, LayoutEngine* layout);
    void DecomposeComboBox(IComponent* comp, std::shared_ptr<RenderNode> parent, LayoutEngine* layout);
    void DecomposeIcon(IComponent* comp, std::shared_ptr<RenderNode> parent, LayoutEngine* layout);
    void DecomposeDialog(IComponent* comp, std::shared_ptr<RenderNode> parent, LayoutEngine* layout);
    // Fase 24 -- ScrollView
    void DecomposeScrollView(IComponent* comp, std::shared_ptr<RenderNode> parent, LayoutEngine* layout);

    void ForEachRecursive(const std::shared_ptr<IRenderNode>& node,
                          std::function<void(const std::shared_ptr<IRenderNode>&)> visitor);

    std::shared_ptr<IRenderNode> root_;
    std::unordered_map<ComponentId, std::shared_ptr<IRenderNode>> nodeMap_;
    bool dirty_ = true;
    LayoutEngine* lastLayoutEngine_ = nullptr;
    std::function<std::string(const std::string&)> evalText_;
};

} // namespace render
} // namespace ui
} // namespace avalang

#endif // AVA_UI_RENDER_RENDER_TREE_H
