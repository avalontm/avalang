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
    src/runtime/runtime_host.cpp
    src/rendering/html_renderer.cpp
    src/web/socket.cpp
    src/web/http_parser.cpp
    src/web/http_server.cpp
    src/web/router.cpp
    src/web/url_codec.cpp
    src/web/static_file_server.cpp
    src/web/app.cpp
    src/config/app_config.cpp
    src/config/app_manifest.cpp
    src/watch/file_watcher.cpp
    src/plugin/plugin_loader.cpp
    src/cli_commands.cpp
    src/main.cpp
)

add_executable(avahost ${AVAHOST_SOURCES})

target_include_directories(avahost PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/public/include
)

target_link_libraries(avahost PRIVATE avalang nlohmann_json::nlohmann_json)

target_compile_features(avahost PRIVATE cxx_std_20)

if(WIN32)
    target_link_libraries(avahost PRIVATE ws2_32)
else()
    target_link_libraries(avahost PRIVATE dl pthread)
endif()

install(TARGETS avahost RUNTIME DESTINATION bin)
