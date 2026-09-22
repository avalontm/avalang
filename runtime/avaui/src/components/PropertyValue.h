#pragma once

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
    Expression,
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

    static PropertyValue MakeExpression(std::string source, bool isInterpolation);

    PropertyType Type() const;

    bool AsBool() const;
    double AsNumber() const;
    const std::string& AsString() const;
    const PropertyList& AsList() const;

    const std::string& AsExpressionSource() const;
    bool IsInterpolation() const;

private:
    PropertyType type_;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    std::shared_ptr<PropertyList> list_;
    bool isInterpolation_ = false;
};

}
}