# ---------------------------------------------------------------------
# avalang -- core library (VM + compiler frontend + C API) y ava_cli.
#
# Incluido desde el CMakeLists.txt raíz vía add_subdirectory(runtime/avalang).
# La detección de ANTLR4 y libffi vive en cmake/FindAntlr4Jar.cmake y
# cmake/FindLibFFI.cmake (incluidos desde la raíz antes de este add_subdirectory,
# así que sus variables ya están disponibles aquí).
# ---------------------------------------------------------------------

set(GRAMMAR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/grammar)
set(GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

if(antlr4-runtime_FOUND AND JAVA_EXECUTABLE AND ANTLR4_JAR)
    set(AVA_HAVE_ANTLR TRUE)
    message(STATUS "avalang: antlr4-runtime found, building full frontend")

    add_custom_command(
        OUTPUT
            ${GENERATED_DIR}/AvaLangLexer.cpp
            ${GENERATED_DIR}/AvaLangParser.cpp
            ${GENERATED_DIR}/AvaLangBaseListener.cpp
        COMMAND ${JAVA_EXECUTABLE} -jar ${ANTLR4_JAR}
                -Dlanguage=Cpp
                -visitor
                -o ${GENERATED_DIR}
                ${GRAMMAR_DIR}/AvaLang.g4
        DEPENDS ${GRAMMAR_DIR}/AvaLang.g4
        COMMENT "Generating ANTLR4 C++ parser from AvaLang.g4"
    )

    set(GENERATED_SOURCES
        ${GENERATED_DIR}/AvaLangLexer.cpp
        ${GENERATED_DIR}/AvaLangParser.cpp
        ${GENERATED_DIR}/AvaLangBaseListener.cpp
    )
    set(FRONTEND_SOURCES
        src/frontend/denter.cpp
        src/frontend/frontend_antlr.cpp
        src/ast/ast_builder.cpp
        src/compiler/compiler.cpp
        ${GENERATED_SOURCES}
    )
else()
    set(AVA_HAVE_ANTLR FALSE)
    message(STATUS "avalang: antlr4-runtime NOT found, building with stub frontend "
                    "(ava_compile() will report a clear error; VM/C-API/CLI still build)")
    set(FRONTEND_SOURCES src/frontend/frontend_stub.cpp)
endif()

# ---------------------------------------------------------------------
# Core library sources (PRIVATE - do not export)
# ---------------------------------------------------------------------
set(CORE_SOURCES
    src/vm/value.cpp
    src/vm/vm_helpers.cpp
    src/vm/vm.cpp
    src/vm/vm_core.cpp
    src/vm/vm_call.cpp
    src/vm/vm_file.cpp
    src/vm/vm_import.cpp
    src/vm/vm_arith.cpp
    src/vm/vm_compare.cpp
    src/vm/vm_containers.cpp
    src/vm/vm_classes.cpp
    src/vm/vm_call_op.cpp
    src/vm/vm_slice.cpp
    src/vm/vm_exceptions.cpp
    src/vm/vm_errors.cpp
    src/vm/vm_extern.cpp
    src/vm/vm_platform_accessor.cpp
    src/vm/vm_async.cpp
    src/vm/module.cpp
    src/vm/coroutine.cpp
    src/vm/vm_task.cpp
    src/vm/gc_trace.cpp
    src/vm/gc_sweep.cpp
    platform/memory/MemoryFileSystem.cpp
    src/compiler/proto_io.cpp
    src/compiler/obfuscate.cpp
    src/builtins/builtin_strings.cpp
    src/builtins/builtin_lists.cpp
    src/builtins/builtin_dicts.cpp
    src/builtins/builtin_coroutine.cpp
    src/builtins/builtin_async.cpp
    src/builtins/builtin_registry.cpp
    src/builtins/builtin_shared.cpp
    src/builtins/builtin_natives.cpp
    src/builtins/builtin_mem.cpp
    src/builtins/builtin_init.cpp
    src/builtins/system_module.cpp
    src/ui/builtins.cpp
    ${FRONTEND_SOURCES}
)

# ---------------------------------------------------------------------
# Platform Abstraction Layer backend (compile-time selection, no runtime
# OS detection).
# ---------------------------------------------------------------------
if(AVA_TARGET_BAREKERNEL)
    set(PLATFORM_SOURCES
        platform/barekernel/BareKernelClock.cpp
        platform/barekernel/BareKernelConsole.cpp
        platform/barekernel/BareKernelEnvironment.cpp
        platform/barekernel/BareKernelFileSystem.cpp
        platform/barekernel/BareKernelLibrary.cpp
        platform/barekernel/BareKernelMemory.cpp
        platform/barekernel/BareKernelMutex.cpp
        platform/barekernel/BareKernelPlatform.cpp
        platform/barekernel/BareKernelProcess.cpp
        platform/barekernel/BareKernelThread.cpp
        platform/barekernel/BareKernelTimer.cpp
        # Definen avastd::AvaThrow/CurrentErrorFrame/ava_setjmp/ava_longjmp
        # (ver ava_error.h). Faltaba en esta lista -- por eso el link
        # anterior tiraba "undefined reference" para esos 3 simbolos pese a
        # que estan implementados: el .cpp que los define simplemente nunca
        # se compilaba para este target.
        platform/barekernel/stdcompat/ava_error.cpp
        # mem*/str* globales (memcpy/memmove/memset/memcmp/strlen) y el
        # resto de libm/ABI de C++ que este toolchain freestanding
        # (--without-headers, sin libstdc++/libsupc++/libm) no provee. Ver
        # el comentario grande al inicio de ese archivo.
        platform/barekernel/BareKernelRuntimeSupport.cpp
    )
    message(STATUS "avalang: BareKernel backend selected")
elseif(WIN32)
    set(PLATFORM_SOURCES
        platform/windows/WinFileSystem.cpp
        platform/windows/WinThread.cpp
        platform/windows/WinMutex.cpp
        platform/windows/WinClock.cpp
        platform/windows/WinLibrary.cpp
        platform/windows/WinConsole.cpp
        platform/windows/WinEnvironment.cpp
        platform/windows/WinProcess.cpp
        platform/windows/WinTimer.cpp
        platform/windows/WinMemory.cpp
        platform/windows/WinPlatform.cpp
    )
elseif(APPLE)
    set(PLATFORM_SOURCES
        platform/macos/MacFileSystem.cpp
        platform/macos/MacThread.cpp
        platform/macos/MacMutex.cpp
        platform/macos/MacClock.cpp
        platform/macos/MacLibrary.cpp
        platform/macos/MacConsole.cpp
        platform/macos/MacEnvironment.cpp
        platform/macos/MacProcess.cpp
        platform/macos/MacTimer.cpp
        platform/macos/MacMemory.cpp
        platform/macos/MacPlatform.cpp
    )
    message(STATUS "avalang: macOS PAL backend is a STUB (Phase 6 not implemented yet)")
elseif(UNIX)
    set(PLATFORM_SOURCES
        platform/linux/LinFileSystem.cpp
        platform/linux/LinThread.cpp
        platform/linux/LinMutex.cpp
        platform/linux/LinClock.cpp
        platform/linux/LinLibrary.cpp
        platform/linux/LinConsole.cpp
        platform/linux/LinEnvironment.cpp
        platform/linux/LinProcess.cpp
        platform/linux/LinTimer.cpp
        platform/linux/LinMemory.cpp
        platform/linux/LinPlatform.cpp
    )
    message(STATUS "avalang: Linux PAL backend functional (POSIX + pthread + dl)")
else()
    message(FATAL_ERROR "avalang: unsupported target platform (no PAL backend available)")
endif()

list(APPEND CORE_SOURCES ${PLATFORM_SOURCES})

# ---------------------------------------------------------------------
# Public library (core + C API)
# ---------------------------------------------------------------------
set(PUBLIC_SOURCES
    ${CORE_SOURCES}
    api/src/c_api.cpp
)

if(AVA_BUILD_UI)
    list(APPEND PUBLIC_SOURCES api/src/c_api_ui.cpp)
endif()

if(AVA_TARGET_BAREKERNEL)
    # CMAKE_SYSTEM_NAME=Generic (necesario para cross-compilar contra el
    # toolchain freestanding i686-elf, ver cmake/toolchain-i686-elf.cmake)
    # no trae reglas de "como armar una libreria compartida" -- esas viven
    # en los modulos Platform/Linux.cmake, Platform/Windows.cmake, etc.,
    # que Generic no carga. Consecuencia real (confirmada: un build asi
    # produjo un libavalang.a de 50 objetos en vez de un .so) es que
    # add_library(... SHARED ...) NO genera un ELF ET_DYN aca, aunque no
    # tire error. Por eso, para AVA_TARGET_BAREKERNEL, SIEMPRE se compila
    # como STATIC (que si funciona de forma confiable) y el .so real se
    # arma aparte, con un paso de link explicito (ver mas abajo).
    add_library(avalang STATIC ${PUBLIC_SOURCES})

    # Estas flags se pasaban antes SOLO en el paso de link manual de mas
    # abajo (avalang_barekernel_so) -- demasiado tarde. Para cuando ese
    # comando corre, los .obj de avalang ya fueron compilados (arriba, en
    # add_library) SIN estas flags, así que igual traían secciones
    # .eh_frame reales (con referencia a __gxx_personality_v0) y RTTI real
    # (typeinfo apuntando a las vtables de __cxxabiv1::__class_type_info /
    # __si_class_type_info) -- ninguna de las dos existe en este target
    # freestanding (sin libstdc++/libsupc++). -fno-exceptions/-fno-rtti al
    # compilar (no solo al linkear) evita que el compilador genere esa
    # informacion en primer lugar. Es seguro para todo el codigo real: el
    # manejo de errores de AvaLang en este target no usa throw/try/catch
    # reales en ningun lado que se compile aca -- pasa por las macros
    # AVA_TRY/AVA_THROW/AVA_CATCH de ava_error.h, que con
    # CKM_CAP_STD_EXCEPTIONS=0 (ver binding.cmake) ya expanden a
    # setjmp/longjmp, no a throw/catch reales (confirmado: los unicos
    # throw/try/catch literales del codebase viven en frontend_antlr.cpp/
    # ast_builder.cpp/compiler.cpp, que ni se compilan aca -- este target
    # no tiene antlr4-runtime y usa frontend_stub.cpp en su lugar -- y en
    # bloques ya guardados con #if AVA_HAVE_EXCEPTIONS / #if
    # AVA_HAVE_STD_LIBRARY en vm.cpp/vm_task.cpp/vm_extern.cpp/c_api.cpp).
    #
    # -fno-threadsafe-statics: sin esto, el Meyer's singleton de
    # VmPlatformAccessor::Get() (unique_ptr local static) genera codigo de
    # inicializacion thread-safe que llama __cxa_guard_acquire/_release/
    # _abort -- otro trio de simbolos que este target no tiene. Correcto
    # de quitar: CKM_CAP_THREADS=0 (ver binding.cmake), no hay ejecucion
    # paralela real que necesite esa proteccion.
    #
    # -fno-builtin: ver el comentario grande en
    # platform/barekernel/BareKernelRuntimeSupport.cpp -- sin esto, GCC
    # puede reconocer el cuerpo de memcpy/memset/strlen definidos ahi como
    # instancias del propio patron que implementan y reescribirlos como
    # llamadas a si mismos (recursion infinita, solo en Release). Aplicada
    # a todo el target (no solo a ese archivo) porque el mismo problema en
    # general (el compilador generando una llamada implicita a
    # memcpy/memset/strlen/round/sqrt/pow/fmod que este toolchain sin libc
    # no provee) puede originarse en cualquier .cpp, no solo en ese.
    # -fPIC: SIN esto, el compilador nunca pide relocations en runtime
    # para direcciones de funcion guardadas como DATOS (no como "call"
    # relativo a PC). El caso real que esto rompia: las entradas de
    # .init_array (punteros a los constructores globales de C++) se
    # grababan como constantes absolutas calculadas en tiempo de link
    # asumiendo base 0 -- exactamente el mismo valor que su offset dentro
    # de la libreria -- y como nunca se generaba una relocation
    # R_386_RELATIVE para esas entradas, LibraryLoader::RunInitializers
    # (litekernel) las llamaba tal cual, saltando a esa direccion CHICA
    # (offset, no virtual final) en vez de a
    # lib->mapped_base + offset -- confirmado con el log: "Calling
    # constructor 0 at 0x156C0" (offset, no una direccion 0x600xxxxx),
    # seguido de un cuelgue total.
    #
    # Las llamadas a funcion normales (R_386_PC32, relativas a PC) y
    # dlsym (que lee st_value de la tabla de simbolos directo, sin pasar
    # por relocations) nunca mostraron este problema -- por eso el resto
    # del pipeline (dlopen, los 6916 REL, los dlsym de ava_vm_create/
    # ava_run/etc.) funcionaba perfecto y esto recien aparecia al llegar
    # al primer puntero-a-funcion guardado como dato.
    #
    # Este gap ya estaba anotado (sin resolver) en
    # cmake/toolchain-i686-elf.cmake, comentario sobre el fallback a
    # gcc -m32: "los .o estaticos no se compilan con -fPIC... revisar el
    # loader propio antes de asumir que es inocuo en otro kernel". Litekernel
    # ya fue revisado (RunInitializers asume relocation ya aplicada) --
    # esto SI era el problema real.
    target_compile_options(avalang PRIVATE
        -fno-exceptions
        -fno-rtti
        -fno-threadsafe-statics
        -fno-builtin
        -fPIC
    )
elseif(AVA_BUILD_SHARED)
    add_library(avalang SHARED ${PUBLIC_SOURCES})
else()
    add_library(avalang STATIC ${PUBLIC_SOURCES})
endif()

if(AVA_TARGET_BAREKERNEL)
    target_compile_definitions(avalang PRIVATE AVA_BAREKERNEL_TARGET_BINDING)
    include("${CMAKE_CURRENT_SOURCE_DIR}/platform/barekernel/bindings/target/binding.cmake")
endif()

target_include_directories(avalang
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/api/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/api/src
        ${CMAKE_CURRENT_SOURCE_DIR}/api/include
        ${GENERATED_DIR}
        ${CMAKE_SOURCE_DIR}/runtime/avaui/src
)

if(AVA_BUILD_UI)
    target_link_libraries(avalang PRIVATE avalang_ui)
endif()

if(AVA_HAVE_ANTLR)
    # El paquete de vcpkg "antlr4-runtime" NO expone un target llamado
    # antlr4-runtime; expone antlr4_static (triplets estaticos, p.ej.
    # x64-windows-static-md) o antlr4_shared (triplets dinamicos).
    if(TARGET antlr4_static)
        set(_ava_antlr_target antlr4_static)
    elseif(TARGET antlr4_shared)
        set(_ava_antlr_target antlr4_shared)
    else()
        # Debian/Ubuntu: no hay target de vcpkg, pero FindAntlr4Jar detecto
        # los headers y la lib via find_path/find_library y seteo
        # antlr4-runtime_FOUND a mano. Usamos esas variables directamente.
        set(_ava_antlr_target "")
        if(ANTLR4_RUNTIME_INCLUDE_DIR)
            target_include_directories(avalang PRIVATE ${ANTLR4_RUNTIME_INCLUDE_DIR})
        endif()
        if(ANTLR4_RUNTIME_LIBRARY)
            target_link_libraries(avalang PRIVATE ${ANTLR4_RUNTIME_LIBRARY})
        endif()
    endif()
    if(_ava_antlr_target)
        message(STATUS "AvaLang debug: usando target ANTLR = '${_ava_antlr_target}'")
        target_link_libraries(avalang PRIVATE ${_ava_antlr_target})
        if(_ava_antlr_target STREQUAL "antlr4_static")
            # Requerido para que antlr4-runtime.h no marque sus clases como
            # dllimport (lo cual rompe el link contra una lib estatica).
            target_compile_definitions(avalang PRIVATE ANTLR4CPP_STATIC)
        endif()
    else()
        message(STATUS "AvaLang debug: usando antlr4-runtime del sistema (Debian/Ubuntu)")
    endif()
    # Debian/Ubuntu: el config package define ANTLR4_INCLUDE_DIR apuntando
    # a /usr/include/antlr4-runtime (los headers viven en un subdirectorio).
    # El target antlr4_static del sistema NO propaga el include dir, asi que
    # lo añadimos a mano para que #include "antlr4-runtime.h" resuelva.
    if(ANTLR4_INCLUDE_DIR)
        target_include_directories(avalang PRIVATE ${ANTLR4_INCLUDE_DIR})
    endif()
    target_compile_definitions(avalang PRIVATE AVA_HAVE_ANTLR)
endif()

target_compile_definitions(avalang PRIVATE AVA_BUILDING_LIBRARY)

if(AVA_HAVE_LIBFFI)
    target_include_directories(avalang PRIVATE ${LIBFFI_INCLUDE_DIR})
    target_link_libraries(avalang PRIVATE ${LIBFFI_LIBRARY})
    target_compile_definitions(avalang PRIVATE AVA_HAVE_LIBFFI)
endif()
if(UNIX AND NOT APPLE)
    # dlopen/dlsym en glibc viejas requiere -ldl explicito (algunas
    # toolchains modernas ya lo integran en libc, pero enlazarlo no hace
    # daño y evita romper en distros mas viejas).
    target_link_libraries(avalang PRIVATE dl)
    # pthread: std::thread/std::mutex/std::condition_variable (usados por
    # LinThread/LinMutex/LinTimer y el async runtime del VM) envuelven
    # pthread en glibc; -lpthread es necesario en toolchains viejos y no
    # hace daño en modernos.
    target_link_libraries(avalang PRIVATE pthread)
    # clock_gettime/nanosleep (LinClock) vivian en librt en glibc vieja;
    # en modernas estan en libc pero enlazar rt no hace daño.
    find_library(RT_LIBRARY rt)
    if(RT_LIBRARY)
        target_link_libraries(avalang PRIVATE ${RT_LIBRARY})
    endif()
endif()

# NOTE: symbol visibility is intentionally left at the compiler default
# (not hidden) so internal headers (src/vm/*.h) remain linkable from
# tests/ and from ava_cli, which both use VM internals directly rather
# than going through the C API. External bindings should still only rely
# on public/include/avalang.h -- the internal headers are not a stable contract.
# (Fase 8 del plan de optimizacion respeta esto -- ver
# cmake/OptimizationOptions.cmake, que reduce simbolos via stripping del
# binario final en Release, no via -fvisibility=hidden.)

# Fase 8 -- Optimizacion: LTO + dead-code elimination + symbol stripping
# en Release (ver cmake/OptimizationOptions.cmake).
# Fase 8 -- Optimizacion: LTO + dead-code elimination + symbol stripping
# en Release (ver cmake/OptimizationOptions.cmake). Con AVA_TARGET_BAREKERNEL
# se omite LTO -- mismo motivo que en runtime/avabare/CMakeLists.txt: el
# cross-compiler i686-elf-g++ freestanding es un toolchain especial (ver
# docs/kernel/binding-status.md), no vale la pena arriesgar esa build ya
# de por si fragil con IPO sin poder probarlo en este entorno.
if(AVA_TARGET_BAREKERNEL)
    ava_apply_optimization_options(avalang NO_LTO)
else()
    ava_apply_optimization_options(avalang)
endif()

# No tests - use scripts to test
#
# NOTA: ava_cli ya NO se define aquí -- vive en runtime/avacli/ (ver
# root CMakeLists.txt: if(AVA_BUILD_CLI) add_subdirectory(runtime/avacli)).
# Este archivo sólo define la librería `avalang`.

install(TARGETS avalang
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# ---------------------------------------------------------------------
# BareKernel: link real del .so (ET_DYN) a partir del libavalang.a
# estatico de arriba.
#
# Por que un paso aparte y no add_library(SHARED): ver el comentario
# junto a add_library(avalang STATIC ...) mas arriba -- CMAKE_SYSTEM_NAME
# Generic no tiene reglas de shared library, asi que invocamos el
# linker a mano con las flags correctas.
#
# -Wl,--whole-archive / --no-whole-archive: sin esto, el linker solo
# saca de libavalang.a los .o que resuelven un simbolo ya pedido, y como
# nada externo "pide" por ejemplo ava_vm_create todavia, esa funcion
# (y el resto de la C API publica) quedaria afuera del .so. whole-archive
# fuerza a meter los 50 objetos completos.
#
# -Wl,-z,defs: hace que el link FALLE si queda algun simbolo sin
# resolver (UND), en vez de que litekernel lo descubra en runtime como
# una relocation resuelta a basura -- ver docs/kernel/binding-status.md
# y el analisis de LibraryLoader::ApplyRelocation (el loader del kernel
# resuelve simbolos SOLO contra el symtab del propio .so, nunca contra
# otras bibliotecas). Si este link falla con "undefined reference",
# esa es informacion real: algo todavia depende de simbolos que el
# kernel no va a poder proveer en carga.
#
# -Wl,-shared (NO "-shared" a secas) -nostdlib -m32: ELF32 compartido,
# sin arrastrar libc/libstdc++ del host ni del cross-compiler (fuera de
# -lgcc, que sí hace falta: trae helpers de soporte del compilador como
# los de aritmetica de 64 bits en 32-bit, no funciones de SO).
#
# Por que -Wl,-shared y no -shared: este toolchain i686-elf esta
# configurado con --without-headers (bare-metal, sin libc), y por eso
# no tiene las "specs" de gcc que le dicen que hacer con -shared -- gcc
# lo descarta en silencio de COLLECT_GCC_OPTIONS antes de pasarselo a
# collect2/ld (se puede confirmar con -v: no aparece en la linea final
# de collect2.exe). El resultado sin este fix es un ELF ET_EXEC, no
# ET_DYN, y el LibraryLoader del kernel (que espera ET_DYN) lo rechaza
# o lo carga mal. -Wl,-shared salta la logica de specs de gcc y llega
# directo al linker.
if(AVA_TARGET_BAREKERNEL)
    set(AVA_BAREKERNEL_SO "${CMAKE_BINARY_DIR}/libavalang.so")

    # -Wl,-N -Wl,-Ttext-segment=0x0: CRITICO para litekernel::LibraryLoader.
    #
    # Un `ld -shared` normal (sin estos flags) separa el .so en al menos
    # dos segmentos PT_LOAD (uno read+exec para .text/.rodata, otro
    # read+write para .data/.got/.init_array/.bss), cada uno alineado a
    # pagina (4096) EN MEMORIA pero NO en el archivo -- entre ambos queda
    # un salto constante (tipicamente 0x1000) entre sh_addr y sh_offset
    # para cualquier seccion del segundo segmento en adelante
    # (.init_array, .got, .got.plt, .dynamic, etc). Confirmado con
    # readelf -S sobre un .so de prueba: sin este flag, .init_array
    # queda en Addr=0x3f58 / Off=0x2f58 (gap de 0x1000); con el flag,
    # Addr=0x35c / Off=0x35c (identicos).
    #
    # LibraryLoader::ParseSections (litekernel, lib_loader.cpp) lee cada
    # seccion como `base_addr + sh_offset` (offset de ARCHIVO, valido
    # porque el .so entero se copia byte a byte), pero
    # LibraryLoader::ApplyRelocation escribe cada relocation en
    # `base_addr + r_offset` (r_offset es una DIRECCION, en la misma
    # base que sh_addr). El loader entero asume sh_addr == sh_offset
    # para toda seccion SHF_ALLOC -- valido solo si el .so es UN SOLO
    # segmento sin padding de alineacion entre archivo y memoria.
    #
    # Sin este flag, la relocation R_386_RELATIVE que corrige cada
    # entrada de .init_array (constructores globales de C++) se escribe
    # 4096 bytes mas adelante de donde realmente vive esa entrada en el
    # buffer copiado -- el slot real nunca se toca y conserva el valor
    # crudo del archivo (el offset link-time sin relocar, p.ej. 0x9740
    # en vez de 0x60009740). RunInitializers despues llama a esa
    # direccion cruda, que cae en memoria de kernel identity-mapped
    # (por eso el sintoma es un CUELGUE, no un page fault limpio: la
    # CPU termina ejecutando codigo de kernel arbitrario en vez de
    # faultear contra una pagina no mapeada).
    #
    # -Wl,-N (OMAGIC): no alinea segmentos a pagina, y hace todo el .so
    # RW+X en un unico PT_LOAD (ld avisa "LOAD segment with RWX
    # permissions" -- inofensivo aca, no hay modelo de W^X en este
    # kernel).
    # -Wl,-Ttext-segment=0x0: sin esto, -N igual mantiene el base
    # tradicional de ET_EXEC (0x08048000 en x86); con esto el .so sigue
    # siendo ET_DYN con direcciones relativas a 0, que es lo que
    # LibraryLoader espera (le suma su propia direccion virtual fija).
    if(AVA_TARGET_BAREKERNEL)
        set(AVA_BAREKERNEL_SO_LAYOUT_FLAGS -Wl,-N -Wl,-Ttext-segment=0x0)
    endif()

    add_custom_command(
        OUTPUT ${AVA_BAREKERNEL_SO}
        COMMAND ${CMAKE_CXX_COMPILER}
                -Wl,-shared -m32 -nostdlib -fno-exceptions -fno-rtti
                ${AVA_BAREKERNEL_SO_LAYOUT_FLAGS}
                -o ${AVA_BAREKERNEL_SO}
                -Wl,--whole-archive $<TARGET_FILE:avalang> -Wl,--no-whole-archive
                -Wl,-z,defs
                -lgcc
        DEPENDS avalang
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "BareKernel: linking libavalang.so (ET_DYN, layout plano sh_addr==sh_offset) from libavalang.a"
        VERBATIM
    )

    add_custom_target(avalang_barekernel_so ALL
        DEPENDS ${AVA_BAREKERNEL_SO}
    )

    install(FILES ${AVA_BAREKERNEL_SO} DESTINATION lib OPTIONAL)
endif()
install(FILES api/include/avalang.h DESTINATION include)
