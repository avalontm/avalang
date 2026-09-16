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
#   option(AVA_BUILD_AVAHOST_NATIVE_LIB ...)
#   if(AVA_BUILD_AVAHOST OR AVA_BUILD_AVAHOST_NATIVE_LIB)
#       add_subdirectory(avahost)
#   endif()
#
# Fase 21.x -- este archivo puede correr con AVA_BUILD_AVAHOST=OFF (solo
# AVA_BUILD_AVAHOST_NATIVE_LIB=ON): en ese caso NO se compila el avahost
# completo (web server, plugins, etc. -- eso solo lo necesita el producto
# AvaHost en si), pero SI se deja disponible el target `avahost_native`
# (mas abajo) -- una libreria estatica reutilizable con el "native app
# host" (registro de la clase Application + loop de ventana nativa via
# avalang_ui_win) que usan tanto avanative.exe (F5/Preview en Ava Studio)
# como los binarios "Desktop UI" que empaqueta avapack (ver
# runtime/avapack/CMakeLists.txt, AVAPACK_DESKTOP_UI, y
# runtime/avacli/src/build_command.cpp, NeedsNativeUiHost).
# ---------------------------------------------------------------------

include(FetchContent)

# nlohmann/json -- usado para appsettings.json / manifest parsing (avahost
# "completo") y tambien por runtime_host.cpp/ui_vm_state_bridge.cpp (parte
# de avahost_native, mas abajo) -- por eso se resuelve aca sin condicionar
# a AVA_BUILD_AVAHOST.
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(json)

# ---------------------------------------------------------------------
# avahost_native -- reusable static library (Fase 21.x).
#
# Extraida de lo que antes vivia solo dentro del ejecutable `avanative`:
# el bridge VM<->AvaUI (state/event bridge, lifecycle), el router, y
# sobre todo native/native_app_host.cpp -- RunNativeApp(), que registra
# la clase `Application` (Application.run(viewPath)) y corre el loop de
# ventana nativa sobre avalang_ui_win (IPlatform/AppSurface +
# EventDispatcher + GdiRenderer).
#
# NO incluye src/native/main_native.cpp -- ese archivo define el propio
# `int main(argc, argv)` de cada consumidor (avanative.exe lo tiene en
# este mismo directorio; avapack tiene su propia copia en
# runtime/avapack/src/main_native.cpp, que arma el PackagedManifest desde
# el proyecto embebido en vez de leerlo de argv/env) -- por eso queda
# fuera de la libreria, cada ejecutable lo compila aparte.
#
# Independiente de AVA_BUILD_AVAHOST (ver AVA_BUILD_AVAHOST_NATIVE_LIB en
# el CMakeLists.txt raiz) -- avapack puede pedir este target sin
# necesitar compilar el avahost completo.
# ---------------------------------------------------------------------
if(WIN32 AND TARGET avalang_ui_win)
    add_library(avahost_native STATIC
        src/runtime/runtime_host.cpp
        src/rendering/ui_vm_state_bridge.cpp
        src/rendering/ui_vm_event_bridge.cpp
        src/rendering/ui_vm_lifecycle.cpp
        src/runtime/vm_ava_view.cpp
        src/web/routing/router.cpp
        src/native/native_app_host.cpp
    )

    # PUBLIC: cualquier consumidor que linkee avahost_native (avanative,
    # avapack_stub_ui, el .exe empacado con AVAPACK_DESKTOP_UI=ON) incluye
    # directamente "native/native_app_host.h" (ver packaged_runtime.cpp)
    # sin tener que repetir este include dir a mano.
    target_include_directories(avahost_native PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_include_directories(avahost_native PRIVATE
        # Ver mismo comentario que en target_include_directories(avahost ...)
        # mas abajo -- IPlatform.h necesita la raiz del repo en el include
        # path.
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/runtime/avalang/api/include
        ${CMAKE_SOURCE_DIR}/runtime/avalang/src
        ${CMAKE_SOURCE_DIR}/runtime/avalang
    )

    # PUBLIC: avalang_ui_win trae consigo (via avalang_ui, PUBLIC en su
    # propio CMakeLists.txt) el include dir de runtime/avaui/src -- de ahi
    # sale "platform/windows/WinBackendEntry.h" que necesitan tanto
    # native_app_host.cpp (adentro de esta libreria) como el
    # main_native.cpp de cada consumidor (afuera). Declararlo PUBLIC aca
    # evita que avanative/avapack tengan que volver a linkear avalang_ui/
    # avalang_ui_win a mano.
    target_link_libraries(avahost_native PUBLIC avalang avalang_ui avalang_ui_win nlohmann_json::nlohmann_json)
    target_compile_definitions(avahost_native PUBLIC AVAHOST_HAS_UI_PIPELINE)
    target_compile_features(avahost_native PUBLIC cxx_std_20)

    if(MSVC AND AVA_BUILD_SHARED)
        target_link_libraries(avahost_native PUBLIC delayimp)
        target_link_options(avahost_native INTERFACE /DELAYLOAD:avalang_ui_win.dll)
    endif()

    ava_apply_optimization_options(avahost_native)

    # -------------------------------------------------------------------
    # avanative -- Fase 7: contraparte de escritorio nativa del `avahost`
    # web. Ahora es solo main_native.cpp -- toda la logica real vive en
    # avahost_native (arriba).
    #
    # Fase 21 -- Packaging: avalang_ui ya no trae el backend de plataforma
    # adentro (ver runtime/avaui/CMakeLists.txt); el backend
    # (avalang_ui_win) y su LinkBackend() llegan transitivamente via
    # avahost_native (PUBLIC, arriba) -- sin eso, GetPlatform() revienta en
    # runtime con "no platform backend registered".
    # -------------------------------------------------------------------
    add_executable(avanative src/native/main_native.cpp)
    target_include_directories(avanative PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}
    )
    target_link_libraries(avanative PRIVATE avahost_native)

    # avanative es la app de escritorio final (ventana nativa via
    # avalang_ui_win), no una herramienta de consola -- sin esto, el
    # linker deja el subsystem por defecto (CONSOLE), asi que Windows
    # siempre abre una consola detras de la ventana real (el "flash"
    # que se ve al lanzar avanative.exe).
    #
    # main_native.cpp define `int main(argc, argv)` (no WinMain), asi
    # que ademas del subsystem hace falta forzar el entry point a
    # mainCRTStartup -- si no, con MSVC y subsystem WINDOWS el linker
    # busca WinMain por defecto y falla con LNK2019.
    set_target_properties(avanative PROPERTIES WIN32_EXECUTABLE TRUE)
    if(MSVC)
        target_link_options(avanative PRIVATE /ENTRY:mainCRTStartup)
    endif()

    ava_apply_optimization_options(avanative)
    install(TARGETS avanative RUNTIME DESTINATION bin)

    # Fase 21 -- Packaging: opcional, arma dist/win/avanative/ con
    # exactamente los archivos que este .exe necesita para correr
    # suelto (ver cmake/AvaPackage.cmake). OFF por defecto -- no cambia
    # el build normal, es solo para probar el empaquetado modular.
    option(AVA_PACKAGE_DIST "Assemble a runnable dist/win/ folder for avanative after building it (Fase 21)" OFF)
    if(AVA_PACKAGE_DIST)
        include(AvaPackage)
        set(AVA_PACKAGE_DIST_APP_FILE "${CMAKE_SOURCE_DIR}/samples/web/testproj/routes/index.avaui" CACHE FILEPATH
            "App file copied next to avanative.exe by AVA_PACKAGE_DIST")
        ava_package_windows_app(avanative "${AVA_PACKAGE_DIST_APP_FILE}" "${CMAKE_BINARY_DIR}/dist/win/avanative")
    endif()
elseif(WIN32 AND TARGET avalang_ui)
    message(WARNING "avahost: avalang_ui_win no esta disponible (AVA_BUILD_UI_BACKEND_WIN=OFF) -- avahost_native/avanative no se compilan. Configura con -DAVA_BUILD_UI_BACKEND_WIN=ON.")
elseif(AVA_BUILD_AVAHOST_NATIVE_LIB)
    message(WARNING "avahost: AVA_BUILD_AVAHOST_NATIVE_LIB=ON pero no hay ningun backend de UI disponible para esta plataforma -- avahost_native no se compila (por ahora solo existe el backend de Windows, ver native/native_app_host.cpp).")
endif()

# ---------------------------------------------------------------------
# avahost -- el producto "completo": servidor web + hosting de rutas
# .avaui + plugins + CLI de administracion. Todo lo de abajo solo se
# procesa cuando AVA_BUILD_AVAHOST=ON -- AVA_BUILD_AVAHOST_NATIVE_LIB=ON
# a secas (sin AVA_BUILD_AVAHOST) deja este bloque entero sin tocar, solo
# para poder ofrecer avahost_native/avanative arriba.
# ---------------------------------------------------------------------
if(AVA_BUILD_AVAHOST)

set(AVAHOST_SOURCES
    src/core/logger.cpp
    src/core/crash_handler.cpp
    src/core/seh_guard.cpp
    src/core/session_manager.cpp
    src/runtime/runtime_host.cpp
    src/rendering/event_binder.cpp
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
    # Bare repo root -- IPlatform.h (runtime/avaui/src/platform/contract/)
    # incluye "runtime/avalang/platform/interfaces/services/ui/UIPlatformInterfaces.h"
    # como ruta absoluta desde la raiz del repo (ver el mismo include dir en
    # avalang_ui/avalang_ui_win, runtime/avaui/CMakeLists.txt). Sin esto,
    # cualquier .cpp de avahost que llegue a IPlatform.h transitivamente
    # falla con C1083 "No se puede abrir el archivo incluir".
    ${CMAKE_SOURCE_DIR}
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
        src/rendering/ui_vm_lifecycle.cpp
        src/rendering/ui_pipeline_dynamic_renderer.cpp
        # Fase 2 -- bytecode cache (content-hash based render cache)
        src/rendering/ui_bytecode_cache.cpp
        # PLAN_AVAUI_VISTAS_COMO_CLASES Fase 1 -- IAvaView sobre la C API
        # de instancias (Fase 0).
        src/runtime/vm_ava_view.cpp
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

# Fase 8 -- Optimizacion: LTO + dead-code elimination + symbol stripping
# en Release (ver cmake/OptimizationOptions.cmake).
ava_apply_optimization_options(avahost)

install(TARGETS avahost RUNTIME DESTINATION bin)

# ---------------------------------------------------------------------
# Fase 21 -- Packaging: layout "Web" del plan (ver
# runtime/avaui/web/README.md para que hay adentro de avalang.ui.web y
# que hueco queda -- avahost sigue generando esos mismos archivos desde
# sus literales embebidos, esto solo arma la carpeta dist/ con la copia
# empaquetada al lado de un app.avaui de ejemplo).
#
# Distinto del AVA_PACKAGE_DIST de arriba (Windows-only, atado a
# avanative/avalang_ui_win) -- avahost es multiplataforma, así que esta
# opción no depende de WIN32 ni de ningun target avalang_ui_*.
# ---------------------------------------------------------------------
option(AVA_PACKAGE_DIST_WEB "Assemble a dist/web/avahost/ folder with avalang.ui.web after building avahost (Fase 21)" OFF)
if(AVA_PACKAGE_DIST_WEB)
    include(AvaPackage)
    set(AVA_PACKAGE_DIST_WEB_APP_FILE "${CMAKE_SOURCE_DIR}/samples/web/testproj/routes/index.avaui" CACHE FILEPATH
        "App file copied next to avalang.ui.web/ by AVA_PACKAGE_DIST_WEB")
    ava_package_web_dist(avahost "${AVA_PACKAGE_DIST_WEB_APP_FILE}" "${CMAKE_BINARY_DIR}/dist/web/avahost")
endif()

endif() # AVA_BUILD_AVAHOST
