# ---------------------------------------------------------------------
# Fase B1 (plan_avapack_barekernel.md): main_barekernel.cpp + el .avb
# embebido (embedded_avb.cpp) -- reemplaza a ava_barekernel_runner
# (runtime/avabare/) para producir un app.exe autocontenido en vez de un
# bootstrap que lee /apps/hello.avb suelto del disco.
#
# Mismo patrón que runtime/avabare/CMakeLists.txt (ver comentario ahí
# sobre "Por qué es una librería STATIC y no un ejecutable"): el formato
# final es AppHeader, no ELF -- este target deja un .a con `_start` como
# símbolo exportado. A diferencia de avabare, acá SÍ hay una herramienta
# en este repo (ava_apphdr_writer, ver ../CMakeLists.txt) que sabe
# envolver eso en AppHeader -- falta la Fase B2 para automatizar
# objcopy -O binary + nm (extraer entry_offset) + invocar
# ava_apphdr_writer en un solo paso; por ahora es manual.
#
# EMBEDDED_AVB_CPP: por defecto usa el sample ya generado
# (samples/embedded_hello_avb.cpp, producido corriendo
# avapack_barekernel_gen de verdad contra
# platform/barekernel/samples/hello.avb -- no es un mock, ver ese
# archivo). Override con -DAVAPACK_BAREKERNEL_EMBEDDED_AVB_CPP=<ruta> una
# vez que exista un .avb real de un proyecto propio (regenerarlo con
# avapack_barekernel_gen --avb <tu.avb> --out <ruta>).
set(AVAPACK_BAREKERNEL_EMBEDDED_AVB_CPP
    "${CMAKE_CURRENT_SOURCE_DIR}/samples/embedded_hello_avb.cpp" CACHE FILEPATH
    "embedded_avb.cpp a compilar junto con main_barekernel.cpp (generado por avapack_barekernel_gen)")

add_library(avapack_barekernel_app STATIC
    main_barekernel.cpp
    "${AVAPACK_BAREKERNEL_EMBEDDED_AVB_CPP}"
)

target_include_directories(avapack_barekernel_app PRIVATE
    ${CMAKE_SOURCE_DIR}/runtime/avalang/api/include
    ${CMAKE_SOURCE_DIR}/runtime/avalang/platform/barekernel/bindings/target
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(avapack_barekernel_app PRIVATE
    -ffreestanding
    -fno-exceptions
    -fno-rtti
    -nostdlib
)

# Mismas capability flags que runtime/avabare/CMakeLists.txt -- este
# componente corre en el mismo binding (target/) y hereda las mismas
# limitaciones (CKM_CAP_LIBSTDCPP=0 sigue siendo el bloqueador crítico de
# kernel.md §6.1, no algo que este target resuelva).
target_compile_definitions(avapack_barekernel_app PRIVATE
    AVA_TARGET_BAREKERNEL
    AVA_BAREKERNEL_TARGET_BINDING
    CKM_CAP_DYNAMIC_LOADING=1
    CKM_CAP_THREADS=0
    CKM_CAP_MUTEX=0
    CKM_CAP_ENVVARS=1
    CKM_CAP_DIR_ENUM=1
    CKM_CAP_COLOR=1
    CKM_CAP_PROCESS_EXEC=0
    CKM_CAP_TIMERS=0
    CKM_CAP_STD_EXCEPTIONS=0
    CKM_CAP_LIBSTDCPP=0
)

# Ver comentario equivalente en runtime/avabare/CMakeLists.txt: solo
# dead-code elimination + stripping, sin LTO -- mismo motivo (toolchain
# i686-elf freestanding, no disponible para probar en este entorno, ver
# docs/kernel/binding-status.md).
ava_apply_optimization_options(avapack_barekernel_app NO_LTO)
