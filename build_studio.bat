@echo off
setlocal enabledelayedexpansion

REM =====================================================================
REM Ava Studio build script (Windows)
REM
REM Configures + builds the whole project with AVA_BUILD_STUDIO=ON, so
REM you get avalang.dll, ava_cli AND ava_studio.exe. Uses its own build
REM directory (build_studio\) so it never touches or reconfigures your
REM regular build\ from build.bat.
REM
REM Usage:
REM   build_studio.bat                build Release with the default generator
REM   build_studio.bat debug           build Debug instead
REM   build_studio.bat clean            wipe build_studio\ first
REM   build_studio.bat ninja            use Ninja instead of Visual Studio/MSBuild
REM                                     (requires ninja.exe on PATH)
REM   build_studio.bat run              after a successful build, launch ava_studio.exe
REM
REM Flags can be combined, e.g.:  build_studio.bat clean debug run
REM
REM The first time you configure with AVA_BUILD_STUDIO=ON, CMake fetches
REM GLFW and Dear ImGui (docking branch) via FetchContent -- needs
REM internet and git on PATH that one time, then they're cached in
REM build_studio\_deps\.
REM
REM If VCPKG_ROOT is set (i.e. you already ran install.bat), its
REM toolchain is picked up automatically so the real ANTLR4 frontend
REM builds too -- otherwise ava_studio still builds and runs fine, it
REM just falls back to the stub frontend when you press Run (F5) on a
REM script (see README.md).
REM =====================================================================

set "BUILD_DIR=build_studio"
set "BUILD_TYPE=Release"
set "CLEAN=0"
set "GENERATOR=Visual Studio 17 2022"
set "USE_NINJA=0"
set "RUN_AFTER=0"

:parse_args
if "%~1"=="" goto args_done
if /I "%~1"=="clean"  set "CLEAN=1"
if /I "%~1"=="debug"  set "BUILD_TYPE=Debug"
if /I "%~1"=="ninja"  set "USE_NINJA=1"
if /I "%~1"=="run"    set "RUN_AFTER=1"
if /I "%~1"=="help"   goto show_help
if /I "%~1"=="/?"     goto show_help
shift
goto parse_args
:args_done

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

set "CMAKE_CONFIGURE_ARGS=-DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DAVA_BUILD_STUDIO=ON"

if defined VCPKG_ROOT (
    echo Using vcpkg toolchain: %VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
    REM NOTE: inside a parenthesized block, %VAR% expands ONCE at block
    REM entry, not after each "set". Must use !VAR! (delayed expansion,
    REM already enabled via setlocal above) so each append actually sees
    REM the previous one's result.
    set "CMAKE_CONFIGURE_ARGS=!CMAKE_CONFIGURE_ARGS! -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
    if defined AVA_VCPKG_TRIPLET (
        echo Using vcpkg triplet: %AVA_VCPKG_TRIPLET%
        set "CMAKE_CONFIGURE_ARGS=!CMAKE_CONFIGURE_ARGS! -DVCPKG_TARGET_TRIPLET=%AVA_VCPKG_TRIPLET%"
    )
) else (
    echo [INFO] VCPKG_ROOT is not set. ava_studio.exe still builds fine, but
    echo        Run ^(F5^) inside it will hit the stub-frontend error instead
    echo        of really parsing your script. Run install.bat once to fix
    echo        that, or just keep using the demo Preview tree for now.
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
cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --target ava_studio --parallel
if errorlevel 1 (
    echo [ERROR] Build failed. See output above.
    exit /b 1
)

REM Multi-config generators (Visual Studio) put the exe under
REM <build>\studio\<Config>\; single-config ones (Ninja) put it under
REM <build>\studio\ directly.
set "STUDIO_EXE=%BUILD_DIR%\studio\%BUILD_TYPE%\ava_studio.exe"
if not exist "%STUDIO_EXE%" set "STUDIO_EXE=%BUILD_DIR%\studio\ava_studio.exe"

echo.
echo =====================================================================
echo Build succeeded.
echo ava_studio.exe: %STUDIO_EXE%
echo =====================================================================

if "%RUN_AFTER%"=="1" (
    if exist "%STUDIO_EXE%" (
        echo.
        echo Launching ava_studio.exe ...
        REM Run from the repo root (not build_studio\) so the Explorer
        REM panel's default "scripts" path (relative to the working
        REM directory) resolves to .\scripts\ like ava_cli's examples.
        pushd "%~dp0"
        start "" "%STUDIO_EXE%"
        popd
    ) else (
        echo [WARN] Expected ava_studio.exe at %STUDIO_EXE% but it's not there.
    )
)

endlocal
exit /b 0

:show_help
echo Usage: build_studio.bat [clean] [debug] [ninja] [run]
echo   clean   wipe build_studio\ before configuring
echo   debug   build Debug instead of Release
echo   ninja   use the Ninja generator instead of Visual Studio/MSBuild
echo   run     launch ava_studio.exe after a successful build
exit /b 0
