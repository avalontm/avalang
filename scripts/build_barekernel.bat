@echo off
setlocal enabledelayedexpansion

REM Este script vive en scripts\ (ver AVALAND_STRUCT.md); nos
REM movemos a la raiz del repo (un nivel arriba) para que las rutas
REM relativas (CMakeLists.txt, cmake\, build_barekernel\) sigan
REM funcionando sin importar desde donde se invoque este .bat.
cd /d "%~dp0.."

REM =====================================================================
REM avalang -- build cruzado contra el toolchain i686-elf (BareKernel)
REM
REM Compila la libreria core avalang + ava_barekernel_runner (el _start()
REM bootstrap de Fase 7, ver runtime/avabare/) con AVA_TARGET_BAREKERNEL=ON
REM contra el cross-compiler en <toolchain>\ (D:\litekernel\tools\x86 por
REM defecto). ava_cli, avaui, ava_studio, avahost y avapack se desactivan a
REM proposito (AVA_BUILD_CLI/UI/STUDIO/AVAHOST/PACK=OFF): ninguno de esos
REM apunta a compilar para un target freestanding todavia.
REM
REM Usa su propia carpeta de build (build_barekernel\) para no tocar
REM build\, build_cli\, build_avahost\ ni build_studio\.
REM
REM Requiere Ninja en el PATH -- el generador de Visual Studio no sabe
REM invocar un cross-compiler GCC, asi que no es opcional aqui (a
REM diferencia de build_cli.bat).
REM
REM Uso:
REM   build_barekernel.bat                  build Release, toolchain en
REM                                          D:\litekernel\tools\x86
REM   build_barekernel.bat <ruta_toolchain>  usa esa carpeta en vez de la
REM                                          default (debe contener
REM                                          i686-elf-gcc.exe / -g++.exe
REM                                          en algun subdirectorio)
REM   build_barekernel.bat debug            build Debug en vez de Release
REM   build_barekernel.bat clean            borra build_barekernel\ y termina
REM
REM Los flags se pueden combinar, ej.:  build_barekernel.bat D:\otra\ruta debug
REM
REM Nota honesta (ver docs\kernel\binding-status.md): este script te lleva
REM hasta invocar el compilador/linker real. Si tu toolchain no trae
REM libstdc++ portado con soporte de excepciones, el build fallara en el
REM link con "undefined reference" -- eso es informacion real sobre el
REM obstaculo critico documentado en docs\kernel\kernel.md §6.1, no un bug
REM de este script.
REM =====================================================================

set "BUILD_DIR=build_barekernel"
set "BUILD_TYPE=Release"
set "CLEAN=0"
set "TOOLCHAIN_DIR=D:\litekernel\tools\x86"
set "OTHER_FLAG=0"

:parse_args
if "%~1"=="" goto args_done
if /I "%~1"=="clean" (set "CLEAN=1" & goto next_arg)
if /I "%~1"=="debug" (set "BUILD_TYPE=Debug" & set "OTHER_FLAG=1" & goto next_arg)
if /I "%~1"=="help"  goto show_help
if /I "%~1"=="/?"    goto show_help
REM cualquier otro argumento se toma como ruta al toolchain
set "TOOLCHAIN_DIR=%~1"
set "OTHER_FLAG=1"
:next_arg
shift
goto parse_args
:args_done

if "%CLEAN%"=="1" if "%OTHER_FLAG%"=="0" (
    if exist "%BUILD_DIR%" (
        echo Cleaning %BUILD_DIR% ...
        rmdir /s /q "%BUILD_DIR%"
        echo Done -- %BUILD_DIR%\ removed.
    ) else (
        echo Nothing to clean -- %BUILD_DIR%\ doesn't exist.
    )
    endlocal
    exit /b 0
)

where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] cmake was not found on PATH.
    echo         Install it from https://cmake.org/download/ and re-run.
    exit /b 1
)

where ninja >nul 2>nul
if errorlevel 1 (
    echo [ERROR] ninja.exe was not found on PATH.
    echo         The i686-elf cross-compiler can't be driven by the Visual
    echo         Studio generator. Install Ninja ^(https://ninja-build.org/,
    echo         or "pip install ninja", or via your package manager^) and
    echo         re-run.
    exit /b 1
)

if not exist "%TOOLCHAIN_DIR%" (
    echo [ERROR] Toolchain folder not found: %TOOLCHAIN_DIR%
    echo         Pass the real path as the first argument, e.g.:
    echo           build_barekernel.bat D:\ruta\real\tools\x86
    exit /b 1
)

if "%CLEAN%"=="1" (
    if exist "%BUILD_DIR%" (
        echo Cleaning %BUILD_DIR% ...
        rmdir /s /q "%BUILD_DIR%"
    )
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo.
echo Toolchain dir: %TOOLCHAIN_DIR%
echo Build type:    %BUILD_TYPE%
echo.
echo Configuring with Ninja ^(%BUILD_TYPE%^) ...
cmake -S . -B "%BUILD_DIR%" -G "Ninja" ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DCMAKE_TOOLCHAIN_FILE=cmake\toolchain-i686-elf.cmake ^
    -DAVA_I686_ELF_TOOLCHAIN_DIR="%TOOLCHAIN_DIR%" ^
    -DAVA_TARGET_BAREKERNEL=ON ^
    -DAVA_BUILD_CLI=OFF ^
    -DAVA_BUILD_UI=OFF ^
    -DAVA_BUILD_STUDIO=OFF ^
    -DAVA_BUILD_AVAHOST=OFF ^
    -DAVA_BUILD_PACK=OFF

if errorlevel 1 (
    echo [ERROR] CMake configure step failed. See output above.
    echo         If this is about a missing i686-elf-gcc/g++, check
    echo         TOOLCHAIN_DIR and cmake\toolchain-i686-elf.cmake.
    exit /b 1
)

echo.
echo Building ^(%BUILD_TYPE%^) -- targets avalang + ava_barekernel_runner ...
cmake --build "%BUILD_DIR%" --target avalang ava_barekernel_runner --parallel

if errorlevel 1 (
    echo.
    echo [ERROR] Build failed. See output above.
    echo         An "undefined reference" wall of errors at the link step
    echo         usually means this toolchain's libstdc++ isn't fully
    echo         portable to the kernel yet -- see docs\kernel\kernel.md
    echo         section 6.1 and docs\kernel\binding-status.md. That is a
    echo         real, expected-possible outcome, not necessarily a
    echo         mistake in this script.
    exit /b 1
)

echo.
echo =====================================================================
echo Build succeeded.
echo Look for the resulting avalang binary under %BUILD_DIR%\runtime\avalang\
echo and ava_barekernel_runner.a under %BUILD_DIR%\runtime\avabare\
echo =====================================================================

endlocal
exit /b 0

:show_help
echo Usage: build_barekernel.bat [ruta_toolchain] [clean] [debug]
echo   ruta_toolchain   carpeta que contiene i686-elf-gcc.exe / i686-elf-g++.exe
echo                    en algun subdirectorio (default: D:\litekernel\tools\x86)
echo   clean            solo: borra build_barekernel\ y termina
echo                    combinado con debug/ruta: limpia antes de compilar
echo   debug            build Debug en vez de Release
echo.
echo Requiere Ninja en el PATH (el generador de Visual Studio no puede
echo invocar el cross-compiler GCC).
exit /b 0
