#ifndef AVA_UI_COMPONENT_H
#define AVA_UI_COMPONENT_H

#include <memory>
#include <string>
#include <vector>

#include "vm/value.h"

namespace ava {
namespace ui {

// Layout kinds for a Component. Values chosen to match the containers
// listed in Ava Studio's toolbox (Column/Row/Stack/Grid/Flex) — see
// PROGRESS.md, "AvaLang.UI" section.
enum class LayoutType {
    None = 0,
    Column,
    Row,
    Stack,
    Grid,
    Flex,
};

// A single node in the AvaLang UI Component Tree.
//
// This is the runtime representation that the C API (ava_ui_*) exposes to
// host bindings (e.g. AvaLang.UI in C#) so a renderer can walk the tree
// produced by running/loading an .ava UI script. See
// public/src/c_api.cpp for the full set of operations this type must
// support (property/event maps, children, id, layout).
class Component : public std::enable_shared_from_this<Component> {
public:
    explicit Component(std::string type);

    const std::string& GetType() const { return type_; }

    void SetId(const std::string& id) { id_ = id; }
    const std::string& GetId() const { return id_; }

    void SetLayout(int layout) { layout_ = layout; }
    int GetLayout() const { return layout_; }

    // Properties (insertion-ordered, like DictObj — see vm/value.h).
    void SetProperty(const std::string& key, const Value& value);
    bool HasProperty(const std::string& key) const;
    Value GetProperty(const std::string& key) const;
    void RemoveProperty(const std::string& key);
    const std::vector<std::pair<std::string, Value>>& GetAllProperties() const { return properties_; }

    // Events (callback values keyed by event name, e.g. "click").
    void SetEvent(const std::string& event, const Value& callback);
    bool HasEvent(const std::string& event) const;
    Value GetEvent(const std::string& event) const;

    // Children.
    void AddChild(std::shared_ptr<Component> child);
    void RemoveChild(Component* child);
    const std::vector<std::shared_ptr<Component>>& GetChildren() const { return children_; }

private:
    std::string type_;
    std::string id_;
    int layout_ = static_cast<int>(LayoutType::None);
    std::vector<std::pair<std::string, Value>> properties_;
    std::vector<std::pair<std::string, Value>> events_;
    std::vector<std::shared_ptr<Component>> children_;
};

} // namespace ui
} // namespace ava

#endif // AVA_UI_COMPONENT_H
