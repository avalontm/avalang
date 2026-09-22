#include "state/StateBinding.h"

#include "state/StateBindingImpl.h"

namespace avalang {
namespace ui {

std::unique_ptr<StateBinding> StateBinding::Create(IState* state, IComponent* component, std::string propertyName) {
    return std::make_unique<state::StateBindingImpl>(state, component, std::move(propertyName));
}

std::unique_ptr<StateBinding> StateBinding::CreateTwoWay(IState* state, IComponent* component,
                                                          std::string propertyName) {
    return std::make_unique<state::TwoWayStateBindingImpl>(state, component, std::move(propertyName));
}

std::unique_ptr<StateBinding> StateBinding::CreateExpression(std::string source, bool isInterpolation,
                                                               IComponent* component, std::string propertyName,
                                                               StateLookup lookup, ExpressionEvaluator evaluate) {
    return std::make_unique<state::ExpressionStateBindingImpl>(
        std::move(source), isInterpolation, component, std::move(propertyName), std::move(lookup),
        std::move(evaluate));
}

}
}