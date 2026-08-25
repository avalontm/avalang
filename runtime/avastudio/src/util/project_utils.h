#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace studio {

// Resolves the entry point of a project: "main.ava" at the root if present,
// otherwise the first ".ava" file found alphabetically (breadth over depth,
// up to 3 levels deep). Returns a path relative to project_dir, or an empty
// string if no ".ava" file was found. Shared by the Build panel and
// "Run Project" so both agree on the same entry point.
std::string DetectEntryFile(const std::filesystem::path& project_dir);

// Recursively lists every ".ava"/".avaui" file under project_dir, walking
// depth-first with entries sorted per directory (same "sort before
// iterating" reasoning DetectEntryFile above already relies on) so callers
// get a stable, predictable order without re-sorting themselves. Returns an
// empty list if project_dir doesn't exist. Originally private to Find in
// Project (CollectSearchableFiles in find_in_project_panel.cpp); pulled out
// here, Fase 4, so Quick Open can use the exact same file walk instead of
// duplicating it -- same reasoning DetectEntryFile was already extracted
// for Run Project/Check.
std::vector<std::filesystem::path> ListSearchableFiles(const std::filesystem::path& project_dir);

}
