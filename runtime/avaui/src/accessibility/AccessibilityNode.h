#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Export.h"
#include "Fwd.h"

namespace avalang {
namespace ui {
namespace accessibility {

enum class AccessibilityRole : unsigned char {
    Unknown,
    Button,
    Text,
    Image,
    Container,
    TextInput,
    CheckBox,
    RadioButton,
    ComboBox,
    Dialog,
    Link,
    ScrollView,
    Page,
};

enum class AccessibilityState : uint32_t {
    None = 0,
    Focusable = 1u << 0,
    Focused = 1u << 1,
    Visible = 1u << 2,
    Hidden = 1u << 3,
    Disabled = 1u << 4,
    Checked = 1u << 5,
    Selected = 1u << 6,
    Expanded = 1u << 7,
};

enum class AccessibilityAction : unsigned char {
    Activate,
    Focus,
    SetValue,
    Scroll,
    Dismiss,
};

struct AccessibilityRect {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

class AVA_UI_API AccessibilityNode {
public:
    explicit AccessibilityNode(ComponentId id, AccessibilityRole role);

    ComponentId Id() const;
    AccessibilityRole Role() const;
    void SetRole(AccessibilityRole role);

    const std::string& Label() const;
    void SetLabel(const std::string& label);

    const std::string& Value() const;
    void SetValue(const std::string& value);

    const std::string& Hint() const;
    void SetHint(const std::string& hint);

    const AccessibilityRect& Bounds() const;
    void SetBounds(const AccessibilityRect& bounds);

    uint32_t States() const;
    void AddState(AccessibilityState state);
    void RemoveState(AccessibilityState state);
    bool HasState(AccessibilityState state) const;

    const std::vector<AccessibilityAction>& Actions() const;
    void AddAction(AccessibilityAction action);

    const std::vector<AccessibilityNode*>& Children() const;
    void AddChild(AccessibilityNode* child);

    AccessibilityNode* Parent() const;
    void SetParent(AccessibilityNode* parent);

private:
    ComponentId id_;
    AccessibilityRole role_;
    std::string label_;
    std::string value_;
    std::string hint_;
    AccessibilityRect bounds_;
    uint32_t states_ = static_cast<uint32_t>(AccessibilityState::None);
    std::vector<AccessibilityAction> actions_;
    std::vector<AccessibilityNode*> children_;
    AccessibilityNode* parent_ = nullptr;
};

}
}
}