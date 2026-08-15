@echo off
setlocal enabledelayedexpansion

REM Este script vive en scripts\ (ver AVALAND_STRUCT.md); nos
REM movemos a la raiz del repo (un nivel arriba) para que las rutas
REM relativas (CMakeLists.txt, third_party\, build\, vcpkg\) sigan
REM funcionando sin importar desde donde se invoque este .bat.
cd /d "%~dp0.."

REM =====================================================================
REM AvaUI build script (Windows)
REM
REM Configures + builds the whole project with AVA_BUILD_UI=ON, so you
REM get avalang.dll, ava_cli AND avalang_ui.dll (Phase 11: Native Backend,
REM Windows -- see docs/AVALANG_UI_PROGRESS.md). Uses its own build
REM directory (build_avaui\) so it never touches build\, build_studio\ or
REM build_avahost\.
REM
REM Usage:
REM   build_avaui.bat                 build Release with the default generator
REM   build_avaui.bat debug           build Debug instead
REM   build_avaui.bat clean           delete build_avaui\ and exit
REM   build_avaui.bat ninja           use Ninja instead of Visual Studio/MSBuild
REM                                   (requires ninja.exe on PATH)
REM
REM Flags can be combined, e.g.:  build_avaui.bat clean debug ninja
REM "clean" alone (no other flag) just wipes build_avaui\ and stops there --
REM handy when you only want to reclaim disk space or force a from-scratch
REM CMake reconfigure later. Combine it with another flag (debug/ninja) to
REM wipe build_avaui\ first and then continue into a normal build, same as
REM before.
REM
REM No "run" flag: avalang_ui.dll is a library, not an executable -- there
REM is nothing to launch yet. avahost/ava_studio wiring against the
REM native backend is future work, not part of Phase 11.
REM
REM ui/include/avalang/ui/scene/ISceneNode.h (Phase 7) needs glm. If
REM VCPKG_ROOT is set, its toolchain is picked up automatically same as
REM build.bat/build_studio.bat, AND this script installs glm via vcpkg
REM itself (install.bat only installs antlr4, not glm). Without VCPKG_ROOT,
REM glm must already be on your include path some other way or the Scene
REM Graph sources won't compile.
REM =====================================================================

set "BUILD_DIR=build_avaui"
set "BUILD_TYPE=Release"
set "CLEAN=0"
set "GENERATOR=Visual Studio 17 2022"
set "USE_NINJA=0"
set "OTHER_FLAG=0"

:parse_args
if "%~1"=="" goto args_done
if /I "%~1"=="clean"  set "CLEAN=1"
if /I "%~1"=="debug"  (set "BUILD_TYPE=Debug" & set "OTHER_FLAG=1")
if /I "%~1"=="ninja"  (set "USE_NINJA=1" & set "OTHER_FLAG=1")
if /I "%~1"=="help"   goto show_help
if /I "%~1"=="/?"     goto show_help
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

if "%CLEAN%"=="1" (
    if exist "%BUILD_DIR%" (
        echo Cleaning %BUILD_DIR% ...
        rmdir /s /q "%BUILD_DIR%"
    )
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

set "CMAKE_CONFIGURE_ARGS=-DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DAVA_BUILD_UI=ON"

if defined VCPKG_ROOT (
    echo Using vcpkg toolchain: %VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
    set "CMAKE_CONFIGURE_ARGS=!CMAKE_CONFIGURE_ARGS! -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
    if defined AVA_VCPKG_TRIPLET (
        echo Using vcpkg triplet: %AVA_VCPKG_TRIPLET%
        set "CMAKE_CONFIGURE_ARGS=!CMAKE_CONFIGURE_ARGS! -DVCPKG_TARGET_TRIPLET=%AVA_VCPKG_TRIPLET%"
        set "GLM_TRIPLET_ARG=:%AVA_VCPKG_TRIPLET%"
    ) else (
        set "GLM_TRIPLET_ARG="
    )
    REM install.bat only vcpkg-installs antlr4, so glm -- needed by the
    REM Phase 7 Scene Graph headers -- is never there unless we grab it too.
    REM "vcpkg install" is idempotent -- it's a fast no-op if glm is
    REM already present, so it's safe to call on every run.
    echo Installing glm via vcpkg ^(no-op if already installed^) ...
    "%VCPKG_ROOT%\vcpkg.exe" install glm!GLM_TRIPLET_ARG!
    if errorlevel 1 (
        echo [ERROR] "vcpkg install glm!GLM_TRIPLET_ARG!" failed. See output above.
        exit /b 1
    )
) else (
    echo [INFO] VCPKG_ROOT is not set. avalang_ui.dll still builds fine for
    echo        Phases 1-6/8-11, but Phase 7 ^(Scene Graph^) needs glm on the
    echo        include path. Run install.bat once to set up vcpkg ^(this
    echo        script then installs glm itself on top of that^), or set
    echo        VCPKG_ROOT by hand if you already have vcpkg elsewhere.
)

echo.
if "%USE_NINJA%"=="1" (
    where ninja >nul 2>nul
    if errorlevel 1 (
        echo [ERROR] ninja.exe not found on PATH but "ninja" was requested.
        exit /b 1
    )
    echo Configuring with Ninja ^(%BUILD_TYPE%^) ...
    cmake -S . -B "%BUILD_DIR%" -G "Ninja" %CMAKE_CONFIGURE_ARGS%
) else (
    echo Configuring with "%GENERATOR%" ...
    cmake -S . -B "%BUILD_DIR%" -G "%GENERATOR%" -A x64 %CMAKE_CONFIGURE_ARGS%
)

if errorlevel 1 (
    echo [ERROR] CMake configure step failed. See output above.
    exit /b 1
)

echo.
echo Building ^(%BUILD_TYPE%^) ...
cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --target avalang_ui --parallel
if errorlevel 1 (
    echo [ERROR] Build failed. See output above.
    exit /b 1
)

REM Multi-config generators (Visual Studio) put the dll under
REM <build>\ui\<Config>\; single-config ones (Ninja) put it under
REM <build>\ui\ directly.
set "AVAUI_DLL=%BUILD_DIR%\ui\%BUILD_TYPE%\avalang_ui.dll"
if not exist "%AVAUI_DLL%" set "AVAUI_DLL=%BUILD_DIR%\ui\avalang_ui.dll"

echo.
echo =====================================================================
echo Build succeeded.
echo avalang_ui.dll: %AVAUI_DLL%
echo =====================================================================

endlocal
exit /b 0

:show_help
echo Usage: build_avaui.bat [clean] [debug] [ninja]
echo   clean   alone: delete build_avaui\ and exit, nothing else
echo           combined with debug/ninja: wipe build_avaui\ first, then build
echo   debug   build Debug instead of Release
echo   ninja   use the Ninja generator instead of Visual Studio/MSBuild
exit /b 0
