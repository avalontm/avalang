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
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)


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

FetchContent_Declare(
    imguicolortextedit
    GIT_REPOSITORY https://github.com/goossens/ImGuiColorTextEdit.git
    GIT_TAG        Legacy
    GIT_SHALLOW    TRUE
    PATCH_COMMAND  git apply --ignore-whitespace
                   ${CMAKE_CURRENT_SOURCE_DIR}/patches/imguicolortextedit_interpolation.patch
                   ${CMAKE_CURRENT_SOURCE_DIR}/patches/imguicolortextedit_bold_keywords.patch
                   ${CMAKE_CURRENT_SOURCE_DIR}/patches/imguicolortextedit_doc_comment.patch
                   ${CMAKE_CURRENT_SOURCE_DIR}/patches/imguicolortextedit_import_path.patch
                   ${CMAKE_CURRENT_SOURCE_DIR}/patches/imguicolortextedit_interface_name.patch
                   ${CMAKE_CURRENT_SOURCE_DIR}/patches/imguicolortextedit_variable_name.patch
                   ${CMAKE_CURRENT_SOURCE_DIR}/patches/imguicolortextedit_smart_backspace_pair.patch
                   ${CMAKE_CURRENT_SOURCE_DIR}/patches/imguicolortextedit_dot_trigger.patch
                   ${CMAKE_CURRENT_SOURCE_DIR}/patches/imguicolortextedit_suggestion_icons.patch
                   ${CMAKE_CURRENT_SOURCE_DIR}/patches/imguicolortextedit_scroll_offset.patch
    UPDATE_DISCONNECTED TRUE
)
FetchContent_MakeAvailable(imguicolortextedit)

add_library(text_editor STATIC ${imguicolortextedit_SOURCE_DIR}/TextEditor.cpp)
target_include_directories(text_editor PUBLIC ${imguicolortextedit_SOURCE_DIR})
target_link_libraries(text_editor PUBLIC imgui_docking)

FetchContent_Declare(
    pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG        v1.14
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(pugixml)

# --- Ava Studio executable ---------------------------------------------
set(STUDIO_SOURCES
    src/main.cpp
    src/theme.cpp
    src/fonts/embedded_font.cpp
    src/languages/avalang_language.cpp
    src/languages/import_file_cache.cpp
    src/languages/block_scanner.cpp
    src/languages/fold_index.cpp
    src/languages/function_index.cpp
    src/languages/class_index.cpp
    src/languages/workspace_index.cpp
    src/languages/member_access_resolver.cpp
    src/languages/diagnostics_engine.cpp
    src/languages/lexer_utils.cpp
    src/languages/builtin_signatures.cpp
    src/languages/keyword_docs.cpp
    src/languages/code_formatter.cpp
    src/util/csv.cpp
    src/util/data_dir.cpp
    src/util/i18n.cpp
    src/util/settings.cpp
    src/util/ava_cli_locator.cpp
    src/util/process_log.cpp
    src/util/project_utils.cpp
    src/util/scaffold_templates.cpp
    src/util/host_platform.cpp
    src/project/avaproj_file.cpp
    src/project/avaproj_user_file.cpp
    src/project/project_config.cpp
    src/project/output_type_labels.cpp
    src/design/component_catalog.cpp
    src/design/design_document.cpp
    src/design/component_resolver.cpp
    src/design/state_eval.cpp
    src/design/live_render_bridge.cpp
    src/design/imgui_renderer.cpp
    src/engine/engine_bridge.cpp
    src/shortcuts/shortcut_registry.cpp
    src/panels/explorer_panel.cpp
    src/panels/editor_panel.cpp
    src/panels/syntax_highlight.cpp
    src/panels/properties_panel.cpp
    src/panels/settings_panel.cpp
    src/panels/build_panel.cpp
    src/panels/project_properties_panel.cpp
    src/panels/preview_panel.cpp
    src/panels/terminal_panel.cpp
    src/panels/logs_panel.cpp
    src/panels/problems_panel.cpp
    src/panels/find_in_project_panel.cpp
    src/panels/command_palette.cpp
    src/panels/quick_open_panel.cpp
    src/panels/new_project_panel.cpp
    src/panels/titlebar_panel.cpp
    src/panels/toolbox_panel.cpp
    src/panels/activity_bar_panel.cpp
    src/panels/designer_canvas.cpp
    src/panels/pending_edits_panel.cpp
    src/platform/win32_titlebar.cpp
    src/branding/logo_texture.cpp
    src/plugins/plugin_host.cpp
    src/plugins/plugin_ui_bridge.cpp
)

if(WIN32)
    list(APPEND STUDIO_SOURCES resources/app.rc)
endif()

add_executable(ava_studio ${STUDIO_SOURCES})

if(WIN32)
    set_target_properties(ava_studio PROPERTIES WIN32_EXECUTABLE TRUE)
    if(MSVC)
        target_link_options(ava_studio PRIVATE /ENTRY:mainCRTStartup)
    endif()
endif()

target_include_directories(ava_studio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/runtime/avalang/api/include
    ${CMAKE_SOURCE_DIR}/runtime/avaui/src
    ${CMAKE_SOURCE_DIR}/runtime/avalang
)

target_link_libraries(ava_studio PRIVATE avalang imgui_docking text_editor avalang_ui pugixml)

add_custom_command(TARGET ava_studio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ava_studio>/data"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/data"
            "$<TARGET_FILE_DIR:ava_studio>/data"
    COMMENT "Copying data/ (docs/, design/, langs/, scaffold/) next to ava_studio.exe"
)

# data/modules/ contiene stubs .ava de SOLO EDITOR (autocompletado/hints)
# para modulos nativos como `system` (Console/DateTime/Environment/IO/
# Diagnostics, ver data/modules/system.ava) -- se copian aparte, a
# <exe_dir>/modules, porque ResolveDefaultModulesDir() (util/data_dir.cpp)
# es la carpeta que ClassIndex/FunctionIndex usan como stdlib_dir por
# defecto cuando el usuario no configuro un modules_path propio en
# Settings o en las propiedades del proyecto.
add_custom_command(TARGET ava_studio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ava_studio>/modules"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/data/modules"
            "$<TARGET_FILE_DIR:ava_studio>/modules"
    COMMENT "Copying data/modules/ (stubs de autocompletado de 'system') to <exe_dir>/modules"
)

if(AVA_BUILD_SHARED)
    add_custom_command(TARGET ava_studio POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:avalang>"
                "$<TARGET_FILE_DIR:ava_studio>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:avalang_ui>"
                "$<TARGET_FILE_DIR:ava_studio>"
        COMMENT "Copying avalang.dll / avalang_ui.dll next to ava_studio.exe"
    )
endif()

if(WIN32)

    target_link_libraries(ava_studio PRIVATE opengl32 dwmapi)
elseif(APPLE)
    target_link_libraries(ava_studio PRIVATE "-framework OpenGL")
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(ava_studio PRIVATE OpenGL::GL)
endif()

add_subdirectory(plugins/hello_world)
add_subdirectory(plugins/ai_agent)

option(AVA_BUILD_STUDIO_TESTS "Build avastudio unit/regression test executables" OFF)
if(AVA_BUILD_STUDIO_TESTS)
    add_executable(ava_studio_grid_flex_regression_test
        ${CMAKE_SOURCE_DIR}/tests/unit/avastudio/GridFlexRegressionTest.cpp
        src/design/design_document.cpp
        src/design/live_render_bridge.cpp
    )
    target_include_directories(ava_studio_grid_flex_regression_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/runtime/avalang/api/include
        ${CMAKE_SOURCE_DIR}/runtime/avaui/src
    )
    target_link_libraries(ava_studio_grid_flex_regression_test PRIVATE avalang avalang_ui)
    target_compile_features(ava_studio_grid_flex_regression_test PRIVATE cxx_std_20)

    add_executable(ava_studio_component_catalog_wrapper_test
        ${CMAKE_SOURCE_DIR}/tests/unit/avastudio/ComponentCatalogWrapperTest.cpp
        src/design/component_catalog.cpp
        src/util/csv.cpp
        src/util/data_dir.cpp
    )
    target_include_directories(ava_studio_component_catalog_wrapper_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/runtime/avaui/src
    )
    target_link_libraries(ava_studio_component_catalog_wrapper_test PRIVATE avalang_ui)
    target_compile_features(ava_studio_component_catalog_wrapper_test PRIVATE cxx_std_20)

    add_executable(ava_studio_catalog_registry_cross_test
        ${CMAKE_SOURCE_DIR}/tests/unit/avastudio/CatalogRegistryCrossTest.cpp
        src/design/component_catalog.cpp
        src/util/csv.cpp
        src/util/data_dir.cpp
    )
    target_include_directories(ava_studio_catalog_registry_cross_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/runtime/avaui/src
    )
    target_link_libraries(ava_studio_catalog_registry_cross_test PRIVATE avalang_ui)
    target_compile_features(ava_studio_catalog_registry_cross_test PRIVATE cxx_std_20)
endif()