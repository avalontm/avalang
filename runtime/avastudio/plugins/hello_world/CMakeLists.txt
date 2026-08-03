# ---------------------------------------------------------------------
# hello_world -- Fase 0's acceptance-criterion sample plugin.
#
# Deliberately the smallest possible plugin: one .cpp, one header
# dependency (plugin_api.h), zero dependencies on ImGui, GLFW, avalang,
# or anything else in this repo -- proving the ABI boundary really is
# self-contained. A real plugin (ai_agent, Fase 1+) is free to add its
# own dependencies (libcurl, its own threads, etc.) -- none of that
# leaks back into ava_studio.exe either way, since only plugin_api.h
# crosses the boundary.
#
# Built as its own SHARED library target (not part of ava_studio's own
# sources -- see the parent CMakeLists.txt's STUDIO_SOURCES, which does
# NOT include this file) and copied into ava_studio's plugins/ folder
# as a POST_BUILD step, so `ava_studio.exe` and `hello_world.dll` never
# even need the same compiler flags beyond plugin_api.h's plain-C
# layout guarantee.
# ---------------------------------------------------------------------

add_library(hello_world_plugin SHARED hello_world_plugin.cpp)

target_include_directories(hello_world_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/plugins
)

set_target_properties(hello_world_plugin PROPERTIES
    OUTPUT_NAME "hello_world"
    PREFIX ""              # no "lib" prefix on Linux/macOS -- match Windows' plain "hello_world.dll" naming
    RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:ava_studio>/plugins"   # Windows .dll
    LIBRARY_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:ava_studio>/plugins"   # Linux/macOS .so/.dylib
    # MSVC (unlike GCC/Clang's default -fvisibility=default on Linux/
    # macOS) does not export ANY symbol from a DLL unless explicitly
    # told to -- extern "C" alone (see ava_plugin_abi_version/init/
    # shutdown in hello_world_plugin.cpp) is not enough on Windows. This
    # makes CMake auto-generate a .def exporting every public symbol,
    # so GetProcAddress(..., "ava_plugin_init") etc. can actually find
    # them -- see PluginHost::LoadAll's "does not export ..." log line
    # otherwise. No-op on Linux/macOS, where this was never the issue.
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

# Belt-and-suspenders copy for generators/platforms where the
# OUTPUT_DIRECTORY properties above don't already land the artifact in
# the right place (e.g. multi-config generators build into a
# $<CONFIG>/ subfolder that only resolves at build time) -- mirrors
# ava_studio's own POST_BUILD copy of data/ in the parent CMakeLists.txt.
add_custom_command(TARGET hello_world_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ava_studio>/plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:hello_world_plugin>"
            "$<TARGET_FILE_DIR:ava_studio>/plugins/$<TARGET_FILE_NAME:hello_world_plugin>"
    COMMENT "Copying hello_world plugin next to ava_studio.exe's plugins/ folder"
)
