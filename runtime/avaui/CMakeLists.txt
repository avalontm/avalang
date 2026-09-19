# ---------------------------------------------------------------------
# avalang.ui.dll -- platform-independent UI framework module.
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
    src/components/ComponentTreeClone.cpp
    # Phase 3 -- Layout Engine
    src/layout/LayoutProperties.cpp
    src/layout/LayoutNode.cpp
    src/layout/TextMeasure.cpp
    src/layout/DensityScale.cpp
    # Real font measurement (stb_truetype-backed) -- replaces the old
    # average-char-width/flat-line-height heuristic. See FontRegistry.h.
    src/layout/FontRegistry.cpp
    src/resources/fonts/DefaultFontData.cpp
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
    src/events/HitTest.cpp
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
    src/commands/RenderCommandBounds.cpp
    # Phase 9 -- Renderer Interface
    src/renderer/BaseRenderer.cpp
    src/renderer/NullRenderer.cpp
    src/renderer/RendererFactory.cpp
    # Phase 10 -- HTML Backend
    src/renderer/HTMLRenderer.cpp
    # Phase 14 -- Parser .avaui -> ComponentTree
    src/parser/AvauiParser.cpp
    src/parser/AvauiPropertyCoercion.cpp
    # Fase 1 (PLAN_CENTRALIZACION_AVAUI.md) -- Writer ComponentTree ->
    # .avaui text, canonical inverse of AvauiParser::Parse for
    # avalang::ui::IComponent. Independent of the VM-side writer in
    # runtime/avalang/src/ui/avaui_text.cpp (see AvauiWriter.h).
    src/parser/AvauiWriter.cpp
    # Composition: extends/slot() page+layout composition (motor unico Fase 1)
    src/composition/ComposePageWithLayout.cpp

    # Phase 20 -- Events: auto-bind, dispatch, event names
    src/events/AutoBind.cpp

    # Phase 22 -- Component import resolver (motor unico Fase 3)
    src/resolver/DottedPath.cpp
    src/resolver/KnownComponentProperties.cpp
    src/resolver/ComponentResolver.cpp
    # Phase 14 -- Scene Graph -> Render Commands walker (this phase's
    # own end-to-end deliverable needs it; see AvauiParser.cpp header
    # comment and docs/AVAUI_FASE13_INTERFACE_FREEZE.md)
    src/commands/SceneCommandWalker.cpp
    src/perf/MemoryProfiler.cpp
    src/perf/StartupProfiler.cpp
    # Phase 15 -- Resources (fonts, images, icons, localization)
    src/resources/ResourceProvider.cpp
    src/resources/ResourcePathResolver.cpp
    src/resources/LocalizationProvider.cpp
    src/resources/ResourceManager.cpp
    # Phase 16 -- Theme system
    src/theme/DefaultTheme.cpp
    src/theme/RenderTheme.cpp
    # Project-configured fonts (app.ava `font "role" "name" "path"`
    # lines) -- shared by AvaHost's render pipeline and AvaStudio's
    # design-time preview so both resolve a project's custom font the
    # same way. See theme/ProjectFontOverrides.h.
    src/theme/ProjectFontOverrides.cpp
    # Project-configured styles.ava (`style *` / `style <type>` blocks)
    # -- simplified per-control-type theme overrides (color, font,
    # padding, margin, ...), same "shared by AvaHost + AvaStudio"
    # rationale as ProjectFontOverrides.cpp above. See
    # theme/ProjectStyleOverrides.h.
    src/theme/ProjectStyleOverrides.cpp
    # Project-configured animations.ava (`animation <target>:<trigger>`
    # blocks) -- today only `dialog:open`/`dialog:close`, overriding
    # HTMLRenderer's built-in modal fade duration/easing/opacity
    # range. Same "shared by AvaHost + AvaStudio", app.ava-declared-
    # file rationale as ProjectStyleOverrides.cpp above. See
    # theme/ProjectAnimationOverrides.h.
    src/theme/ProjectAnimationOverrides.cpp
    src/theme/VisualState.cpp
    # Phase 17 -- Controls: Button (primer control real, valida Theme + Resources)
    src/controls/Button.cpp
    src/controls/ButtonController.cpp
    # Phase 18 -- Controls: resto del set base
    src/controls/Text.cpp
    src/controls/Image.cpp
    src/controls/Icon.cpp
    src/controls/Container.cpp
    src/controls/ScrollView.cpp
    src/controls/TextBox.cpp
    src/controls/TextEditBuffer.cpp
    src/controls/TextBoxEditingController.cpp
    src/controls/CheckBox.cpp
    src/controls/CheckBoxController.cpp
    src/controls/RadioButton.cpp
    src/controls/RadioButtonController.cpp
    src/controls/ComboBox.cpp
    src/controls/ComboBoxController.cpp
    src/controls/ScrollViewController.cpp
    src/controls/Dialog.cpp
    src/controls/Link.cpp
    # CI-5 -- tabla unica de metadata por tipo de control (prop de estado,
    # evento, activacion), en vez de "if (type == ...)" repetido en cada
    # consumidor (ver AvaUI_CONTROLS_INTERACTION_IMPLEMENTATION_PLAN.md)
    src/controls/ControlMetadata.cpp
    # Phase 19 -- Animation
    src/animation/IAnimatable.cpp
    src/animation/Easing.cpp
    src/animation/AnimationClock.cpp
    src/animation/Timeline.cpp
    src/animation/AnimationController.cpp
    src/animation/AnimationBinding.cpp

    # Phase 9 -- Navigation
    src/navigation/Route.cpp
    src/navigation/RouteStack.cpp
    src/navigation/Navigator.cpp

    # Phase 11 -- Accessibility
    src/accessibility/AccessibilityNode.cpp
    src/accessibility/AccessibilityTree.cpp
    src/accessibility/AccessibilityMappings.cpp

    # Phase 6 (avastudio/avaui unification plan, Part B) -- component
    # type registry: single source for the catalog avastudio currently
    # keeps by hand.
    src/registry/ComponentTypeRegistry.cpp

    # Fase 21 -- Packaging: registro de IPlatform desacoplado de un
    # backend concreto (antes GetPlatform() era un simbolo resuelto en
    # link-time contra un unico .cpp de plataforma compilado en el
    # mismo binario). Con esto avalang_ui es un target 100% independiente
    # de plataforma -- el backend real (avalang_ui_win/linux/macos/
    # android) se linkea aparte y se autorregistra al cargar.
    src/platform/contract/PlatformRegistry.cpp

    src/capi/avaui_c_api.cpp

    # Ver comentario en src/view/IAvaView.h/.cpp -- sin este .cpp la
    # interfaz nunca se compila dentro de avalang_ui y su ctor/dtor
    # exportados no existen para consumidores externos (avahost/avanative).
    src/view/IAvaView.cpp
)

if(AVA_BUILD_SHARED)
    add_library(avalang_ui SHARED ${AVA_UI_SOURCES})
    target_compile_definitions(avalang_ui PUBLIC AVA_UI_BUILD_SHARED)

    # avalang.dll depende de avalang_ui.dll (ver target_link_libraries(avalang
    # PRIVATE avalang_ui) en runtime/avalang/CMakeLists.txt). En Windows no hay
    # RPATH, asi que avalang_ui.dll (y su import lib) tienen que terminar en la
    # MISMA carpeta que avalang.dll/avalang.lib para que cualquier ejecutable
    # que cargue avalang.dll (ava_cli, avapack_gen, el .exe empacado) pueda
    # resolver esa dependencia transitiva al arrancar, y para que ese mismo
    # directorio sirva como "carpeta prebuilt" completa y autocontenida (ver
    # AVA_PACK_USE_PREBUILT_AVALANG en el CMakeLists.txt raiz). Sin esto, avaui
    # cae en su carpeta de salida por defecto (runtime/avaui/<Config>/) y
    # cualquier .exe en runtime/avalang/<Config>/ falla al iniciar con
    # STATUS_DLL_NOT_FOUND (0xC0000135).
    set_target_properties(avalang_ui PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime/avalang"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime/avalang"
    )
else()
    add_library(avalang_ui STATIC ${AVA_UI_SOURCES})
endif()

target_compile_definitions(avalang_ui PRIVATE AVA_UI_BUILDING_LIBRARY)

target_include_directories(avalang_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/api/include
)

target_link_libraries(avalang_ui PRIVATE avalang)

target_include_directories(avalang_ui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
    # stb_truetype.h (vendored, single-header, public domain) --
    # FontRegistry.cpp's real glyph measurement. Header-only, so this
    # is a compile-time include only, nothing extra to link.
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
)

target_compile_features(avalang_ui PRIVATE cxx_std_20)

target_link_libraries(avalang_ui PUBLIC glm::glm)

# Fase 8 -- Optimizacion: LTO + dead-code elimination + symbol stripping
# en Release (ver cmake/OptimizationOptions.cmake).
ava_apply_optimization_options(avalang_ui)

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
    EXCLUDE PATTERN "platform/android/*"
    EXCLUDE PATTERN "platform/ios/*"
)

# ---------------------------------------------------------------------
# Fase 21 -- Packaging: backends de plataforma como paquetes separados.
#
# avalang_ui (arriba) es el nucleo, 100% independiente de plataforma --
# no referencia Win32/DOM/UIKit/JNI a nivel de link (Regla 1-3 del plan,
# seccion 23). Cada backend abajo es su propio target/DLL
# (avalang_ui_win.dll, avalang_ui_linux.so, ...), se linkea PRIVATE
# contra avalang_ui y se autorregistra en IPlatform (PlatformRegistry.cpp)
# via un inicializador estatico al cargar -- ver WinPlatform.cpp,
# LinPlatform.cpp, MacPlatform.cpp, AndroidPlatform.cpp.
#
# Un exe que use avalang.ui en runtime tiene que:
#   1. linkear contra el backend de su plataforma (avalang_ui_win, ...)
#   2. llamar su LinkBackend() una vez (referencia minima para que el
#      linker no descarte la dependencia -- ver runtime/avahost/src/
#      native/main_native.cpp)
#
# avalang_ui_ios es, igual que linux/macos, un STUB: registra un
# IPlatform valido (StubUIServices, sin UIKit real detras) para que el
# paquete avalang.ui.ios exista y el layout de Fase 21 tenga los 6
# backends definidos -- no dibuja nada. Implementar UIKit real
# (superficie, touch, teclado en pantalla) queda fuera de esta
# sub-fase, igual que linux/macos.
# ---------------------------------------------------------------------

function(ava_add_ui_backend name)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs SOURCES LINK_LIBS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(AVA_BUILD_SHARED)
        add_library(${name} SHARED ${ARG_SOURCES})
        target_compile_definitions(${name} PUBLIC AVA_UI_BUILD_SHARED)
        set_target_properties(${name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime/avalang"
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime/avalang"
            # Backend classes like GdiRenderer/WinKeyTranslation predate
            # Fase 21 and were never annotated with AVA_UI_API (they used
            # to live in the same binary as their callers). Rather than
            # touch every header, export everything on MSVC so consumers
            # like avanative can still link against them as a separate DLL.
            WINDOWS_EXPORT_ALL_SYMBOLS ON
        )
    else()
        add_library(${name} STATIC ${ARG_SOURCES})
    endif()

    target_compile_definitions(${name} PRIVATE AVA_UI_BACKEND_BUILDING_LIBRARY)
    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}
    )
    target_compile_features(${name} PRIVATE cxx_std_20)
    target_link_libraries(${name} PRIVATE avalang_ui avalang)
    if(ARG_LINK_LIBS)
        target_link_libraries(${name} PRIVATE ${ARG_LINK_LIBS})
    endif()

    # NO_LTO: WINDOWS_EXPORT_ALL_SYMBOLS (arriba) hace que CMake genere
    # exports.def corriendo `cmake -E __create_def` sobre los .obj del
    # target -- un parser propio de CMake que espera COFF normal. Con
    # /GL (INTERPROCEDURAL_OPTIMIZATION_RELEASE, ver OptimizationOptions.cmake)
    # los .obj de MSVC quedan en formato intermedio de LTCG en vez de COFF
    # comun, y ese parser puede fallar con "unrecognized file format" en
    # cualquiera de ellos (visto con WinPlatform.obj: funciona o no segun
    # que objetos quedaron cacheados de un build incremental anterior --
    # no es determinista). Ningun backend de UI es un target grande donde
    # LTO importe para performance (a diferencia de avalang/avalang_ui);
    # mas vale un binario chico sin whole-program-optimization que un
    # build que a veces rompe.
    ava_apply_optimization_options(${name} NO_LTO)

    install(TARGETS ${name}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )
endfunction()

set(_ava_ui_backend_win_default OFF)
set(_ava_ui_backend_linux_default OFF)
set(_ava_ui_backend_macos_default OFF)
set(_ava_ui_backend_android_default OFF)
set(_ava_ui_backend_ios_default OFF)
if(ANDROID)
    set(_ava_ui_backend_android_default ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(_ava_ui_backend_ios_default ON)
elseif(WIN32)
    set(_ava_ui_backend_win_default ON)
elseif(APPLE)
    set(_ava_ui_backend_macos_default ON)
elseif(UNIX)
    set(_ava_ui_backend_linux_default ON)
endif()

option(AVA_BUILD_UI_BACKEND_WIN "Build avalang_ui_win (Windows platform backend)" ${_ava_ui_backend_win_default})
option(AVA_BUILD_UI_BACKEND_LINUX "Build avalang_ui_linux (Linux platform backend, STUB)" ${_ava_ui_backend_linux_default})
option(AVA_BUILD_UI_BACKEND_MACOS "Build avalang_ui_macos (macOS platform backend, STUB)" ${_ava_ui_backend_macos_default})
option(AVA_BUILD_UI_BACKEND_ANDROID "Build avalang_ui_android (Android platform backend)" ${_ava_ui_backend_android_default})
option(AVA_BUILD_UI_BACKEND_IOS "Build avalang_ui_ios (iOS platform backend, STUB)" ${_ava_ui_backend_ios_default})

if(AVA_BUILD_UI_BACKEND_WIN)
    if(NOT WIN32)
        message(FATAL_ERROR "AVA_BUILD_UI_BACKEND_WIN requires a Windows toolchain")
    endif()
    ava_add_ui_backend(avalang_ui_win
        SOURCES
            src/platform/windows/WinPlatform.cpp
            src/platform/windows/WinAppSurface.cpp
            src/platform/windows/WinWindow.cpp
            src/platform/windows/WinMouse.cpp
            src/platform/windows/WinKeyboard.cpp
            src/platform/windows/WinCursor.cpp
            src/platform/windows/WinClipboard.cpp
            src/platform/windows/WinDisplay.cpp
            src/platform/windows/WinPlatformPaths.cpp
            src/platform/windows/WinKeyTranslation.cpp
            src/platform/windows/WinInputState.cpp
            src/platform/windows/WinPlatformServices.cpp
            src/platform/windows/WinAccessibilityBridge.cpp
            src/platform/windows/WinAccessibilityProvider.cpp
            src/platform/windows/GdiRenderer.cpp
            src/platform/windows/WinRendererRegistration.cpp
        LINK_LIBS gdi32 user32 shcore msimg32 uiautomationcore oleaut32
    )
    # Phase 11 -- UNICODE/_UNICODE so TEXT()-less macros (IDC_ARROW,
    # CreateWindowEx, RegisterClassEx, ...) resolve to the *W entry
    # points used throughout this backend.
    target_compile_definitions(avalang_ui_win PRIVATE UNICODE _UNICODE)
endif()

if(AVA_BUILD_UI_BACKEND_LINUX)
    ava_add_ui_backend(avalang_ui_linux
        SOURCES src/platform/linux/LinPlatform.cpp
    )
    message(STATUS "avalang_ui_linux: STUB backend (en estudio / pausado)")
endif()

if(AVA_BUILD_UI_BACKEND_MACOS)
    ava_add_ui_backend(avalang_ui_macos
        SOURCES src/platform/macos/MacPlatform.cpp
    )
    message(STATUS "avalang_ui_macos: STUB backend (en estudio / pausado)")
endif()

if(AVA_BUILD_UI_BACKEND_ANDROID)
    if(NOT ANDROID)
        message(FATAL_ERROR "AVA_BUILD_UI_BACKEND_ANDROID requires an Android NDK toolchain")
    endif()
    ava_add_ui_backend(avalang_ui_android
        SOURCES
            src/platform/android/AndroidJNI.cpp
            src/platform/android/AndroidJNIExports.cpp
            src/platform/android/AndroidAppLifecycle.cpp
            src/platform/android/AndroidMobileSurface.cpp
            src/platform/android/AndroidSafeArea.cpp
            src/platform/android/AndroidVirtualKeyboard.cpp
            src/platform/android/AndroidDisplayMetrics.cpp
            src/platform/android/AndroidInputState.cpp
            src/platform/android/AndroidClipboard.cpp
            src/platform/android/AndroidPlatformPaths.cpp
            src/platform/android/AndroidPlatformServices.cpp
            src/platform/android/AndroidPermissions.cpp
            src/platform/android/AndroidAppSurface.cpp
            src/platform/android/AndroidPlatform.cpp
            src/platform/android/AndroidBackNavigation.cpp
            src/platform/android/AndroidAccessibilityBridge.cpp
            src/platform/android/AndroidCanvasRenderer.cpp
            src/platform/android/AndroidRendererRegistration.cpp
        LINK_LIBS log android
    )
endif()

if(AVA_BUILD_UI_BACKEND_IOS)
    if(NOT (CMAKE_SYSTEM_NAME STREQUAL "iOS"))
        message(FATAL_ERROR "AVA_BUILD_UI_BACKEND_IOS requires an iOS toolchain")
    endif()
    ava_add_ui_backend(avalang_ui_ios
        SOURCES src/platform/ios/IosPlatform.cpp
    )
    message(STATUS "avalang_ui_ios: STUB backend (sin UIKit real)")
endif()

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

    # Phase 6 (avastudio/avaui unification plan, Part B) -- minimal
    # unit test for ComponentTypeRegistry (registers fake types, checks
    # lookup/listing/idempotency).
    add_executable(ava_component_type_registry_test ${CMAKE_SOURCE_DIR}/tests/unit/avaui/ComponentTypeRegistryTest.cpp)
    target_link_libraries(ava_component_type_registry_test PRIVATE avalang_ui)
    target_include_directories(ava_component_type_registry_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_features(ava_component_type_registry_test PRIVATE cxx_std_20)

    add_executable(ava_component_type_registry_controls_test ${CMAKE_SOURCE_DIR}/tests/unit/avaui/ComponentTypeRegistryControlsTest.cpp)
    target_link_libraries(ava_component_type_registry_controls_test PRIVATE avalang_ui)
    target_include_directories(ava_component_type_registry_controls_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_features(ava_component_type_registry_controls_test PRIVATE cxx_std_20)

    # Fase A.4 (plan unificado avastudio/avaui) -- tests unitarios de
    # Grid/Flex en LayoutEngineImpl: N x M columnas/filas, mas hijos que
    # celdas explicitas, un solo hijo, y direccion horizontal/vertical
    # para Flex (incluyendo el fallback vertical por defecto).
    add_executable(ava_grid_flex_layout_test ${CMAKE_SOURCE_DIR}/tests/unit/avaui/GridFlexLayoutTest.cpp)
    target_link_libraries(ava_grid_flex_layout_test PRIVATE avalang_ui)
    target_include_directories(ava_grid_flex_layout_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_features(ava_grid_flex_layout_test PRIVATE cxx_std_20)

    # Fase B.5 (plan unificado avastudio/avaui) -- TypeNames() de
    # AvauiPropertyCoercion.cpp derivado del registro (ya no hay tabla
    # hardcodeada de tipos reales), mas auditoria de PropertyAliases()
    # contra el registro para no repetir el bug de enabled/isEnabled.
    add_executable(ava_typename_property_alias_audit_test ${CMAKE_SOURCE_DIR}/tests/unit/avaui/TypeNameAndPropertyAliasAuditTest.cpp)
    target_link_libraries(ava_typename_property_alias_audit_test PRIVATE avalang_ui)
    target_include_directories(ava_typename_property_alias_audit_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_features(ava_typename_property_alias_audit_test PRIVATE cxx_std_20)

    add_executable(ava_container_spacing_defaults_test ${CMAKE_SOURCE_DIR}/tests/unit/avaui/ContainerSpacingDefaultsTest.cpp)
    target_link_libraries(ava_container_spacing_defaults_test PRIVATE avalang_ui)
    target_include_directories(ava_container_spacing_defaults_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_features(ava_container_spacing_defaults_test PRIVATE cxx_std_20)

    # Fase 21 -- Suite de tests de avaui (render commands, RenderTree,
    # HitTest, eventos, snapshots). Corre contra el pipeline real
    # (ComponentTree -> LayoutEngine -> RenderTree -> SceneGraph ->
    # SceneCommandWalker), sin mocks de renderer; HitTest y eventos
    # usan FakeMouse/FakeKeyboard/FakeTextInput (tests/FakeInput.h) para
    # accionar EventDispatcher::PollInput de verdad.
    add_executable(avaui_tests
        tests/main.cpp
        tests/RenderCommandTests.cpp
        tests/RenderTreeTests.cpp
        tests/HitTestTests.cpp
        tests/EventTests.cpp
        tests/SnapshotTests.cpp
        tests/HTMLRendererTests.cpp
        tests/ControlActivationTests.cpp
        tests/ControlMetadataTests.cpp
    )
    target_link_libraries(avaui_tests PRIVATE avalang_ui)
    target_include_directories(avaui_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_SOURCE_DIR}
    )
    target_compile_features(avaui_tests PRIVATE cxx_std_20)
endif()

option(AVA_BUILD_DIALOG_SIZE_PROBE "Build the standalone Dialog layout probe" OFF)
if(AVA_BUILD_DIALOG_SIZE_PROBE)
    add_executable(ava_dialog_size_probe ${CMAKE_SOURCE_DIR}/tests/integration/avaui_demos/DialogSizeProbe.cpp)
    target_link_libraries(ava_dialog_size_probe PRIVATE avalang_ui)
    target_include_directories(ava_dialog_size_probe PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_features(ava_dialog_size_probe PRIVATE cxx_std_20)
endif()
