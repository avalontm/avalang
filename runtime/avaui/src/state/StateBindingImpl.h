#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "state/StateBinding.h"
#include "common/NonCopyable.h"

namespace avalang {
namespace ui {
namespace state {

class StateBindingImpl final : public StateBinding, private common::NonCopyable {
public:
    StateBindingImpl(IState* state, IComponent* component, std::string propertyName);
    ~StateBindingImpl() override;

private:
    IState* state_;
    IComponent* component_;
    std::string propertyName_;
    std::size_t subscriptionId_ = 0;
    bool subscribed_ = false;
};

class TwoWayStateBindingImpl final : public StateBinding, private common::NonCopyable {
public:
    TwoWayStateBindingImpl(IState* state, IComponent* component, std::string propertyName);
    ~TwoWayStateBindingImpl() override;

    bool WriteBack(PropertyValue value) override;

private:
    IState* state_;
    IComponent* component_;
    std::string propertyName_;
    std::size_t subscriptionId_ = 0;
    bool subscribed_ = false;
    bool writingBack_ = false;
};

class ExpressionStateBindingImpl final : public StateBinding, private common::NonCopyable {
public:
    ExpressionStateBindingImpl(std::string source, bool isInterpolation, IComponent* component,
                               std::string propertyName, StateBinding::StateLookup lookup,
                               StateBinding::ExpressionEvaluator evaluate);
    ~ExpressionStateBindingImpl() override;

private:
    struct Dependency {
        IState* state;
        std::size_t subscriptionId;
    };

    void Reevaluate();

    std::string source_;
    bool isInterpolation_;
    IComponent* component_;
    std::string propertyName_;
    StateBinding::ExpressionEvaluator evaluate_;
    std::vector<Dependency> dependencies_;
};

}
}
}