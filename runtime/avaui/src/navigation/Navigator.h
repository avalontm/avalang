#pragma once

#include "Export.h"
#include "navigation/INavigator.h"
#include "navigation/RouteStack.h"

namespace avalang {
namespace ui {
namespace navigation {

class AVA_UI_API Navigator : public INavigator {
public:
    Navigator() = default;

    void SetHandler(NavigationHandler handler) override;

    void Push(Route route) override;
    void Replace(Route route) override;
    bool Back() override;
    bool CanBack() const override;

    const Route* Current() const override;
    size_t Depth() const override;
    void Clear() override;

private:
    void Notify(NavigationType type);

    RouteStack stack_;
    NavigationHandler handler_;
};

}
}
}