#include "state/StateBindingImpl.h"

#include <cctype>
#include <unordered_set>
#include <utility>

#include "components/IComponent.h"
#include "state/IState.h"

namespace avalang {
namespace ui {
namespace state {

StateBindingImpl::StateBindingImpl(IState* state, IComponent* component, std::string propertyName)
    : state_(state), component_(component), propertyName_(std::move(propertyName)) {
    if (!state_ || !component_) {
        return;
    }

    component_->SetProperty(propertyName_, state_->Value());

    subscriptionId_ = state_->Subscribe([this](const PropertyValue& newValue) {
        component_->SetProperty(propertyName_, newValue);
    });
    subscribed_ = true;
}

StateBindingImpl::~StateBindingImpl() {
    if (subscribed_) {
        state_->Unsubscribe(subscriptionId_);
    }
}

TwoWayStateBindingImpl::TwoWayStateBindingImpl(IState* state, IComponent* component, std::string propertyName)
    : state_(state), component_(component), propertyName_(std::move(propertyName)) {
    if (!state_ || !component_) {
        return;
    }

    component_->SetProperty(propertyName_, state_->Value());

    subscriptionId_ = state_->Subscribe([this](const PropertyValue& newValue) {
        if (writingBack_) return;
        component_->SetProperty(propertyName_, newValue);
    });
    subscribed_ = true;
}

TwoWayStateBindingImpl::~TwoWayStateBindingImpl() {
    if (subscribed_) {
        state_->Unsubscribe(subscriptionId_);
    }
}

bool TwoWayStateBindingImpl::WriteBack(PropertyValue value) {
    if (!state_) return false;
    writingBack_ = true;
    state_->Set(std::move(value));
    writingBack_ = false;
    return true;
}

namespace {

std::unordered_set<std::string> ExtractIdentifiers(const std::string& source) {
    static const std::unordered_set<std::string> kKeywords = {
        "true", "false", "null", "and", "or", "not",
    };
    std::unordered_set<std::string> names;
    size_t i = 0;
    while (i < source.size()) {
        char c = source[i];
        if (!(std::isalpha(static_cast<unsigned char>(c)) || c == '_')) {
            ++i;
            continue;
        }
        size_t start = i;
        while (i < source.size() &&
               (std::isalnum(static_cast<unsigned char>(source[i])) || source[i] == '_')) {
            ++i;
        }
        std::string word = source.substr(start, i - start);
        if (!kKeywords.count(word)) {
            names.insert(word);
        }
    }
    return names;
}

}

ExpressionStateBindingImpl::ExpressionStateBindingImpl(std::string source, bool isInterpolation,
                                                        IComponent* component, std::string propertyName,
                                                        StateBinding::StateLookup lookup,
                                                        StateBinding::ExpressionEvaluator evaluate)
    : source_(std::move(source)), isInterpolation_(isInterpolation), component_(component),
      propertyName_(std::move(propertyName)), evaluate_(std::move(evaluate)) {
    if (!component_ || !evaluate_) {
        return;
    }

    if (lookup) {
        for (const std::string& name : ExtractIdentifiers(source_)) {
            IState* state = lookup(name);
            if (!state) continue;
            std::size_t id = state->Subscribe([this](const PropertyValue&) { Reevaluate(); });
            dependencies_.push_back({state, id});
        }
    }

    Reevaluate();
}

ExpressionStateBindingImpl::~ExpressionStateBindingImpl() {
    for (const Dependency& dep : dependencies_) {
        dep.state->Unsubscribe(dep.subscriptionId);
    }
}

void ExpressionStateBindingImpl::Reevaluate() {
    component_->SetProperty(propertyName_, evaluate_(source_, isInterpolation_));
}

}
}
}