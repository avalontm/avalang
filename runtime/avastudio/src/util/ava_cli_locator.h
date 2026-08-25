#pragma once

#include <filesystem>

namespace studio {

std::filesystem::path SelfExecutableDir();

std::filesystem::path DetectAvaCliPath();

}
