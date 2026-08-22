@echo off
setlocal enabledelayedexpansion

REM Este script vive en scripts\ (ver AVALAND_STRUCT.md); nos
REM movemos a la raiz del repo (un nivel arriba) para que las rutas
REM relativas (build\, samples\test\main.ava, etc.) sigan funcionando
REM sin importar desde donde se invoque este .bat.
cd /d "%~dp0.."

REM =====================================================================
REM Fase 8 -- Optimizacion: items 6-9 (medir tamano de DLL, memoria de
REM VM, startup, rendimiento). Ver avalang_runtime_stl_barekernel_plan.md.
REM
REM Requiere un build Release ya hecho (ver build.bat). No compila nada
REM aqui -- solo busca los binarios ya generados y los mide. La logica
REM de timing/memoria vive en measure_release.ps1 (mas facil de leer y
REM de mantener que PowerShell embebido en un .bat).
REM
REM Usage:
REM   measure_release.bat             usa .\build (default de build.bat)
REM   measure_release.bat <carpeta>   usa esa carpeta de build en su lugar
REM =====================================================================

set "BUILD_DIR=%~1"
if "%BUILD_DIR%"=="" set "BUILD_DIR=build"

if not exist "%BUILD_DIR%" (
    echo [ERROR] No existe "%BUILD_DIR%". Corre build.bat primero ^(build Release^).
    exit /b 1
)

where powershell >nul 2>nul
if errorlevel 1 (
    echo [ERROR] powershell no esta disponible en PATH ^(deberia venir con Windows^).
    exit /b 1
)

echo Buscando binarios en %BUILD_DIR%\ ...
set "AVA_CLI="
set "AVALANG_DLL="
set "AVALANG_UI_DLL="
for /r "%BUILD_DIR%" %%F in (ava_cli.exe) do if not defined AVA_CLI set "AVA_CLI=%%F"
for /r "%BUILD_DIR%" %%F in (avalang.dll) do if not defined AVALANG_DLL set "AVALANG_DLL=%%F"
for /r "%BUILD_DIR%" %%F in (avalang_ui.dll) do if not defined AVALANG_UI_DLL set "AVALANG_UI_DLL=%%F"

if not defined AVA_CLI (
    echo [ERROR] No se encontro ava_cli.exe bajo %BUILD_DIR%\. Compila con build.bat primero.
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0measure_release.ps1" ^
    -AvaCli "%AVA_CLI%" -AvalangDll "%AVALANG_DLL%" -AvalangUiDll "%AVALANG_UI_DLL%"

endlocal
exit /b 0
