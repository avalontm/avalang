#include "util/project_utils.h"

#include <algorithm>
#include <functional>
#include <system_error>
#include <vector>

namespace studio {

namespace fs = std::filesystem;

namespace {

bool IsSearchableFile(const fs::path& path) {
    const std::string ext = path.extension().string();
    return ext == ".ava" || ext == ".avaui";
}

}

std::string DetectEntryFile(const fs::path& project_dir) {
    std::error_code ec;
    if (fs::exists(project_dir / "main.ava", ec)) return "main.ava";

    std::vector<std::string> found;
    constexpr int kMaxDepth = 3;
    std::function<void(const fs::path&, int)> walk = [&](const fs::path& dir, int depth) {
        if (depth > kMaxDepth) return;
        std::error_code walk_ec;
        for (const auto& entry : fs::directory_iterator(dir, walk_ec)) {
            if (entry.is_directory()) {
                walk(entry.path(), depth + 1);
            } else if (entry.path().extension() == ".ava") {
                found.push_back(fs::relative(entry.path(), project_dir, ec).generic_string());
            }
        }
    };
    walk(project_dir, 0);
    if (found.empty()) return "";
    std::sort(found.begin(), found.end());
    return found.front();
}

std::vector<fs::path> ListSearchableFiles(const fs::path& project_dir) {
    std::vector<fs::path> out;
    std::error_code ec;
    if (!fs::exists(project_dir, ec) || !fs::is_directory(project_dir, ec)) return out;

    std::function<void(const fs::path&)> walk = [&](const fs::path& dir) {
        std::error_code walk_ec;
        std::vector<fs::directory_entry> entries;
        for (const auto& entry : fs::directory_iterator(dir, walk_ec)) {
            entries.push_back(entry);
        }
        std::sort(entries.begin(), entries.end(),
                  [](const fs::directory_entry& a, const fs::directory_entry& b) { return a.path() < b.path(); });

        for (const auto& entry : entries) {
            if (entry.is_directory()) {
                walk(entry.path());
            } else if (IsSearchableFile(entry.path())) {
                out.push_back(entry.path());
            }
        }
    };
    walk(project_dir);
    return out;
}

}
