#include "navigation/Route.h"

namespace avalang {
namespace ui {
namespace navigation {

Route::Route(std::string path) : path_(std::move(path)) {}

Route::Route(std::string path, std::vector<std::pair<std::string, std::string>> params)
    : path_(std::move(path)), params_(std::move(params)) {}

const std::string& Route::Path() const {
    return path_;
}

const std::vector<std::pair<std::string, std::string>>& Route::Params() const {
    return params_;
}

const std::string* Route::Param(const std::string& name) const {
    for (const auto& entry : params_) {
        if (entry.first == name) return &entry.second;
    }
    return nullptr;
}

std::string Route::Resolve() const {
    std::string result = path_;
    for (const auto& entry : params_) {
        const std::string token = "{" + entry.first + "}";
        size_t pos = 0;
        while ((pos = result.find(token, pos)) != std::string::npos) {
            result.replace(pos, token.size(), entry.second);
            pos += entry.second.size();
        }
    }
    return result;
}

bool Route::operator==(const Route& other) const {
    return path_ == other.path_ && params_ == other.params_;
}

bool Route::operator!=(const Route& other) const {
    return !(*this == other);
}

}
}
}