#include "watch/file_watcher.h"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace avahost {

FileWatcher::FileWatcher(std::string rootDir, std::vector<std::string> extensions)
    : rootDir_(std::move(rootDir)), extensions_(std::move(extensions)) {}

void FileWatcher::PollOnce(const FileChangedCallback& callback) {
    if (!fs::exists(rootDir_)) return;

    std::unordered_map<std::string, fs::file_time_type> current;

    for (const auto& entry : fs::recursive_directory_iterator(rootDir_)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        if (std::find(extensions_.begin(), extensions_.end(), ext) == extensions_.end()) continue;

        std::error_code ec;
        auto writeTime = fs::last_write_time(entry.path(), ec);
        if (ec) continue;

        std::string path = entry.path().string();
        current[path] = writeTime;

        auto it = lastSeen_.find(path);
        bool changed = (it == lastSeen_.end()) || (it->second != writeTime);
        if (primed_ && changed) {
            callback(path);
        }
    }

    lastSeen_ = std::move(current);
    primed_ = true; // first poll only primes the baseline, doesn't fire callbacks
}

} // namespace avahost
