#pragma once
// AvaHost.FileWatcher -- watches .ava/.avaui for Hot Reload
// (plan section 15). v0.1 uses simple mtime polling, which is
// trivially cross-platform (std::filesystem); an OS-native watcher
// (ReadDirectoryChangesW / inotify) can replace this transparently
// later since callers only see the FileWatcher interface.
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace avahost {

using FileChangedCallback = std::function<void(const std::string& path)>;

class FileWatcher {
public:
    // Watches every file with one of `extensions` (e.g. {".ava",
    // ".avaui"}) under `rootDir`, recursively.
    FileWatcher(std::string rootDir, std::vector<std::string> extensions);

    // Takes one polling snapshot; invokes `callback` once per changed
    // (new, modified, or removed-then-flagged) file. Call this in a
    // loop with a sleep between calls (e.g. from `avahost watch`).
    void PollOnce(const FileChangedCallback& callback);

private:
    std::string rootDir_;
    std::vector<std::string> extensions_;
    // Kept as the filesystem's own clock type (not converted to
    // system_clock) -- last_write_time() timestamps are only ever
    // compared against each other, never displayed, so there's no
    // reason to convert them and every reason not to: a from-scratch
    // "now() offset" conversion recomputed on every poll produces a
    // slightly different value each time for the SAME unchanged file,
    // which used to make every watched file look "changed" on every
    // single poll (see git history for the bug this replaced).
    std::unordered_map<std::string, std::filesystem::file_time_type> lastSeen_;
    bool primed_ = false;
};

} // namespace avahost
