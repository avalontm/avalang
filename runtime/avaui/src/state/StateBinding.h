#pragma once

#include <functional>
#include <memory>
#include <string>

#include "Export.h"
#include "Fwd.h"
#include "components/IComponent.h"
#include "components/PropertyValue.h"
#include "state/IState.h"

namespace avalang {
namespace ui {

class AVA_UI_API StateBinding {
public:
    static std::unique_ptr<StateBinding> Create(IState* state, IComponent* component, std::string propertyName);

    static std::unique_ptr<StateBinding> CreateTwoWay(IState* state, IComponent* component,
                                                       std::string propertyName);

    using StateLookup = std::function<IState*(const std::string& name)>;
    using ExpressionEvaluator = std::function<PropertyValue(const std::string& source, bool isInterpolation)>;

    static std::unique_ptr<StateBinding> CreateExpression(std::string source, bool isInterpolation,
                                                            IComponent* component, std::string propertyName,
                                                            StateLookup lookup, ExpressionEvaluator evaluate);

    virtual ~StateBinding() = default;

    virtual bool WriteBack(PropertyValue value) { (void)value; return false; }
};

}
}