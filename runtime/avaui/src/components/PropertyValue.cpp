#include "components/PropertyValue.h"

namespace avalang {
namespace ui {

PropertyValue::PropertyValue() : type_(PropertyType::Nil) {}

PropertyValue::PropertyValue(bool value)
    : type_(PropertyType::Bool), bool_(value) {}

PropertyValue::PropertyValue(double value)
    : type_(PropertyType::Number), number_(value) {}

PropertyValue::PropertyValue(std::string value)
    : type_(PropertyType::String), string_(std::move(value)) {}

PropertyValue::PropertyValue(const char* value)
    : type_(PropertyType::String), string_(value ? value : "") {}

PropertyType PropertyValue::Type() const {
    return type_;
}

bool PropertyValue::AsBool() const {
    return bool_;
}

double PropertyValue::AsNumber() const {
    return number_;
}

const std::string& PropertyValue::AsString() const {
    return string_;
}

} // namespace ui
} // namespace avalang
