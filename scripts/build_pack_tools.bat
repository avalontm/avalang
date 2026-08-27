@echo off
setlocal enabledelayedexpansion

REM Este script vive en scripts\ (ver AVALAND_STRUCT.md); nos movemos a la
REM raiz del repo (un nivel arriba) para que las rutas relativas sigan
REM funcionando sin importar desde donde se invoque este .bat.
cd /d "%~dp0.."

REM =====================================================================
REM Fase 9 (runtime/avapack/README.md) -- compila UNA VEZ las herramientas
REM que `ava_cli build` (target desktop) necesita para empacar proyectos
REM SIN el repo/CMake al lado: avapack_gen.exe + avapack_stub.exe.
REM
REM A diferencia de build_cli.bat/build_studio.bat (que compilan binarios
REM para USAR), esto es un paso de "preparar distribucion" -- se corre una
REM sola vez (o cuando cambia avapack/avalang), y el resultado
REM (avapack_gen.exe, avapack_stub.exe, avalang.dll, avalang_ui.dll) se
REM copia a mano junto a ava_cli.exe -- ahi es donde build_command.cpp los
REM busca (GetSelfExecutableDir(), ver runtime/avacli/src/build_command.cpp).
REM Sin este paso, `ava_cli build` sigue funcionando igual que antes
REM (recompila avapack desde fuente en build_pack\ via CMake) -- este script
REM es lo que habilita el camino rapido nuevo, no reemplaza al viejo.
REM
REM Uso:
REM   build_pack_tools.bat                build Release, copia a dist_pack_tools\
REM   build_pack_tools.bat clean           borra build_pack_tools\ y dist_pack_tools\
REM   build_pack_tools.bat ninja           usa Ninja en vez de Visual Studio/MSBuild
REM =====================================================================

set "BUILD_DIR=build_pack_tools"
set "DIST_DIR=dist_pack_tools"
set "BUILD_TYPE=Release"
set "USE_NINJA=0"
set "GENERATOR=Visual Studio 17 2022"

if /I "%~1"=="clean" (
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    if exist "%DIST_DIR%" rmdir /s /q "%DIST_DIR%"
    echo Done -- %BUILD_DIR%\ y %DIST_DIR%\ removidos.
    exit /b 0
)
if /I "%~1"=="ninja" set "USE_NINJA=1"

where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] cmake no esta en el PATH.
    exit /b 1
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

set "CMAKE_CONFIGURE_ARGS=-DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DAVA_BUILD_PACK=ON -DAVA_BUILD_CLI=OFF -DAVA_BUILD_STUDIO=OFF -DAVA_BUILD_AVAHOST=OFF"

if defined VCPKG_ROOT (
    echo Usando vcpkg toolchain: %VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
    set "CMAKE_CONFIGURE_ARGS=!CMAKE_CONFIGURE_ARGS! -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
) else (
    echo [INFO] VCPKG_ROOT no esta definida -- corre install.bat primero si
    echo        avalang necesita el frontend ANTLR real.
)

echo.
if "%USE_NINJA%"=="1" (
    echo Configurando con Ninja ^(%BUILD_TYPE%^) ...
    cmake -S . -B "%BUILD_DIR%" -G "Ninja" %CMAKE_CONFIGURE_ARGS%
) else (
    echo Configurando con "%GENERATOR%" ...
    cmake -S . -B "%BUILD_DIR%" -G "%GENERATOR%" -A x64 %CMAKE_CONFIGURE_ARGS%
)
if errorlevel 1 (
    echo [ERROR] cmake ^(configure^) fallo. Ver arriba.
    exit /b 1
)

echo.
echo Compilando avapack_gen + avapack_stub ^(%BUILD_TYPE%^) ...
cmake --build "%BUILD_DIR%" --target avapack_gen --target avapack_stub --config %BUILD_TYPE% --parallel
if errorlevel 1 (
    echo [ERROR] el build fallo. Ver arriba.
    exit /b 1
)

set "OUT_DIR=%BUILD_DIR%\runtime\avalang"
if "%USE_NINJA%"=="0" set "OUT_DIR=%BUILD_DIR%\runtime\avalang\%BUILD_TYPE%"

if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"
copy /y "%OUT_DIR%\avapack_gen.exe" "%DIST_DIR%\" >nul
copy /y "%OUT_DIR%\avapack_stub.exe" "%DIST_DIR%\" >nul
copy /y "%OUT_DIR%\avalang.dll" "%DIST_DIR%\" >nul 2>nul
copy /y "%OUT_DIR%\avalang_ui.dll" "%DIST_DIR%\" >nul 2>nul

echo.
echo Listo. Copia el contenido de %DIST_DIR%\ junto a ava_cli.exe para que
echo `ava_cli build --target desktop` use el camino sin-repo (Fase 9) en vez
echo de recompilar avapack desde fuente via CMake en cada build.
exit /b 0
