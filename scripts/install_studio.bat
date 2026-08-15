@echo off
setlocal enabledelayedexpansion

REM Este script vive en scripts\ (ver AVALAND_STRUCT.md); nos
REM movemos a la raiz del repo (un nivel arriba) para que las rutas
REM relativas (CMakeLists.txt, third_party\, build\, vcpkg\) sigan
REM funcionando sin importar desde donde se invoque este .bat.
cd /d "%~dp0.."

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
REM   3. If VCPKG_ROOT is set (install.bat already ran), also installs
REM      curl via that same vcpkg -- needed by the ai_agent plugin
REM      (find_package(CURL REQUIRED) in its CMakeLists.txt) even if
REM      VCPKG_ROOT was set by an older install.bat run from before that
REM      plugin existed. Otherwise just reminds you (does not force) to
REM      run install.bat first -- that installs the ANTLR4 frontend AND
REM      curl via vcpkg, so scripts you Run (F5) inside Ava Studio
REM      actually get parsed instead of hitting the stub-frontend error,
REM      and the ai_agent plugin configures cleanly.
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

    REM --- 3b. libcurl for the ai_agent plugin ----------------------------
    REM Self-healing: si VCPKG_ROOT quedo seteado por una corrida vieja de
    REM install.bat (de antes de que el plugin ai_agent existiera), esa
    REM corrida nunca instalo curl -- no alcanza con pedirle al usuario
    REM que vuelva a correr install.bat, porque build_studio falla en el
    REM configure step ANTES de llegar a mostrar ese mensaje. Se instala
    REM aca tambien, directo, sin depender de que install.bat este al dia.
    if not exist "%VCPKG_ROOT%\vcpkg.exe" (
        echo [WARN] VCPKG_ROOT is set but vcpkg.exe was not found there.
        echo        Run install.bat to (re)install vcpkg, then re-run this script.
    ) else (
        if not defined AVA_VCPKG_TRIPLET set "AVA_VCPKG_TRIPLET=x64-windows-static-md"
        echo.
        echo Checking curl:%AVA_VCPKG_TRIPLET% via vcpkg ^(needed by ai_agent^) ...
        "%VCPKG_ROOT%\vcpkg.exe" install curl:%AVA_VCPKG_TRIPLET%
        if errorlevel 1 (
            echo [ERROR] "vcpkg install curl:%AVA_VCPKG_TRIPLET%" failed. See output above.
            exit /b 1
        )
    )
) else (
    echo [INFO] VCPKG_ROOT is not set. Ava Studio will still build and run,
    echo        but Run ^(F5^) inside it will hit the stub-frontend error
    echo        instead of actually parsing your script ^(see README.md^),
    echo        and the ai_agent plugin will fail to configure ^(it needs
    echo        curl from vcpkg too^).
    echo        For both, run install.bat once first, then come back and
    echo        run this script.
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
