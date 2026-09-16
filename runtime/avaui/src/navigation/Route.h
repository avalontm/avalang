#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "Export.h"

namespace avalang {
namespace ui {
namespace navigation {

class AVA_UI_API Route {
public:
    Route() = default;
    explicit Route(std::string path);
    Route(std::string path, std::vector<std::pair<std::string, std::string>> params);

    const std::string& Path() const;
    const std::vector<std::pair<std::string, std::string>>& Params() const;
    const std::string* Param(const std::string& name) const;

    std::string Resolve() const;

    bool operator==(const Route& other) const;
    bool operator!=(const Route& other) const;

private:
    std::string path_;
    std::vector<std::pair<std::string, std::string>> params_;
};

}
}
}