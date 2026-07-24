# ---------------------------------------------------------------------
# Ava Studio -- the visual IDE shell for AvaLang.
#
# Milestone 1 ("engine first"): a single ImGui window that links
# directly against the `avalang` core library (no FFI boundary, no
# JSON bridge -- this is C++ calling C++). It proves the full pipeline
# end to end:
#
#   .ava source --> ava_compile/ava_run --> Output panel
#   ava_ui_* calls --> AvaComponentTree --> Preview panel
#
# Panel layout matches the Explorer/Toolbox/Designer/Properties/Output
# sketch from the Ava Studio vision doc. Toolbox and a real drag/drop
# Designer come in the next milestone -- for now Preview renders a
# fixed demo tree to validate the component/property/child model.
# ---------------------------------------------------------------------

include(FetchContent)

# --- GLFW (window + input backend) ------------------------------------
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
# X11 alone is enough on Linux and avoids requiring wayland-scanner /
# wayland-protocols to be installed just to configure the build.
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# --- Dear ImGui (docking branch -- gives us the dockspace we need) ----
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

add_library(imgui_docking STATIC ${IMGUI_SOURCES})
target_include_directories(imgui_docking PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_docking PUBLIC glfw)

# --- Ava Studio executable ---------------------------------------------
set(STUDIO_SOURCES
    src/main.cpp
    src/engine/engine_bridge.cpp
    src/panels/explorer_panel.cpp
    src/panels/editor_panel.cpp
    src/panels/properties_panel.cpp
    src/panels/preview_panel.cpp
    src/panels/output_panel.cpp
)

add_executable(ava_studio ${STUDIO_SOURCES})

target_include_directories(ava_studio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/public/include
)

target_link_libraries(ava_studio PRIVATE avalang imgui_docking)

if(WIN32)
    target_link_libraries(ava_studio PRIVATE opengl32)
elseif(APPLE)
    target_link_libraries(ava_studio PRIVATE "-framework OpenGL")
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(ava_studio PRIVATE OpenGL::GL)
endif()
