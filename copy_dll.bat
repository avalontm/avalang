@echo off
REM =============================================================================
REM copy_dll.bat - Copia avalang.dll al proyecto dotnet
REM 
REM Uso:
REM   copy_dll.bat              - Copia Release DLL
REM   copy_dll.bat debug         - Copia Debug DLL
REM   copy_dll.bat all           - Copia ambas versiones
REM =============================================================================

setlocal enabledelayedexpansion

set "SOURCE_DIR=%~dp0"
set "DOTNET_DIR=%SOURCE_DIR%..\avalang-dotnet\AvaLang.Interop\runtimes\win-x64\native"

REM Cambiar a directorio del script
cd /d "%SOURCE_DIR%"

REM Verificar que existe el directorio de salida
if not exist "build" (
    echo [ERROR] No se encontro build\. Ejecuta build.bat primero.
    exit /b 1
)

REM Verificar argumentos
set "CONFIG=Release"
if "%~1"=="debug" set "CONFIG=Debug"
if "%~1"=="all" goto :copy_all

:copy_single
echo Copiando avalang.dll (%CONFIG%) al proyecto dotnet...
if not exist "build\%CONFIG%\avalang.dll" (
    echo [ERROR] No se encontro build\%CONFIG%\avalang.dll
    exit /b 1
)

if not exist "%DOTNET_DIR%" mkdir "%DOTNET_DIR%"
copy /Y "build\%CONFIG%\avalang.dll" "%DOTNET_DIR%\" >nul
if errorlevel 1 (
    echo [ERROR] Fallo al copiar el DLL
    exit /b 1
)
echo [OK] DLL copiado a %DOTNET_DIR%
exit /b 0

:copy_all
echo Copiando ambas versiones de avalang.dll...
if not exist "build\Release\avalang.dll" (
    echo [WARNING] No se encontro build\Release\avalang.dll
)
if not exist "build\Debug\avalang.dll" (
    echo [WARNING] No se encontro build\Debug\avalang.dll
)

if not exist "%DOTNET_DIR%" mkdir "%DOTNET_DIR%"

if exist "build\Release\avalang.dll" (
    copy /Y "build\Release\avalang.dll" "%DOTNET_DIR%\" >nul
    echo [OK] Release DLL copiado
)

if exist "build\Debug\avalang.dll" (
    REM Copiar Debug a una subcarpeta alternativa
    if not exist "%DOTNET_DIR%\debug" mkdir "%DOTNET_DIR%\debug"
    copy /Y "build\Debug\avalang.dll" "%DOTNET_DIR%\debug\" >nul
    echo [OK] Debug DLL copiado a debug\
)

echo.
echo [OK] Todos los DLLs copiados
exit /b 0