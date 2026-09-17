#pragma once

#include <string>
#include <vector>

#include "Export.h"
#include "Fwd.h"
#include "parser/AvauiParser.h"

namespace avalang {
namespace ui {
namespace parser {

struct AvauiStateEntry {
    std::string key;
    std::string value;
};

struct AvauiRouteEntry {
    std::string route_template;
};

struct AvauiWriteOptions {
    std::string code_behind;
    std::vector<AvauiStateEntry> initial_state;
    std::vector<std::string> imports;
    std::string extends;
    std::vector<AvauiRouteEntry> routes;
    std::vector<AnimationSpec> animations;
};

AVA_UI_API std::string WriteAvaui(const IComponent* root, const AvauiWriteOptions& options);

}
}
}