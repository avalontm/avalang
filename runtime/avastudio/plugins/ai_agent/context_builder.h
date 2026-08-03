#pragma once

#include "plugin_api.h"

#include <cstddef>
#include <string>
#include <vector>

std::string BuildContextMessage(AvaStudioHost* host, size_t max_chars);

// Relative (to `root`) paths of every regular file under a project root,
// skipping the same noise directories (.git, build, node_modules, ...)
// BuildContextMessage's file tree does. Shared with tools.cpp's
// list_project_files so Fase 3's auto-context and Fase 4's tool call
// stay consistent about what counts as "the project's files".
std::vector<std::string> ListProjectFiles(const std::string& root, size_t max_files);
