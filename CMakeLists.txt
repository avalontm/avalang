cmake_minimum_required(VERSION 3.20)
project(avalang LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force vcpkg to use static-md triplet where antlr4-runtime is installed
set(VCPKG_TARGET_TRIPLET x64-windows-static-md)
set(ENV{VCPKG_TARGET_TRIPLET} x64-windows-static-md)
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows-static-md")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

option(AVA_BUILD_SHARED "Build avalang as a shared library (.dll/.so)" ON)
option(AVA_BUILD_CLI "Build the standalone ava_cli (compile/run scripts)" ON)
option(AVA_BUILD_STUDIO "Build ava_studio, the ImGui-based IDE shell (fetches GLFW + Dear ImGui via CMake FetchContent)" OFF)
option(AVA_BUILD_AVAHOST "Build avahost, the native C++ app hosting platform (links directly against avalang)" OFF)
option(AVA_BUILD_UI "Build avalang.ui.dll, the platform-independent UI framework (Phase 1: Core Architecture only)" ON)

# ANTLR4 y libffi son opcionales en tiempo de configure: si no se
# encuentran, avalang igual compila (frontend stub / extern sin FFI
# real). Ver cmake/FindAntlr4Jar.cmake y cmake/FindLibFFI.cmake.
include(FindAntlr4Jar)
include(FindLibFFI)

add_subdirectory(runtime/avalang)

if(AVA_BUILD_CLI)
    add_subdirectory(runtime/avacli)
endif()

# ---------------------------------------------------------------------
# Subproyectos opcionales (OFF por defecto para que un `cmake ..` plano
# no necesite red para bajar GLFW/ImGui/nlohmann-json vía FetchContent).
# ---------------------------------------------------------------------
if(AVA_BUILD_STUDIO)
    add_subdirectory(runtime/avastudio)
endif()

if(AVA_BUILD_UI)
    add_subdirectory(runtime/avaui)
endif()

if(AVA_BUILD_AVAHOST)
    add_subdirectory(runtime/avahost)
endif()
