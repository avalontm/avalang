#ifndef AVA_UI_COMPONENTS_PROPERTYVALUE_H
#define AVA_UI_COMPONENTS_PROPERTYVALUE_H

#include <string>

#include "Export.h"

namespace avalang {
namespace ui {

// Generic property-bag value. Phase 2 only stores and retrieves these --
// interpreting them (as a layout width, a bound state expression, or
// literal text) is the responsibility of later phases (Layout Engine,
// State System, Render Tree). Deliberately not a template/std::variant
// to keep the public ABI simple; four cases cover every property kind
// a component needs to declare today.
enum class PropertyType {
    Nil,
    Bool,
    Number,
    String,
};

class AVA_UI_API PropertyValue {
public:
    PropertyValue();
    explicit PropertyValue(bool value);
    explicit PropertyValue(double value);
    explicit PropertyValue(std::string value);
    // Fase 18 fix: without this overload, PropertyValue("literal") binds
    // to PropertyValue(bool) instead of PropertyValue(std::string) --
    // const char* -> bool is a standard conversion, const char* ->
    // std::string is a user-defined conversion, and overload resolution
    // always prefers the former even though every PropertyValue ctor is
    // explicit. Found while writing controls/ for Fase 18 (RenderTheme's
    // Button textColor default silently stored as a bool, so AsString()
    // came back empty); see docs/AVAUI_FASE18_CONTROLS.md.
    explicit PropertyValue(const char* value);

    PropertyType Type() const;

    bool AsBool() const;
    double AsNumber() const;
    const std::string& AsString() const;

private:
    PropertyType type_;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
};

} // namespace ui
} // namespace avalang

#endif // AVA_UI_COMPONENTS_PROPERTYVALUE_H
