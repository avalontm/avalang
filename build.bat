@echo off
setlocal enabledelayedexpansion

REM =====================================================================
REM AvaLang build script (Windows)
REM
REM Usage:
REM   build.bat                build Release with the default generator
REM   build.bat debug           build Debug instead

REM   build.bat clean            wipe the build directory first
REM   build.bat ninja            use Ninja instead of Visual Studio/MSBuild
REM                              (requires ninja.exe on PATH)
REM
REM Flags can be combined, e.g.:  build.bat clean debug
REM
REM If the VCPKG_ROOT environment variable is set, its toolchain file is
REM passed to CMake automatically so an antlr4-runtime installed via
REM vcpkg ("vcpkg install antlr4") is picked up without extra flags.
REM =====================================================================

set "BUILD_DIR=build"
set "BUILD_TYPE=Release"
set "CLEAN=0"
set "GENERATOR=Visual Studio 17 2022"
set "USE_NINJA=0"

:parse_args
if "%~1"=="" goto args_done
if /I "%~1"=="clean"  set "CLEAN=1"
if /I "%~1"=="debug"  set "BUILD_TYPE=Debug"
if /I "%~1"=="ninja"  set "USE_NINJA=1"
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

set "CMAKE_CONFIGURE_ARGS=-DCMAKE_BUILD_TYPE=%BUILD_TYPE%"

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
    echo [INFO] VCPKG_ROOT is not set. If antlr4-runtime is not otherwise
    echo        discoverable, the build falls back to the stub frontend
    echo        automatically ^(see README.md^). To enable the full
    echo        frontend: install vcpkg, run "vcpkg install antlr4",
    echo        then set VCPKG_ROOT and re-run this script.
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
cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --parallel
if errorlevel 1 (
    echo [ERROR] Build failed. See output above.
    exit /b 1
)

echo.
echo =====================================================================
echo Build succeeded.
if "%USE_NINJA%"=="1" (
    echo Binaries are in %BUILD_DIR%\
) else (
    echo Binaries are in %BUILD_DIR%\%BUILD_TYPE%\
)
echo =====================================================================

endlocal
exit /b 0

:show_help
echo Usage: build.bat [clean] [debug] [ninja]
echo   clean   wipe the build directory before configuring
echo   debug   build Debug instead of Release
echo   ninja   use the Ninja generator instead of Visual Studio/MSBuild
exit /b 0
