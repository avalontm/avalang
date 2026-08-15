@echo off
setlocal enabledelayedexpansion

REM =====================================================================
REM sign_release.bat -- Fase 5 de plan_ava_pack.md (ver runtime/avapack/README.md).
REM
REM Firma manualmente un .exe ya construido (empacado o no) con signtool,
REM usando un certificado de firma de codigo propio (.pfx). Este script NO
REM crea ni gestiona certificados -- asume que ya conseguiste uno (de una
REM CA publica o interno de tu organizacion) y lo tenes a mano como .pfx.
REM
REM `ava_cli build --sign-pfx ...` (runtime/avacli/src/build_command.cpp)
REM ya hace este mismo paso automaticamente al final de un build empacado.
REM Este script es para firmar por separado -- re-firmar un build viejo,
REM firmar un binario que no vino de ava_cli build, o correrlo a mano para
REM depurar un problema de firma sin tener que re-compilar nada.
REM
REM Uso:
REM   sign_release.bat <ruta.exe> <ruta.pfx> [password_env] [timestamp_url]
REM
REM   ruta.exe        Binario a firmar (se firma in-place).
REM   ruta.pfx        Certificado de firma de codigo.
REM   password_env    (opcional) Nombre de una variable de entorno que ya
REM                   tenga la password del .pfx. Igual que en ava_cli
REM                   build, la password NUNCA se pasa como argumento de
REM                   este .bat -- si tu .pfx tiene password, exportala a
REM                   una variable de entorno ANTES de llamar a este script
REM                   (ej. `set MI_PFX_PASSWORD=...`) y pasa el NOMBRE de
REM                   esa variable aca, no el valor.
REM   timestamp_url   (opcional, default: http://timestamp.digicert.com)
REM                   Servidor de timestamp RFC 3161. Sin timestamp, la
REM                   firma deja de ser valida cuando expire el
REM                   certificado, aunque el binario no cambie.
REM =====================================================================

if "%~1"=="" (
    echo uso: sign_release.bat ^<ruta.exe^> ^<ruta.pfx^> [password_env] [timestamp_url]
    exit /b 1
)
if "%~2"=="" (
    echo uso: sign_release.bat ^<ruta.exe^> ^<ruta.pfx^> [password_env] [timestamp_url]
    exit /b 1
)

set "EXE_PATH=%~1"
set "PFX_PATH=%~2"
set "PASSWORD_ENV=%~3"
set "TIMESTAMP_URL=%~4"
if "%TIMESTAMP_URL%"=="" set "TIMESTAMP_URL=http://timestamp.digicert.com"

if not exist "%EXE_PATH%" (
    echo error: no existe %EXE_PATH%
    exit /b 1
)
if not exist "%PFX_PATH%" (
    echo error: no existe %PFX_PATH%
    exit /b 1
)

where signtool >nul 2>nul
if errorlevel 1 (
    echo error: signtool no esta en el PATH.
    echo        Viene con el Windows SDK -- normalmente se agrega al PATH abriendo
    echo        una "Developer Command Prompt for VS" en vez de un cmd.exe comun.
    exit /b 1
)

set "SIGNTOOL_ARGS=sign /f "%PFX_PATH%" /fd sha256 /tr %TIMESTAMP_URL% /td sha256"

if not "%PASSWORD_ENV%"=="" (
    REM No expandimos la password en este .bat mas alla de pasarsela a
    REM signtool -- no se imprime ni se guarda en ningun log de este script.
    call set "PFX_PASSWORD=%%%PASSWORD_ENV%%%"
    if "!PFX_PASSWORD!"=="" (
        echo error: la variable de entorno %PASSWORD_ENV% no esta definida o esta vacia.
        exit /b 1
    )
    set "SIGNTOOL_ARGS=%SIGNTOOL_ARGS% /p "!PFX_PASSWORD!""
)

echo firmando %EXE_PATH% ...
signtool %SIGNTOOL_ARGS% "%EXE_PATH%"
set "SIGNTOOL_EXIT=%errorlevel%"
set "PFX_PASSWORD="

if not "%SIGNTOOL_EXIT%"=="0" (
    echo error: signtool fallo ^(exit code %SIGNTOOL_EXIT%^)
    exit /b %SIGNTOOL_EXIT%
)

echo listo: %EXE_PATH% firmado.
exit /b 0
