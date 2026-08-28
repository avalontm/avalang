#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace studio {

class ImportFileCache {
public:
    const std::string* Load(const std::string& path);

private:
    std::unordered_map<std::string, std::string> cache_;
    std::unordered_set<std::string> missing_;
};

}
