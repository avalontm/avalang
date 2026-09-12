#include "languages/workspace_index.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "util/project_utils.h"

namespace studio {

namespace {

namespace fs = std::filesystem;

std::string ReadFileText(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

}

WorkspaceIndex::~WorkspaceIndex() {
    JoinWorker();
}

void WorkspaceIndex::JoinWorker() {
    if (worker_.joinable()) worker_.join();
}

void WorkspaceIndex::SetRoot(const std::string& root_dir, const std::string& stdlib_dir) {
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        if (root_dir_ == root_dir && stdlib_dir_ == stdlib_dir) return;
        root_dir_ = root_dir;
        stdlib_dir_ = stdlib_dir;
    }
    MarkDirty();
}

void WorkspaceIndex::MarkDirty() {
    rebuild_pending_ = true;
}

void WorkspaceIndex::Poll() {
    if (!rebuild_pending_ || rebuild_running_) return;
    JoinWorker();
    StartRebuild();
}

void WorkspaceIndex::StartRebuild() {
    rebuild_pending_ = false;
    rebuild_running_ = true;

    std::string root_dir;
    std::string stdlib_dir;
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        root_dir = root_dir_;
        stdlib_dir = stdlib_dir_;
    }

    worker_ = std::thread([this, root_dir, stdlib_dir] {
        auto snapshot = std::make_shared<Snapshot>();

        if (!root_dir.empty()) {
            for (const fs::path& path : ListSearchableFiles(root_dir)) {
                if (path.extension() != ".ava") continue;

                const std::string text = ReadFileText(path);
                if (text.empty()) continue;

                std::error_code ec;
                const std::string abs_path = fs::absolute(path, ec).string();
                const std::string source_file = ec ? path.string() : abs_path;

                snapshot->classes.ScanFile(text, source_file);
                snapshot->functions.ScanFile(text, source_file);
            }
        }

        {
            std::lock_guard<std::mutex> lock(snapshot_mutex_);
            snapshot_ = snapshot;
        }
        rebuild_running_ = false;
    });
}

std::shared_ptr<const WorkspaceIndex::Snapshot> WorkspaceIndex::CurrentSnapshot() const {
    std::lock_guard<std::mutex> lock(snapshot_mutex_);
    return snapshot_;
}

}
