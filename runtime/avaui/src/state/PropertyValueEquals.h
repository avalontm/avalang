#pragma once

#include "components/PropertyValue.h"

namespace avalang {
namespace ui {
namespace state {

inline bool PropertyValueEquals(const PropertyValue& a, const PropertyValue& b) {
    if (a.Type() != b.Type()) {
        return false;
    }
    switch (a.Type()) {
        case PropertyType::Nil:
            return true;
        case PropertyType::Bool:
            return a.AsBool() == b.AsBool();
        case PropertyType::Number:
            return a.AsNumber() == b.AsNumber();
        case PropertyType::String:
            return a.AsString() == b.AsString();
        case PropertyType::Expression:
            return a.AsExpressionSource() == b.AsExpressionSource() &&
                   a.IsInterpolation() == b.IsInterpolation();
        case PropertyType::List:
            break;
    }
    return false;
}

}
}
}