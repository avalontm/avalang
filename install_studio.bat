@echo off
setlocal enabledelayedexpansion

REM =====================================================================
REM Ava Studio install_studio.bat
REM
REM Installs/checks everything needed to configure and build ava_studio
REM (the ImGui-based IDE shell), then runs build_studio.bat.
REM
REM What it checks/does:
REM   1. git      -- required by CMake FetchContent to pull GLFW + Dear
REM                  ImGui (docking branch) the first time you configure
REM                  with AVA_BUILD_STUDIO=ON.
REM   2. cmake    -- required to configure/build at all.
REM   3. Reminds you (does not force) to run install.bat first if you
REM      haven't -- that installs the ANTLR4 frontend via vcpkg, so
REM      scripts you Run (F5) inside Ava Studio actually get parsed
REM      instead of hitting the stub-frontend error. Ava Studio builds
REM      and runs fine without it either way.
REM   4. Runs build_studio.bat (unless "skipbuild" is passed).
REM
REM Usage:
REM   install_studio.bat              check deps, then build
REM   install_studio.bat skipbuild    check deps only
REM =====================================================================

set "SKIP_BUILD=0"
for %%A in (%*) do (
    if /I "%%A"=="skipbuild" set "SKIP_BUILD=1"
)

echo =====================================================================
echo Ava Studio dependency check
echo =====================================================================
echo.

REM --- 1. git --------------------------------------------------------------
where git >nul 2>nul
if errorlevel 1 (
    echo [ERROR] git not found on PATH.
    echo         CMake needs it to fetch GLFW and Dear ImGui the first time
    echo         you configure with AVA_BUILD_STUDIO=ON. Install it from
    echo         https://git-scm.com/ and re-run.
    exit /b 1
)
echo [OK] git found

REM --- 2. cmake --------------------------------------------------------------
where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] cmake not found on PATH.
    echo         Install it from https://cmake.org/download/ and re-run.
    exit /b 1
)
echo [OK] cmake found

REM --- 3. Nudge toward install.bat for the real ANTLR4 frontend -----------
echo.
if defined VCPKG_ROOT (
    echo [OK] VCPKG_ROOT is set ^(%VCPKG_ROOT%^) -- looks like install.bat
    echo      already ran. Ava Studio will get the real AvaLang parser.
) else (
    echo [INFO] VCPKG_ROOT is not set. Ava Studio will still build and run,
    echo        but Run ^(F5^) inside it will hit the stub-frontend error
    echo        instead of actually parsing your script ^(see README.md^).
    echo        For the real parser, run install.bat once first, then
    echo        come back and run this script.
)

echo.
echo =====================================================================
echo Dependency check done.
echo =====================================================================

if "%SKIP_BUILD%"=="1" (
    echo.
    echo Skipping build ^(skipbuild passed^). Run build_studio.bat manually when ready.
    endlocal
    exit /b 0
)

echo.
echo Running build_studio.bat ...
call "%~dp0build_studio.bat"
set "BUILD_RESULT=%errorlevel%"
endlocal
exit /b %BUILD_RESULT%
