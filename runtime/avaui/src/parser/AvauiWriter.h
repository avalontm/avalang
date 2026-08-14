#ifndef AVA_UI_PARSER_AVAUIWRITER_H
#define AVA_UI_PARSER_AVAUIWRITER_H

#include <string>
#include <vector>

#include "Export.h"
#include "Fwd.h"

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
};

AVA_UI_API std::string WriteAvaui(const IComponent* root, const AvauiWriteOptions& options);

}
}
}

#endif