#pragma once

namespace avalang {
namespace ui {

class IComponent;

class ILifecycleObserver {
public:
    virtual ~ILifecycleObserver() = default;

    virtual void OnMount(IComponent* component) = 0;
    virtual void OnUnmount(IComponent* component) = 0;
};

}
}
