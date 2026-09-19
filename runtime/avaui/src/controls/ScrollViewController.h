#pragma once

#include "Export.h"
#include "Fwd.h"
#include "events/IEventDispatcher.h"

#include <memory>
#include <vector>

namespace avalang {
namespace ui {

class LayoutEngine;

namespace controls {

class AVA_UI_API ScrollViewController {
public:
    ScrollViewController(events::IEventDispatcher& dispatcher, LayoutEngine& layoutEngine);
    ~ScrollViewController();

    ScrollViewController(const ScrollViewController&) = delete;
    ScrollViewController& operator=(const ScrollViewController&) = delete;

    void Attach(IComponent* root);

private:
    class Handler;

    void AttachRecursive(IComponent* node, IComponent* currentScrollView);

    events::IEventDispatcher& dispatcher_;
    LayoutEngine& layoutEngine_;
    std::vector<std::unique_ptr<Handler>> handlers_;
};

}
}
}
