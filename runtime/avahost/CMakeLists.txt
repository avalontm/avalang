# ---------------------------------------------------------------------
# AvaHost -- official hosting platform for the AvaLang ecosystem.
#
# Same model as Ava Studio: a native C++ executable that links directly
# against the `avalang` core library target (no FFI, no separate .dll
# boundary at build time -- C++ calling C++, via public/include/avalang.h
# only, per docs/architecture/AVAHOST_IMPLEMENTATION_PLAN.md section 10).
#
# Included from the root CMakeLists.txt via:
#   option(AVA_BUILD_AVAHOST ...)
#   if(AVA_BUILD_AVAHOST) add_subdirectory(avahost) endif()
# ---------------------------------------------------------------------

include(FetchContent)

# nlohmann/json -- only used for appsettings.json / manifest parsing.
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(json)

set(AVAHOST_SOURCES
    src/core/logger.cpp
    src/core/crash_handler.cpp
    src/core/seh_guard.cpp
    src/core/session_manager.cpp
    src/runtime/runtime_host.cpp
    src/rendering/event_binder.cpp
    src/component/dotted_path.cpp
    src/web/transport/socket.cpp
    src/web/protocol/http_parser.cpp
    src/web/protocol/url_codec.cpp
    src/web/server/http_server.cpp
    src/web/server/thread_pool.cpp
    src/web/routing/router.cpp
    src/web/static/static_file_server.cpp
    src/web/server/app.cpp
    src/config/app_config.cpp
    src/config/app_manifest.cpp
    src/watch/file_watcher.cpp
    src/plugin/plugin_loader.cpp
    src/cli_commands.cpp
    src/main.cpp
)

if(WIN32)
    # PAL Fase 2 (docs/PAL_PROGRESS.md): plugin_loader.cpp usa
    # ava::platform::ILibraryLoader en vez de LoadLibrary/GetProcAddress
    # crudos. Se compila directo aqui (en vez de depender de que
    # `avalang` exporte simbolos internos del PAL) porque WinLibrary.cpp
    # es una unidad pequena y autocontenida, sin dependencias del VM.
    list(APPEND AVAHOST_SOURCES ${CMAKE_SOURCE_DIR}/runtime/avalang/platform/windows/WinLibrary.cpp)

    # Delay-load failure hook for avalang.dll/avalang_ui.dll -- see
    # src/core/delay_load_guard.cpp for why (without it, a missing DLL
    # kills the process before main() with zero console output).
    list(APPEND AVAHOST_SOURCES src/core/delay_load_guard.cpp)
endif()

add_executable(avahost ${AVAHOST_SOURCES})

target_include_directories(avahost PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/runtime/avalang/api/include
    ${CMAKE_SOURCE_DIR}/runtime/avalang/src
    ${CMAKE_SOURCE_DIR}/runtime/avalang
)

target_link_libraries(avahost PRIVATE avalang nlohmann_json::nlohmann_json)

# /EHa (as opposed to the default /EHsc) makes the compiler treat any
# instruction as potentially able to raise a structured (hardware)
# exception, not just explicit `throw`s -- required for
# core/seh_guard.h's _set_se_translator to actually intercept an access
# violation happening inside `handler(request)` in this file. Scoped to
# this one translation unit only (not the whole `avahost` target): /EHa
# has a real cost (it disables some optimizations and changes what the
# compiler assumes can throw), and every other file already gets normal
# C++ exception safety from the default /EHsc -- only the file that
# calls the request handler needs the wider net.
if(WIN32 AND MSVC)
    set_source_files_properties(src/web/server/http_server.cpp PROPERTIES COMPILE_OPTIONS "/EHa")
endif()

if(WIN32 AND MSVC AND AVA_BUILD_SHARED)
    target_link_options(avahost PRIVATE /DELAYLOAD:avalang.dll)
    target_link_libraries(avahost PRIVATE delayimp)
endif()

# ---------------------------------------------------------------------
# avaui (runtime/avaui, AVA_BUILD_UI, default ON) is now the only
# .avaui render path avahost has -- the legacy RuntimeHost/HtmlRenderer
# path was removed. Root CMakeLists.txt adds runtime/avaui *before*
# runtime/avahost, so the `avalang_ui` target already exists here
# whenever AVA_BUILD_UI is ON.
# ---------------------------------------------------------------------
if(TARGET avalang_ui)
    target_sources(avahost PRIVATE
        src/rendering/ui_pipeline_static_renderer.cpp
        # Fase 20.0 -- VM bridge (state/click handlers) + the dynamic
        # pipeline that binds them before rendering. See
        # src/rendering/ui_pipeline_dynamic_renderer.h.
        src/rendering/ui_vm_state_bridge.cpp
        src/rendering/ui_vm_event_bridge.cpp
        src/rendering/ui_pipeline_dynamic_renderer.cpp
        # Fase 20.2 -- imports resolver for the new avaui engine.
        src/rendering/ui_component_resolver.cpp
        # Fase 2 -- bytecode cache (content-hash based render cache)
        src/rendering/ui_bytecode_cache.cpp
    )
    target_link_libraries(avahost PRIVATE avalang_ui)
    target_compile_definitions(avahost PRIVATE AVAHOST_HAS_UI_PIPELINE)
    if(WIN32 AND MSVC AND AVA_BUILD_SHARED)
        target_link_options(avahost PRIVATE /DELAYLOAD:avalang_ui.dll)
    endif()
    message(STATUS "avahost: AVA_BUILD_UI is ON -- .avaui routes will render via the avaui engine")
else()
    message(WARNING "avahost: AVA_BUILD_UI is OFF -- avahost has no .avaui render path in this configuration; configure with -DAVA_BUILD_UI=ON")
endif()

target_compile_features(avahost PRIVATE cxx_std_20)

# AvaHost esta enfocado en Windows por ahora (ver docs/PAL_PROGRESS.md);
# el bloque Linux/macOS se deja tal cual (paused/stub) y no se toca en
# esta fase.
if(WIN32)
    target_link_libraries(avahost PRIVATE ws2_32)
else()
    target_link_libraries(avahost PRIVATE dl pthread)
endif()

install(TARGETS avahost RUNTIME DESTINATION bin)
