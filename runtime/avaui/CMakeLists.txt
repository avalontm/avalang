# ---------------------------------------------------------------------
# avalang.ui.dll -- platform-independent UI framework module.
#
# Phase 1 (Core Architecture) only: folder structure, public interface
# (UIModule), internal interfaces (Fwd.h, IUIPlatformFactory), platform
# bridge stubs. No component tree, no layout, no rendering yet -- see
# docs/AVALANG_UI_IMPLEMENTATION_PLAN.md and docs/AVALANG_UI_PROGRESS.md.
#
# Included from the root CMakeLists.txt via:
#   option(AVA_BUILD_UI ...)
#   if(AVA_BUILD_UI) add_subdirectory(ui) endif()
#
# Independent of avalang (the compiler/VM/runtime target) at the source
# level -- ui/ does not include anything from core/src/. It only reuses
# the PAL's UI service stubs (core/platform/interfaces/services/ui/),
# same headers avalang.dll already ships.
# ---------------------------------------------------------------------

# Phase 7 -- Scene Graph uses glm (installed via vcpkg by build_avaui.bat
# or manually with "vcpkg install glm"). It's a CMake-config package, so a
# bare #include isn't enough -- it must be found and linked explicitly.
find_package(glm CONFIG REQUIRED)

set(AVA_UI_SOURCES
    src/UIModule.cpp
    # Phase 13 -- Interface freeze: shared color-parsing utility (fixes
    # the string-vs-struct color gap found while freezing the 9
    # interfaces, see docs/AVAUI_FASE13_INTERFACE_FREEZE.md)
    src/common/ColorParse.cpp
    # Phase 2 -- Component Tree
    src/components/PropertyValue.cpp
    src/components/Component.cpp
    src/components/ComponentTreeImpl.cpp
    src/components/ComponentTree.cpp
    # Phase 3 -- Layout Engine
    src/layout/LayoutProperties.cpp
    src/layout/LayoutNode.cpp
    src/layout/TextMeasure.cpp
    src/layout/LayoutEngineImpl.cpp
    src/layout/LayoutEngine.cpp
    # Phase 4 -- State System
    src/state/StateImpl.cpp
    src/state/IState.cpp
    src/state/StateBindingImpl.cpp
    src/state/StateBinding.cpp
    # Phase 5 -- Event System
    src/events/Event.cpp
    src/events/EventDispatcher.cpp
    src/events/EventDispatcherFactory.cpp
    # Phase 6 -- Render Tree
    src/render_tree/RenderNode.cpp
    src/render_tree/RenderTree.cpp
    src/render_tree/RenderTreeFactory.cpp
    # Phase 7 -- Scene Graph
    src/scene/SceneNode.cpp
    src/scene/SceneGraph.cpp
    src/scene/SceneGraphFactory.cpp
    # Phase 8 -- Render Commands
    src/commands/RenderCommandSink.cpp
    # Phase 9 -- Renderer Interface
    src/renderer/BaseRenderer.cpp
    src/renderer/NullRenderer.cpp
    src/renderer/RendererFactory.cpp
    # Phase 10 -- HTML Backend
    src/renderer/HTMLRenderer.cpp
    # Phase 14 -- Parser .avaui -> ComponentTree
    src/parser/AvauiParser.cpp
    src/parser/AvauiPropertyCoercion.cpp
    # Phase 14 -- Scene Graph -> Render Commands walker (this phase's
    # own end-to-end deliverable needs it; see AvauiParser.cpp header
    # comment and docs/AVAUI_FASE13_INTERFACE_FREEZE.md)
    src/commands/SceneCommandWalker.cpp
    # Phase 15 -- Resources (fonts, images, icons, localization)
    src/resources/ResourceProvider.cpp
    src/resources/ResourcePathResolver.cpp
    src/resources/LocalizationProvider.cpp
    # Phase 16 -- Theme system
    src/theme/DefaultTheme.cpp
    src/theme/RenderTheme.cpp
    # Phase 17 -- Controls: Button (primer control real, valida Theme + Resources)
    src/controls/Button.cpp
    # Phase 18 -- Controls: resto del set base
    src/controls/Text.cpp
    src/controls/Image.cpp
    src/controls/Container.cpp
    src/controls/TextBox.cpp
    src/controls/CheckBox.cpp
    src/controls/RadioButton.cpp
    src/controls/ComboBox.cpp
    src/controls/Dialog.cpp
    # Phase 19 -- Animation
    src/animation/IAnimatable.cpp
    src/animation/Easing.cpp
    src/animation/AnimationClock.cpp
    src/animation/Timeline.cpp
    src/animation/AnimationController.cpp
    src/animation/AnimationBinding.cpp
)

# Platform bridge backend (compile-time selection, no runtime OS
# detection -- same convention as avalang.dll's PAL, see
# docs/AVALANG_UI_PROGRESS.md, "Alcance actual: solo Windows").
#   Windows -> ui/src/platform/windows (real desde Fase 11)
#   Linux   -> ui/src/platform/linux   (STUB, en estudio / pausado)
#   macOS   -> ui/src/platform/macos   (STUB, en estudio / pausado)
if(WIN32)
    list(APPEND AVA_UI_SOURCES
        # Phase 11 -- Native Backend (Windows only)
        src/platform/windows/WinUIPlatformFactory.cpp
        src/platform/windows/WinWindow.cpp
        src/platform/windows/WinMouse.cpp
        src/platform/windows/WinKeyboard.cpp
        src/platform/windows/WinCursor.cpp
        src/platform/windows/WinClipboard.cpp
        src/platform/windows/WinDisplay.cpp
        src/renderer/GdiRenderer.cpp
    )
elseif(APPLE)
    list(APPEND AVA_UI_SOURCES src/platform/macos/MacUIPlatformFactory.cpp)
    message(STATUS "avalang.ui: macOS backend is a STUB (en estudio)")
elseif(UNIX)
    list(APPEND AVA_UI_SOURCES src/platform/linux/LinUIPlatformFactory.cpp)
    message(STATUS "avalang.ui: Linux backend is a STUB (en estudio)")
else()
    message(FATAL_ERROR "avalang.ui: unsupported target platform (no UI platform backend available)")
endif()

if(AVA_BUILD_SHARED)
    add_library(avalang_ui SHARED ${AVA_UI_SOURCES})
    target_compile_definitions(avalang_ui PUBLIC AVA_UI_BUILD_SHARED)
else()
    add_library(avalang_ui STATIC ${AVA_UI_SOURCES})
endif()

target_compile_definitions(avalang_ui PRIVATE AVA_UI_BUILDING_LIBRARY)

target_include_directories(avalang_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_include_directories(avalang_ui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)

target_compile_features(avalang_ui PRIVATE cxx_std_20)

target_link_libraries(avalang_ui PUBLIC glm::glm)

if(WIN32)
    # Phase 11 -- Native Backend: gdi32/user32 for WinWindow/GdiRenderer,
    # shcore for WinDisplay's per-monitor DPI (GetDpiForMonitor).
    target_link_libraries(avalang_ui PRIVATE gdi32 user32 shcore)
    # UNICODE/_UNICODE so TEXT()-less macros (IDC_ARROW, CreateWindowEx,
    # RegisterClassEx, ...) resolve to the *W entry points used
    # throughout ui/src/platform/windows/ -- WinWindow.cpp calls the W
    # APIs explicitly, but stock resource macros like IDC_ARROW are
    # ANSI/wide depending on this define, not on which entry point you
    # call yourself.
    target_compile_definitions(avalang_ui PRIVATE UNICODE _UNICODE)
endif()

install(TARGETS avalang_ui
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(DIRECTORY src/ DESTINATION include 
    FILES_MATCHING PATTERN "*.h"
    EXCLUDE PATTERN "platform/windows/*"
    EXCLUDE PATTERN "platform/linux/*"
    EXCLUDE PATTERN "platform/macos/*"
)

# ---------------------------------------------------------------------
# Fase 14 -- entregable end-to-end: .avaui real -> Parser -> ComponentTree
# -> LayoutEngine -> IRenderTree -> ISceneGraph -> SceneCommandWalker ->
# HTMLRenderer -> HTML. Ver tests/integration/avaui_demos/AvauiPipelineDemo.cpp y
# docs/AVAUI_FASE14_PARSER.md.
#
# Opcional (OFF por defecto) para no forzarlo en builds que solo quieren
# la libreria. Reusa los headers PRIVATE de avalang_ui (commands/*.h)
# porque el demo vive fuera del target de la libreria en si.
# ---------------------------------------------------------------------
option(AVA_BUILD_UI_TESTS "Build avalang.ui end-to-end demo/test executables" OFF)
if(AVA_BUILD_UI_TESTS)
    # Fase 14 -- .avaui parser end-to-end demo
    add_executable(ava_ui_pipeline_demo ${CMAKE_SOURCE_DIR}/tests/integration/avaui_demos/AvauiPipelineDemo.cpp)
    target_link_libraries(ava_ui_pipeline_demo PRIVATE avalang_ui)
    target_include_directories(ava_ui_pipeline_demo PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_features(ava_ui_pipeline_demo PRIVATE cxx_std_20)

    # Fase 17 -- Button control end-to-end demo (validates Theme + Resources)
    add_executable(ava_button_demo ${CMAKE_SOURCE_DIR}/tests/integration/avaui_demos/ButtonDemo.cpp)
    target_link_libraries(ava_button_demo PRIVATE avalang_ui)
    target_include_directories(ava_button_demo PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_features(ava_button_demo PRIVATE cxx_std_20)

    # Fase 18 -- rest of base controls end-to-end demo
    add_executable(ava_controls_demo ${CMAKE_SOURCE_DIR}/tests/integration/avaui_demos/ControlsDemo.cpp)
    target_link_libraries(ava_controls_demo PRIVATE avalang_ui)
    target_include_directories(ava_controls_demo PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_features(ava_controls_demo PRIVATE cxx_std_20)

    # Fase 19 -- Animation end-to-end demo (parser `animate` block ->
    # AnimationController -> click trigger -> Update() loop -> HTML)
    add_executable(ava_animation_demo ${CMAKE_SOURCE_DIR}/tests/integration/avaui_demos/AnimationDemo.cpp)
    target_link_libraries(ava_animation_demo PRIVATE avalang_ui)
    target_include_directories(ava_animation_demo PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_features(ava_animation_demo PRIVATE cxx_std_20)

    # Fase 20.2 -- validation demo: parse + render 3 fixtures that
    # exercise sub-fases 20.2.2 (extends), 20.2.3 (imports), 20.2.4
    # (state/code). See tests/integration/avahost_fixtures/. Only the static
    # portion of the pipeline -- import resolution, layout composition,
    # and state cache round-trip live in avahost/ and require the
    # full UiComponentResolver + RenderAvauiDynamicWithLayoutAndState.
    add_executable(ava_ui_pipeline_fase20_2_demo ${CMAKE_SOURCE_DIR}/tests/integration/avaui_demos/UiPipelineFase20_2Demo.cpp)
    target_link_libraries(ava_ui_pipeline_fase20_2_demo PRIVATE avalang_ui)
    target_include_directories(ava_ui_pipeline_fase20_2_demo PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/runtime/avahost/src
    )
    target_compile_features(ava_ui_pipeline_fase20_2_demo PRIVATE cxx_std_20)
endif()
