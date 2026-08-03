include(FetchContent)

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(json)

add_library(ai_agent_plugin SHARED
    ai_agent_plugin.cpp
    openrouter_client.cpp
    config.cpp
    context_builder.cpp
    memory.cpp
    tools.cpp
    avalang_reference.cpp
)

target_include_directories(ai_agent_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/plugins
)

target_link_libraries(ai_agent_plugin PRIVATE
    CURL::libcurl
    nlohmann_json::nlohmann_json
    Threads::Threads
)

set_target_properties(ai_agent_plugin PROPERTIES
    OUTPUT_NAME "ai_agent"
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:ava_studio>/plugins"
    LIBRARY_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:ava_studio>/plugins"
    # MSVC (unlike GCC/Clang's default -fvisibility=default on Linux/
    # macOS) does not export ANY symbol from a DLL unless explicitly
    # told to -- extern "C" alone (see ava_plugin_abi_version/init/
    # shutdown in ai_agent_plugin.cpp) is not enough on Windows. This
    # makes CMake auto-generate a .def exporting every public symbol,
    # so GetProcAddress(..., "ava_plugin_init") etc. can actually find
    # them -- see PluginHost::LoadAll's "does not export ..." log line
    # otherwise. No-op on Linux/macOS, where this was never the issue.
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

add_custom_command(TARGET ai_agent_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ava_studio>/plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:ai_agent_plugin>"
            "$<TARGET_FILE_DIR:ava_studio>/plugins/$<TARGET_FILE_NAME:ai_agent_plugin>"
    COMMENT "Copying ai_agent plugin next to ava_studio.exe's plugins/ folder"
)
