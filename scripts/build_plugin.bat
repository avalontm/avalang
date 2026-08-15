@echo off
setlocal enabledelayedexpansion

REM Este script vive en scripts\ (ver AVALAND_STRUCT.md); nos
REM movemos a la raiz del repo (un nivel arriba) para que las rutas
REM relativas (build_studio\, etc.) sigan funcionando sin importar
REM desde donde se invoque este .bat.
cd /d "%~dp0.."

REM =====================================================================
REM Ava Studio plugin build script (Windows)
REM
REM Compila UN SOLO plugin (ai_agent o hello_world) contra el
REM build_studio\ que ya existe, sin tocar ava_studio.exe ni el resto
REM del proyecto -- para iterar rapido en un plugin sin esperar a que
REM recompile el IDE entero cada vez. NO configura CMake: si
REM build_studio\ no existe todavia, corre build_studio.bat primero (una
REM sola vez alcanza, después este script reusa esa configuracion).
REM
REM Usage:
REM   build_plugin.bat                    build ai_agent (default), Release
REM   build_plugin.bat ai_agent           lo mismo, explicito
REM   build_plugin.bat hello_world        build hello_world en cambio
REM   build_plugin.bat ai_agent debug     build ai_agent en Debug
REM
REM El nombre de plugin y "debug" pueden ir en cualquier orden.
REM =====================================================================

set "BUILD_DIR=build_studio"
set "BUILD_TYPE=Release"
set "PLUGIN_NAME=ai_agent"

:parse_args
if "%~1"=="" goto args_done
if /I "%~1"=="debug"       (set "BUILD_TYPE=Debug" & shift & goto parse_args)
if /I "%~1"=="ai_agent"    (set "PLUGIN_NAME=ai_agent" & shift & goto parse_args)
if /I "%~1"=="hello_world" (set "PLUGIN_NAME=hello_world" & shift & goto parse_args)
if /I "%~1"=="help"        goto show_help
if /I "%~1"=="/?"          goto show_help
echo [ERROR] Unknown argument: %~1
goto show_help

:args_done

if not exist "%BUILD_DIR%" (
    echo [INFO] %BUILD_DIR%\ doesn't exist yet -- needs to be configured once
    echo        before this script can build just the plugin. Running
    echo        build_studio.bat first ^(this one full build is unavoidable
    echo        the first time; after this, build_plugin.bat reuses it^) ...
    echo.
    set "BOOTSTRAP_ARG="
    if /I "%BUILD_TYPE%"=="Debug" set "BOOTSTRAP_ARG=debug"
    call "%~dp0build_studio.bat" !BOOTSTRAP_ARG!
    if errorlevel 1 (
        echo [ERROR] build_studio.bat failed. See output above.
        exit /b 1
    )
    echo.
)

where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] cmake was not found on PATH.
    echo         Install it from https://cmake.org/download/ and re-run.
    exit /b 1
)

set "PLUGIN_TARGET=%PLUGIN_NAME%_plugin"

echo.
echo Building %PLUGIN_TARGET% ^(%BUILD_TYPE%^) against existing %BUILD_DIR%\ ...
cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --target %PLUGIN_TARGET% --parallel
if errorlevel 1 (
    echo [ERROR] Build failed. See output above.
    echo         If this is a stale-cache error ^(target not found, etc.^),
    echo         run build_studio.bat once to reconfigure, then retry.
    exit /b 1
)

REM Multi-config generators (Visual Studio) put ava_studio.exe under
REM <build>\studio\<Config>\; single-config ones (Ninja) put it under
REM <build>\studio\ directly -- the plugin's RUNTIME_OUTPUT_DIRECTORY is
REM "$<TARGET_FILE_DIR:ava_studio>/plugins", i.e. right next to it.
set "STUDIO_EXE_DIR=%BUILD_DIR%\studio\%BUILD_TYPE%"
if not exist "%STUDIO_EXE_DIR%\ava_studio.exe" set "STUDIO_EXE_DIR=%BUILD_DIR%\studio"
set "PLUGIN_DLL=%STUDIO_EXE_DIR%\plugins\%PLUGIN_NAME%.dll"

echo.
echo =====================================================================
echo Build succeeded.
echo %PLUGIN_NAME%.dll: %PLUGIN_DLL%
echo =====================================================================
echo.
echo Si ava_studio.exe ya esta abierto, cerralo y volvelo a abrir para que
echo cargue el .dll nuevo -- PluginHost::LoadAll solo escanea plugins\ al
echo arrancar, no hay hot-reload.

endlocal
exit /b 0

:show_help
echo Usage: build_plugin.bat [ai_agent^|hello_world] [debug]
echo   ai_agent      build the ai_agent plugin (default)
echo   hello_world   build the hello_world plugin instead
echo   debug         build Debug instead of Release
echo.
echo Requires build_studio\ to already be configured -- runs
echo build_studio.bat once automatically if it's missing.
exit /b 0
