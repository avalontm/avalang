#include "component/dotted_path.h"

#include <cctype>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace avahost {

namespace {
std::vector<std::string> SplitOnDots(const std::string& dotted) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : dotted) {
        if (c == '.') {
            parts.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    parts.push_back(current);
    return parts;
}
} // namespace

fs::path ResolveDottedAvauiPath(const std::string& projectRoot, const std::string& dotted) {
    if (dotted.empty()) return {};
    fs::path path = fs::path(projectRoot);
    for (const auto& segment : SplitOnDots(dotted)) {
        path /= segment;
    }
    path += ".avaui";
    return path;
}

std::string CallableTagFromDotted(const std::string& dotted) {
    if (dotted.empty()) return "";
    auto parts = SplitOnDots(dotted);
    std::string tag = parts.back();
    if (!tag.empty()) tag[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(tag[0])));
    return tag;
}

} // namespace avahost
