#ifndef AVA_UI_STATE_PROPERTYVALUEEQUALS_H
#define AVA_UI_STATE_PROPERTYVALUEEQUALS_H

#include "components/PropertyValue.h"

namespace avalang {
namespace ui {
namespace state {

// PropertyValue (Phase 2) intentionally exposes no operator== -- Phase
// 2 never needed to compare values, only store/retrieve them. Phase 4
// does need it, to decide whether IState::Set() actually changed
// anything (and therefore whether subscribers should be notified), so
// the comparison lives here instead of widening Phase 2's public API
// for a single internal caller.
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
    }
    return false;
}

} // namespace state
} // namespace ui
} // namespace avalang

#endif // AVA_UI_STATE_PROPERTYVALUEEQUALS_H
