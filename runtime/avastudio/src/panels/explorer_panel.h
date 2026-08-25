#pragma once

#include <optional>
#include <string>
#include <utility>

namespace studio {

struct ExplorerState {
    std::string root_dir;

    std::string selected_path;
};

struct ExplorerResult {

    std::optional<std::string> file_to_open;

    std::optional<std::string> file_deleted;

    std::optional<std::pair<std::string, std::string>> file_renamed;

    std::optional<std::string> reveal_in_file_manager;
};

ExplorerResult DrawExplorerPanel(ExplorerState& state, bool* p_open = nullptr);

}
