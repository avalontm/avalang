#ifndef AVA_UI_COMPONENTS_PROPERTYVALUE_H
#define AVA_UI_COMPONENTS_PROPERTYVALUE_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Export.h"

namespace avalang {
namespace ui {

enum class PropertyType {
    Nil,
    Bool,
    Number,
    String,
    List,
};

class PropertyValue;

using PropertyRecord = std::unordered_map<std::string, PropertyValue>;
using PropertyList = std::vector<PropertyRecord>;

class AVA_UI_API PropertyValue {
public:
    PropertyValue();
    explicit PropertyValue(bool value);
    explicit PropertyValue(double value);
    explicit PropertyValue(std::string value);
    explicit PropertyValue(const char* value);
    explicit PropertyValue(PropertyList value);

    PropertyType Type() const;

    bool AsBool() const;
    double AsNumber() const;
    const std::string& AsString() const;
    const PropertyList& AsList() const;

private:
    PropertyType type_;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    std::shared_ptr<PropertyList> list_;
};

} // namespace ui
} // namespace avalang

#endif // AVA_UI_COMPONENTS_PROPERTYVALUE_H