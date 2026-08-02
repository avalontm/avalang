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
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
)

add_library(imgui_docking STATIC ${IMGUI_SOURCES})
target_include_directories(imgui_docking PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${imgui_SOURCE_DIR}/misc/cpp
)
target_link_libraries(imgui_docking PUBLIC glfw)

# --- ImGuiColorTextEdit (line numbers, syntax highlighting, autocomplete) --
# The editor widget behind the Code Editor panel. Like GLFW/ImGui above,
# this is fetched at configure time and compiled straight into ava_studio
# as a static library -- there is no separate .dll/.so to ship, the editor
# code ends up baked into ava_studio.exe.
#
# PATCH_COMMAND applies both patches in one `git apply` call (it accepts
# multiple patch files and applies them in order against the pristine
# clone):
#  1. patches/imguicolortextedit_interpolation.patch -- adds
#     Color::interpolation so `{expr}` inside an f-string colors
#     differently from the surrounding string (upstream has no hook for
#     this).
#  2. patches/imguicolortextedit_bold_keywords.patch -- adds an optional
#     second (bold) ImFont that TextEditor uses per-glyph for whichever
#     Color::... values the caller flags via SetBoldColors() (AvaLang
#     uses it for keyword/declaration), instead of a single font for
#     the whole Render() call.
#  3. patches/imguicolortextedit_doc_comment.patch -- adds
#     Color::docComment and Color::docParamTag plus a Language field
#     (docCommentPrefix) so a "##" doc-comment block colors differently
#     from a plain "#" comment, and the "@param" tag itself (not the name
#     or description that follows) inside a doc
#     block gets its own color too (upstream only has one flat
#     Color::comment for the whole singleLineComment match).
# See patches/README.md for why these are patches and not a fork. git
# apply fails loudly (breaks the CMake configure step) if any patch
# can't apply cleanly, rather than silently falling back to the
# unpatched library.
# NOTE (2026-07): pinned to the "Legacy" release tag (commit efd42a4) instead
# of GIT_TAG master. Upstream is mid-rewrite -- their README states master
# will eventually flip to a new, backwards-incompatible architecture once
# it's tested, and even before that, master drifted enough to shift
# TextEditor.cpp's line numbers and break the patch hunks below ("patch does
# not apply" at TextEditor.cpp:3647). The "Legacy" tag is upstream's own
# snapshot of the pre-rewrite architecture ("most backwards compatible with
# the various forks" per their release notes), so pinning to it fixes today's
# apply failure and stops master's eventual rewrite from silently breaking
# this build later. See patches/README.md.
FetchContent_Declare(
    imguicolortextedit
    GIT_REPOSITORY https://github.com/goossens/ImGuiColorTextEdit.git
    GIT_TAG        Legacy
    GIT_SHALLOW    TRUE
    PATCH_COMMAND  git apply --ignore-whitespace
                   ${CMAKE_CURRENT_SOURCE_DIR}/patches/imguicolortextedit_interpolation.patch
                   ${CMAKE_CURRENT_SOURCE_DIR}/patches/imguicolortextedit_bold_keywords.patch
                   ${CMAKE_CURRENT_SOURCE_DIR}/patches/imguicolortextedit_doc_comment.patch
    UPDATE_DISCONNECTED TRUE
)
FetchContent_MakeAvailable(imguicolortextedit)

add_library(text_editor STATIC ${imguicolortextedit_SOURCE_DIR}/TextEditor.cpp)
target_include_directories(text_editor PUBLIC ${imguicolortextedit_SOURCE_DIR})
target_link_libraries(text_editor PUBLIC imgui_docking)

# --- Ava Studio executable ---------------------------------------------
set(STUDIO_SOURCES
    src/main.cpp
    src/theme.cpp
    src/fonts/embedded_font.cpp
    src/languages/avalang_language.cpp
    src/languages/function_index.cpp
    src/languages/class_index.cpp
    src/languages/member_access_resolver.cpp
    src/languages/builtin_signatures.cpp
    src/languages/keyword_docs.cpp
    src/util/csv.cpp
    src/util/data_dir.cpp
    src/util/settings.cpp
    src/design/component_catalog.cpp
    src/design/design_document.cpp
    src/design/avaui_text.cpp
    src/design/component_resolver.cpp
    src/design/layout_engine.cpp
    src/design/state_eval.cpp
    src/engine/engine_bridge.cpp
    src/panels/explorer_panel.cpp
    src/panels/editor_panel.cpp
    src/panels/properties_panel.cpp
    src/panels/preview_panel.cpp
    src/panels/output_panel.cpp
    src/panels/titlebar_panel.cpp
    src/panels/toolbox_panel.cpp
    src/panels/designer_canvas.cpp
    src/platform/win32_titlebar.cpp
    src/branding/logo_texture.cpp
)

if(WIN32)
    list(APPEND STUDIO_SOURCES resources/app.rc)
endif()

add_executable(ava_studio ${STUDIO_SOURCES})

if(WIN32)
    # GUI subsystem, no console window. We still define int main() (not
    # WinMain) -- WIN32_EXECUTABLE alone is enough for MinGW (it auto-
    # generates the WinMain shim around main()), but MSVC's default entry
    # point for a /SUBSYSTEM:WINDOWS target is WinMainCRTStartup, so it
    # needs to be told explicitly to start at mainCRTStartup instead, or
    # linking fails with "unresolved external symbol WinMain".
    set_target_properties(ava_studio PROPERTIES WIN32_EXECUTABLE TRUE)
    if(MSVC)
        target_link_options(ava_studio PRIVATE /ENTRY:mainCRTStartup)
    endif()
endif()

target_include_directories(ava_studio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/runtime/avalang/api/include
)

target_link_libraries(ava_studio PRIVATE avalang imgui_docking text_editor)

# data/keyword_docs.csv, data/builtin_signatures.csv and
# data/component_catalog.csv -- the editable
# tooltip/autocomplete-help tables (see languages/keyword_docs.h and
# languages/builtin_signatures.h). Copied next to the .exe on every
# build, same folder ResolveDataDir() (util/data_dir.cpp) looks in at
# runtime, mirroring how ResolveWorkspaceDir() in main.cpp already
# resolves "scripts/" the same way. Uses copy_directory instead of
# copy_if_different so deleting a row from the source CSV during
# development is reflected in the build output too, not just additions.
add_custom_command(TARGET ava_studio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ava_studio>/data"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/data"
            "$<TARGET_FILE_DIR:ava_studio>/data"
    COMMENT "Copying data/ (keyword_docs.csv, builtin_signatures.csv, component_catalog.csv) next to ava_studio.exe"
)

if(WIN32)
    # dwmapi -- DwmExtendFrameIntoClientArea, used by
    # src/platform/win32_titlebar.cpp to keep the native drop shadow on
    # our borderless/custom-titlebar window.
    target_link_libraries(ava_studio PRIVATE opengl32 dwmapi)
elseif(APPLE)
    target_link_libraries(ava_studio PRIVATE "-framework OpenGL")
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(ava_studio PRIVATE OpenGL::GL)
endif()
