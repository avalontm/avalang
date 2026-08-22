cmake_minimum_required(VERSION 3.20)
# LANGUAGES CXX C: el C se agregó en Fase 3 de avapack, único punto del repo
# que compila fuente C directa (tiny-AES-c, ver
# runtime/avapack/third_party/tiny-aes-c/VENDOR.md) -- sin esto, CMake no
# tiene un compilador C configurado y (según el generador) puede terminar
# compilando aes.c como C++ igual, produciendo símbolos con name mangling
# que no matchean las declaraciones `extern "C"` del lado que lo consume.
project(avalang LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force vcpkg to use static-md triplet where antlr4-runtime is installed
# (Windows only -- en Linux/macOS se usan deps del sistema via find_package)
if(WIN32)
    set(VCPKG_TARGET_TRIPLET x64-windows-static-md)
    set(ENV{VCPKG_TARGET_TRIPLET} x64-windows-static-md)
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows-static-md")
endif()
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

option(AVA_BUILD_SHARED "Build avalang as a shared library (.dll/.so)" ON)
option(AVA_BUILD_CLI "Build the standalone ava_cli (compile/run scripts)" ON)
option(AVA_BUILD_STUDIO "Build ava_studio, the ImGui-based IDE shell (fetches GLFW + Dear ImGui via CMake FetchContent)" OFF)
option(AVA_BUILD_AVAHOST "Build avahost, the native C++ app hosting platform (links directly against avalang)" OFF)
option(AVA_BUILD_UI "Build avalang.ui.dll, the platform-independent UI framework (Phase 1: Core Architecture only)" ON)
option(AVA_BUILD_PACK "Build avapack (ava_cli build / avapack_gen -- ver runtime/avapack/README.md)" OFF)

# Para el arbol build_pack de `ava_cli build` (empacar .ava -> .exe): avalang
# ya esta compilado en build_cli\, y avapack_gen / el .exe empacado solo
# necesitan linkear contra el avalang.dll/avalang_ui.dll ya existentes (ver
# runtime/avapack/CMakeLists.txt) -- no hace falta recompilar avalang/avaui
# desde fuente cada vez. Con esta opcion ON, en vez de add_subdirectory
# (que compila desde cero) se declaran `avalang`/`avalang_ui` como targets
# IMPORTED apuntando a los binarios ya compilados en AVA_PREBUILT_AVALANG_DIR.
# Pensado solo para una configuracion "pack-only" (AVA_BUILD_CLI/STUDIO/
# AVAHOST=OFF) -- `ava_cli build` la activa automaticamente (ver
# runtime/avacli/src/build_command.cpp).
option(AVA_PACK_USE_PREBUILT_AVALANG
    "En vez de compilar avalang/avalang_ui desde fuente, reusar los .dll/.lib ya compilados en AVA_PREBUILT_AVALANG_DIR (solo para el arbol build_pack de ava_cli build)"
    OFF)
set(AVA_PREBUILT_AVALANG_DIR "" CACHE PATH
    "Carpeta con avalang.dll/avalang.lib/avalang_ui.dll/avalang_ui.lib ya compilados (requerido si AVA_PACK_USE_PREBUILT_AVALANG=ON)")

# ---------------------------------------------------------------------
# Fase 8 -- Optimizacion (ver avalang_runtime_stl_barekernel_plan.md).
# Define ava_apply_optimization_options(<target>), llamado desde cada
# subdirectorio; ver cmake/OptimizationOptions.cmake para el detalle
# (LTO, dead-code elimination, symbol stripping -- todo solo en Release).
# ---------------------------------------------------------------------
option(AVA_ENABLE_LTO "Habilitar LTO/IPO para builds Release (Fase 8)" ON)
include(OptimizationOptions)

# ANTLR4 y libffi son opcionales en tiempo de configure: si no se
# encuentran, avalang igual compila (frontend stub / extern sin FFI
# real). Ver cmake/FindAntlr4Jar.cmake y cmake/FindLibFFI.cmake.
include(FindAntlr4Jar)
include(FindLibFFI)

if(AVA_PACK_USE_PREBUILT_AVALANG)
    if("${AVA_PREBUILT_AVALANG_DIR}" STREQUAL "")
        message(FATAL_ERROR "AVA_PACK_USE_PREBUILT_AVALANG=ON requiere -DAVA_PREBUILT_AVALANG_DIR=<carpeta con avalang.dll/avalang.lib/avalang_ui.dll/avalang_ui.lib>")
    endif()
    foreach(_ava_prebuilt_file avalang.dll avalang.lib avalang_ui.dll avalang_ui.lib)
        if(NOT EXISTS "${AVA_PREBUILT_AVALANG_DIR}/${_ava_prebuilt_file}")
            message(FATAL_ERROR "AVA_PACK_USE_PREBUILT_AVALANG=ON pero no se encontro '${_ava_prebuilt_file}' en AVA_PREBUILT_AVALANG_DIR ('${AVA_PREBUILT_AVALANG_DIR}'). Compila build_cli primero (ver build_cli.bat) o desactiva esta opcion para compilar avalang desde fuente.")
        endif()
    endforeach()

    message(STATUS "avalang: usando avalang.dll/avalang_ui.dll ya compilados en ${AVA_PREBUILT_AVALANG_DIR} (AVA_PACK_USE_PREBUILT_AVALANG=ON, no se recompila desde fuente)")

    add_library(avalang SHARED IMPORTED GLOBAL)
    set_target_properties(avalang PROPERTIES
        IMPORTED_LOCATION "${AVA_PREBUILT_AVALANG_DIR}/avalang.dll"
        IMPORTED_IMPLIB "${AVA_PREBUILT_AVALANG_DIR}/avalang.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/runtime/avalang/api/include"
    )
    add_library(avalang_ui SHARED IMPORTED GLOBAL)
    set_target_properties(avalang_ui PROPERTIES
        IMPORTED_LOCATION "${AVA_PREBUILT_AVALANG_DIR}/avalang_ui.dll"
        IMPORTED_IMPLIB "${AVA_PREBUILT_AVALANG_DIR}/avalang_ui.lib"
    )
else()
    add_subdirectory(runtime/avalang)
endif()

if(AVA_BUILD_CLI)
    add_subdirectory(runtime/avacli)
endif()

if(AVA_TARGET_BAREKERNEL)
    add_subdirectory(runtime/avabare)
endif()

# ---------------------------------------------------------------------
# Subproyectos opcionales (OFF por defecto para que un `cmake ..` plano
# no necesite red para bajar GLFW/ImGui/nlohmann-json vía FetchContent).
# ---------------------------------------------------------------------
if(AVA_BUILD_UI AND NOT AVA_PACK_USE_PREBUILT_AVALANG)
    add_subdirectory(runtime/avaui)
endif()

if(AVA_BUILD_STUDIO)
    if(NOT AVA_BUILD_UI)
        message(FATAL_ERROR "AVA_BUILD_STUDIO requires AVA_BUILD_UI (ava_studio links avalang_ui, see AVAUI_DESIGNER_REAL_RENDER_PLAN.md Fase 2)")
    endif()
    add_subdirectory(runtime/avastudio)
endif()

if(AVA_BUILD_AVAHOST)
    add_subdirectory(runtime/avahost)
endif()

if(AVA_BUILD_PACK)
    add_subdirectory(runtime/avapack)
endif()
