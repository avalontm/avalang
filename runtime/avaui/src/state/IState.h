#pragma once

#include <cstddef>
#include <functional>
#include <memory>

#include "Export.h"
#include "Fwd.h"
#include "components/PropertyValue.h"

namespace avalang {
namespace ui {

class AVA_UI_API IState {
public:
    static std::unique_ptr<IState> Create(PropertyValue initial = PropertyValue());

    virtual ~IState() = default;

    virtual const PropertyValue& Value() const = 0;

    virtual void Set(PropertyValue value) = 0;

    using ChangeHandler = std::function<void(const PropertyValue& newValue)>;

    virtual std::size_t Subscribe(ChangeHandler handler) = 0;

    virtual void Unsubscribe(std::size_t subscriptionId) = 0;
};

}
}