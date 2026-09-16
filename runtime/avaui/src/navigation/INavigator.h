#pragma once

#include <cstddef>
#include <functional>
#include <memory>

#include "Export.h"
#include "navigation/NavigationContext.h"
#include "navigation/Route.h"

namespace avalang {
namespace ui {
namespace navigation {

class AVA_UI_API INavigator {
public:
    using NavigationHandler = std::function<void(const NavigationContext&)>;

    static std::unique_ptr<INavigator> Create();

    virtual ~INavigator() = default;

    virtual void SetHandler(NavigationHandler handler) = 0;

    virtual void Push(Route route) = 0;
    virtual void Replace(Route route) = 0;
    virtual bool Back() = 0;
    virtual bool CanBack() const = 0;

    virtual const Route* Current() const = 0;
    virtual size_t Depth() const = 0;
    virtual void Clear() = 0;
};

}
}
}