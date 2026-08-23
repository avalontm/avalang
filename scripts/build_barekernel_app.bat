@echo off
setlocal enabledelayedexpansion

REM Este script vive en scripts\ (ver AVALAND_STRUCT.md); nos
REM movemos a la raiz del repo (un nivel arriba) para que las rutas
REM relativas (CMakeLists.txt, build_cli\, build_pack_barekernel\, etc.)
REM sigan funcionando sin importar desde donde se invoque este .bat.
cd /d "%~dp0.."

REM =====================================================================
REM build_barekernel_app.bat -- wrapper de conveniencia para el nuevo
REM subcomando `ava_cli build --target barekernel` (Fase B2 de
REM plan_avapack_barekernel.md).
REM
REM A diferencia de build_barekernel.bat (que compila avalang/
REM ava_barekernel_runner cruzado a mano, sin pasar por ava_cli), este
REM script:
REM   1. Compila ava_cli.exe (build normal de HOST -- mismo target que
REM      build_cli.bat, en build_cli\ para no duplicar compilaciones si
REM      ya lo corriste antes).
REM   2. Corre ava_cli.exe build --target barekernel --project <dir>
REM      --entry <archivo.ava> --toolchain-dir <toolchain> --out <salida>,
REM      que a su vez:
REM        - compila+serializa tu .ava a .avb (usa avalang.dll, ya
REM          linkeado en el propio ava_cli.exe -- necesita VCPKG_ROOT con
REM          ANTLR4 real para parsear de verdad, ver install.bat; sin eso
REM          cae al frontend stub y este paso va a fallar con un error
REM          claro, no un mock).
REM        - compila avapack_barekernel_gen/ava_apphdr_writer como
REM          binarios de host (build_pack_barekernel\, separado de
REM          build_cli\).
REM        - configura+compila CRUZADO contra --toolchain-dir
REM          (build_barekernel_pack\, separado de build_barekernel\ que
REM          usa el otro script).
REM        - linkea con i686-elf-ld -T app.ld, corre objcopy y nm, y
REM          finalmente ava_apphdr_writer para envolver el AppHeader.
REM
REM Requiere el toolchain i686-elf real (i686-elf-gcc/g++/ld/objcopy/nm)
REM bajo --toolchain-dir -- a diferencia de build_barekernel.bat, este
REM subcomando llama a ld/objcopy/nm DIRECTAMENTE (no via el fallback
REM gcc -m32 del .cmake), asi que el fallback NO aplica para esos tres.
REM
REM Uso:
REM   build_barekernel_app.bat <toolchain_dir> <project_dir> <entry.ava> <out.exe>
REM   build_barekernel_app.bat <toolchain_dir> <project_dir> <entry.ava> <out.exe> clean
REM
REM Ejemplo:
REM   build_barekernel_app.bat D:\litekernel\tools\x86 samples\test main.ava out\hello.exe
REM
REM "clean" (ultimo argumento, opcional) borra build_cli\,
REM build_pack_barekernel\ y build_barekernel_pack\ antes de compilar --
REM util si veniste de una build vieja y sospechas de un estado corrupto.
REM =====================================================================

set "TOOLCHAIN_DIR=%~1"
set "PROJECT_DIR=%~2"
set "ENTRY_FILE=%~3"
set "OUT_PATH=%~4"
set "CLEAN=0"
if /I "%~5"=="clean" set "CLEAN=1"

if "%TOOLCHAIN_DIR%"=="" goto show_help
if "%PROJECT_DIR%"=="" goto show_help
if "%ENTRY_FILE%"=="" goto show_help
if "%OUT_PATH%"=="" goto show_help

where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] cmake was not found on PATH.
    echo         Install it from https://cmake.org/download/ and re-run.
    exit /b 1
)

if not exist "%TOOLCHAIN_DIR%" (
    echo [ERROR] --toolchain-dir no existe: %TOOLCHAIN_DIR%
    echo         Necesitas i686-elf-gcc/g++/ld/objcopy/nm reales bajo esa
    echo         carpeta -- a diferencia de build_barekernel.bat, este
    echo         subcomando llama a ld/objcopy/nm directo, sin fallback a
    echo         gcc -m32 del host para esos tres.
    exit /b 1
)

if "%CLEAN%"=="1" (
    for %%D in (build_cli build_pack_barekernel build_barekernel_pack) do (
        if exist "%%D" (
            echo Cleaning %%D ...
            rmdir /s /q "%%D"
        )
    )
)

REM ---- Paso 1: compilar ava_cli.exe (build de host, mismo patron que
REM build_cli.bat, misma carpeta build_cli\ para reusar el build si ya
REM corriste ese script antes) ----
set "BUILD_DIR=build_cli"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

set "CMAKE_CONFIGURE_ARGS=-DCMAKE_BUILD_TYPE=Release -DAVA_BUILD_CLI=ON"
if defined VCPKG_ROOT (
    echo Using vcpkg toolchain: %VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
    set "CMAKE_CONFIGURE_ARGS=!CMAKE_CONFIGURE_ARGS! -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
    if defined AVA_VCPKG_TRIPLET (
        set "CMAKE_CONFIGURE_ARGS=!CMAKE_CONFIGURE_ARGS! -DVCPKG_TARGET_TRIPLET=%AVA_VCPKG_TRIPLET%"
    )
) else (
    echo [WARNING] VCPKG_ROOT no esta definida. ava_cli.exe compila igual,
    echo           pero el paso de "compilar tu .ava a .avb" va a fallar
    echo           con el error del frontend stub ^(ver install.bat^) en
    echo           vez de parsear de verdad. Corre install.bat una vez si
    echo           no lo hiciste todavia.
)

echo.
echo Configuring ava_cli ^(Release^) ...
if exist "%BUILD_DIR%\CMakeCache.txt" (
    echo Reusing existing %BUILD_DIR%\ ^(generator locked to whatever build_cli.bat used there^) ...
    cmake -S . -B "%BUILD_DIR%" %CMAKE_CONFIGURE_ARGS%
) else (
    echo No existing cache in %BUILD_DIR%\ -- configuring with Visual Studio 17 2022 ^(same default as build_cli.bat^) ...
    cmake -S . -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 %CMAKE_CONFIGURE_ARGS%
)
if errorlevel 1 (
    echo [ERROR] CMake configure step failed. See output above.
    echo         Si el error es "Does not match the generator used
    echo         previously", borra %BUILD_DIR%\ ^(o corre este script con
    echo         "clean" como ultimo argumento^) y volve a correr.
    exit /b 1
)

echo.
echo Building ava_cli ...
cmake --build "%BUILD_DIR%" --config Release --target ava_cli --parallel
if errorlevel 1 (
    echo [ERROR] ava_cli build failed. See output above.
    exit /b 1
)

set "AVA_CLI_EXE=%BUILD_DIR%\runtime\avalang\Release\ava_cli.exe"
if not exist "%AVA_CLI_EXE%" set "AVA_CLI_EXE=%BUILD_DIR%\runtime\avalang\ava_cli.exe"
if not exist "%AVA_CLI_EXE%" (
    echo [ERROR] No se encontro ava_cli.exe tras el build en %BUILD_DIR%\runtime\avalang\
    exit /b 1
)

REM ---- Paso 2: correr el subcomando barekernel de verdad ----
echo.
echo =====================================================================
echo Running: %AVA_CLI_EXE% build --project %PROJECT_DIR% --entry %ENTRY_FILE% --out %OUT_PATH% --target barekernel --toolchain-dir %TOOLCHAIN_DIR%
echo =====================================================================
echo.
"%AVA_CLI_EXE%" build --project "%PROJECT_DIR%" --entry "%ENTRY_FILE%" --out "%OUT_PATH%" --target barekernel --toolchain-dir "%TOOLCHAIN_DIR%"
if errorlevel 1 (
    echo.
    echo [ERROR] ava_cli build --target barekernel fallo. El mensaje real de
    echo         arriba dice en que paso -- compilar/serializar el .ava,
    echo         compilar las herramientas de host, el build cruzado, el
    echo         link con i686-elf-ld, objcopy, nm, o ava_apphdr_writer.
    echo         Ninguno de esos pasos es un mock, asi que el error de
    echo         arriba es sobre la herramienta real que fallo.
    exit /b 1
)

echo.
echo =====================================================================
echo Listo -- ^.exe barekernel en: %OUT_PATH%
echo =====================================================================

endlocal
exit /b 0

:show_help
echo Uso: build_barekernel_app.bat ^<toolchain_dir^> ^<project_dir^> ^<entry.ava^> ^<out.exe^> [clean]
echo   toolchain_dir  carpeta con i686-elf-gcc/g++/ld/objcopy/nm reales
echo                  ^(ej. D:\litekernel\tools\x86^)
echo   project_dir    raiz del proyecto AvaLang a empaquetar
echo   entry.ava      archivo de entrada, relativo a project_dir
echo   out.exe        ruta del .exe final ^(o carpeta terminada en \^)
echo   clean          ^(opcional, ultimo argumento^) borra build_cli\,
echo                  build_pack_barekernel\ y build_barekernel_pack\
echo                  antes de compilar
echo.
echo Ejemplo:
echo   build_barekernel_app.bat D:\litekernel\tools\x86 samples\test main.ava out\hello.exe
exit /b 0
