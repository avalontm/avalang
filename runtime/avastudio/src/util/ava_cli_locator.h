#pragma once

#include <filesystem>

namespace studio {

// Directory containing the currently running executable (ava_studio.exe
// itself), not the cwd. Used by DetectAvaCliPath() below. Exposed in
// case a future caller needs "next to me" resolution for something else
// that isn't ava_cli.
std::filesystem::path SelfExecutableDir();

// Best-effort guess at where ava_cli(.exe) lives, assuming the common
// build layouts (see the .cpp for the exact candidates tried): same
// folder as ava_studio.exe, or a sibling `avalang/` output folder from
// the same CMake build tree. Returns an empty path if nothing was
// found -- callers should fall back to an explicit user-configured path
// (see StudioSettings::build_ava_cli_path) in that case, same convention
// used for the Build panel's own "ava_cli path" setting.
std::filesystem::path DetectAvaCliPath();

} // namespace studio
