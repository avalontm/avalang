cmake_minimum_required(VERSION 3.20)
project(avalang LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(AVA_STATIC_CRT
    "Linkea el CRT de MSVC estatico (/MT) en vez de dinamico (/MD) y usa el triplet x64-windows-static de vcpkg. Recomendado para binarios (.exe/.dll) que se distribuyen sueltos sin instalar el redistributable de VC++."
    OFF)

if(WIN32)
    if(AVA_STATIC_CRT)
        set(VCPKG_TARGET_TRIPLET x64-windows-static)
        set(ENV{VCPKG_TARGET_TRIPLET} x64-windows-static)
        list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows-static")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        set(VCPKG_TARGET_TRIPLET x64-windows-static-md)
        set(ENV{VCPKG_TARGET_TRIPLET} x64-windows-static-md)
        list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows-static-md")
    endif()
endif()
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

option(AVA_BUILD_SHARED "Build avalang as a shared library (.dll/.so)" ON)
option(AVA_BUILD_CLI "Build the standalone ava_cli (compile/run scripts)" ON)
option(AVA_BUILD_STUDIO "Build ava_studio, the ImGui-based IDE shell (fetches GLFW + Dear ImGui via CMake FetchContent)" OFF)
option(AVA_BUILD_AVAHOST "Build avahost, the native C++ app hosting platform (links directly against avalang)" OFF)
# Fase 21.x -- ver runtime/avahost/CMakeLists.txt. Permite procesar ese
# subdirectorio SOLO para dejar disponible el target avahost_native
# (Application class + view loop nativo) sin compilar el avahost
# completo (web server, plugins, etc.) -- usado por el empaquetado de
# apps Desktop UI de avapack (ver runtime/avapack/CMakeLists.txt,
# AVAPACK_DESKTOP_UI, y runtime/avacli/src/build_command.cpp).
option(AVA_BUILD_AVAHOST_NATIVE_LIB
    "Process runtime/avahost only far enough to build avahost_native (the reusable native desktop app host), without the rest of AvaHost -- used by avapack's Desktop-UI packaging path"
    OFF)
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
# Empaquetado Desktop UI (ver NeedsNativeUiHost en build_command.cpp):
# avahost_native/avapack_stub_ui necesitan el target avalang_ui_win, que
# solo existe si runtime/avaui se procesa con add_subdirectory (no hay
# forma de "importar" un backend nativo suelto). Antes, pedir esto forzaba
# tambien a recompilar avalang (el motor: ANTLR, VM, etc.) desde fuente,
# porque runtime/avaui quedaba atado a NOT AVA_PACK_USE_PREBUILT_AVALANG
# mas abajo. avaui es independiente de avalang a nivel de fuente (linkea
# PRIVATE contra el target avalang via su import lib, ver
# runtime/avaui/CMakeLists.txt) -- no hace falta que avalang tambien se
# recompile. Esta opcion permite compilar SOLO avaui desde fuente mientras
# avalang se sigue reusando prebuilt.
option(AVA_PACK_BUILD_UI_FROM_SOURCE
    "Compilar runtime/avaui (avalang_ui + backends nativos) desde fuente aunque AVA_PACK_USE_PREBUILT_AVALANG=ON este reusando el avalang.dll/lib prebuilt -- usado por el empaquetado Desktop UI de avapack"
    OFF)

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
        message(FATAL_ERROR "AVA_PACK_USE_PREBUILT_AVALANG=ON requiere -DAVA_PREBUILT_AVALANG_DIR=<carpeta con avalang.dll/avalang.lib (y avalang_ui.dll/avalang_ui.lib si AVA_BUILD_UI=ON)>")
    endif()
    set(_ava_prebuilt_required_files avalang.dll avalang.lib)
    if(AVA_BUILD_UI AND NOT AVA_PACK_BUILD_UI_FROM_SOURCE)
        # Si AVA_PACK_BUILD_UI_FROM_SOURCE=ON, avalang_ui se compila desde
        # fuente mas abajo (add_subdirectory) en vez de importarse -- no
        # hace falta que un avalang_ui.dll/lib prebuilt exista.
        list(APPEND _ava_prebuilt_required_files avalang_ui.dll avalang_ui.lib)
    endif()
    foreach(_ava_prebuilt_file ${_ava_prebuilt_required_files})
        if(NOT EXISTS "${AVA_PREBUILT_AVALANG_DIR}/${_ava_prebuilt_file}")
            message(FATAL_ERROR "AVA_PACK_USE_PREBUILT_AVALANG=ON pero no se encontro '${_ava_prebuilt_file}' en AVA_PREBUILT_AVALANG_DIR ('${AVA_PREBUILT_AVALANG_DIR}'). Compila build_cli primero (ver build_cli.bat) o desactiva esta opcion para compilar avalang desde fuente.")
        endif()
    endforeach()

    message(STATUS "avalang: usando avalang.dll ya compilado en ${AVA_PREBUILT_AVALANG_DIR} (AVA_PACK_USE_PREBUILT_AVALANG=ON, no se recompila desde fuente)")

    add_library(avalang SHARED IMPORTED GLOBAL)
    set_target_properties(avalang PROPERTIES
        IMPORTED_LOCATION "${AVA_PREBUILT_AVALANG_DIR}/avalang.dll"
        IMPORTED_IMPLIB "${AVA_PREBUILT_AVALANG_DIR}/avalang.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/runtime/avalang/api/include"
    )
    if(AVA_BUILD_UI AND AVA_PACK_BUILD_UI_FROM_SOURCE)
        message(STATUS "avalang: AVA_PACK_BUILD_UI_FROM_SOURCE=ON -- avalang_ui (y sus backends nativos) se compilan desde fuente en runtime/avaui, linkeando contra el avalang.dll prebuilt de arriba (no se importa un avalang_ui.dll prebuilt).")
    elseif(AVA_BUILD_UI)
        add_library(avalang_ui SHARED IMPORTED GLOBAL)
        set_target_properties(avalang_ui PROPERTIES
            IMPORTED_LOCATION "${AVA_PREBUILT_AVALANG_DIR}/avalang_ui.dll"
            IMPORTED_IMPLIB "${AVA_PREBUILT_AVALANG_DIR}/avalang_ui.lib"
        )
        message(STATUS "avalang: usando avalang_ui.dll ya compilado en ${AVA_PREBUILT_AVALANG_DIR} (AVA_BUILD_UI=ON)")
    else()
        message(STATUS "avalang: AVA_BUILD_UI=OFF -- no se requiere/importa avalang_ui.dll. OJO: si este avalang.dll prebuilt fue compilado con AVA_BUILD_UI=ON en su momento, YA tiene la dependencia a avalang_ui.dll horneada en su propio binario -- este flag no puede quitarla retroactivamente. Para un avalang.dll realmente libre de avalang_ui necesitas un prebuilt generado con AVA_BUILD_UI=OFF, o compilar avalang desde fuente (AVA_PACK_USE_PREBUILT_AVALANG=OFF).")
    endif()
else()
    add_subdirectory(runtime/avalang)
endif()

if(AVA_BUILD_CLI)
    add_subdirectory(runtime/avacli)
endif()

if(AVA_TARGET_BAREKERNEL)
    add_subdirectory(runtime/avabare)
    # Fase B1 de plan_avapack_barekernel.md -- main_barekernel.cpp +
    # embedded_avb.cpp, el reemplazo con .avb embebido para
    # ava_barekernel_runner. Vive bajo runtime/avapack/src/barekernel/
    # (no runtime/avabare/) porque conceptualmente es parte de
    # avapack_barekernel, no del bootstrap mínimo original -- ver el plan.
    add_subdirectory(runtime/avapack/src/barekernel avapack_barekernel)
endif()

# ---------------------------------------------------------------------
# Subproyectos opcionales (OFF por defecto para que un `cmake ..` plano
# no necesite red para bajar GLFW/ImGui/nlohmann-json vía FetchContent).
# ---------------------------------------------------------------------
if(AVA_BUILD_UI AND (NOT AVA_PACK_USE_PREBUILT_AVALANG OR AVA_PACK_BUILD_UI_FROM_SOURCE))
    add_subdirectory(runtime/avaui)
endif()

if(AVA_BUILD_STUDIO)
    if(NOT AVA_BUILD_UI)
        message(FATAL_ERROR "AVA_BUILD_STUDIO requires AVA_BUILD_UI (ava_studio links avalang_ui, see AVAUI_DESIGNER_REAL_RENDER_PLAN.md Fase 2)")
    endif()
    add_subdirectory(runtime/avastudio)
endif()

if(AVA_BUILD_AVAHOST OR AVA_BUILD_AVAHOST_NATIVE_LIB)
    add_subdirectory(runtime/avahost)
endif()

if(AVA_BUILD_PACK)
    add_subdirectory(runtime/avapack)
endif()