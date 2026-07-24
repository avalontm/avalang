#pragma once

#include <string>
#include <utility>
#include <vector>

#include "avalang.h"

namespace studio {

// Result of a compile+run cycle, shown in the Output panel.
struct RunResult {
    bool success = false;
    std::string message; // stdout summary or error text
};

// Thin C++ wrapper around the avalang C API (public/include/avalang.h).
// Owns one AvaVM for the lifetime of the studio session.
//
// This is intentionally NOT a generic "engine service" abstraction --
// it only wraps exactly what the Milestone 1 panels need. Grow it as
// the Designer/Toolbox/live-reload panels come online.
class EngineBridge {
public:
    EngineBridge();
    ~EngineBridge();

    EngineBridge(const EngineBridge&) = delete;
    EngineBridge& operator=(const EngineBridge&) = delete;

    // Compiles and runs `source` (source_name is only used for error
    // messages, e.g. the file path). Does not persist any state between
    // calls beyond the shared AvaVM globals.
    RunResult RunScript(const std::string& source, const std::string& source_name);

    // A read-only, host-side mirror of one AvaComponent -- built while we
    // construct the demo tree below, so the Preview panel has something
    // interactive to walk without re-implementing JSON parsing.
    struct PreviewNode {
        std::string type;
        std::string id;
        std::vector<std::pair<std::string, std::string>> properties;
        std::vector<PreviewNode> children;
    };

    struct DemoTree {
        PreviewNode root;
        std::string json; // from ava_ui_tree_to_json, for the Output panel
    };

    // Builds a small fixed Component Tree via ava_ui_* (page > stack >
    // text + button). This is a stand-in for "run a script that declares
    // UI" until the `page`/`stack`/`button` builtins
    // (core/src/ui/builtins.cpp) are wired into the VM -- see that
    // file's header comment. Validates the Component/Property/Child/
    // JSON path the Preview panel depends on.
    DemoTree BuildDemoComponentTree();

private:
    AvaVM* vm_ = nullptr;
};

} // namespace studio
