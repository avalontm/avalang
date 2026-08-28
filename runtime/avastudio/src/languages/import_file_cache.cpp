#include "languages/import_file_cache.h"

#include <fstream>
#include <sstream>

namespace studio {

const std::string* ImportFileCache::Load(const std::string& path) {
    auto it = cache_.find(path);
    if (it != cache_.end()) return &it->second;
    if (missing_.count(path)) return nullptr;

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        missing_.insert(path);
        return nullptr;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return &cache_.emplace(path, ss.str()).first->second;
}

}
