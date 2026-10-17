@echo off
set "EXE=%~dp0Duster.exe"
if not exist "%EXE%" (
    set "EXE=%~dp0build\Duster_artefacts\Release\Standalone\Duster.exe"
)

if not exist "%EXE%" (
    echo [ERRORE] Eseguibile non trovato ne in:
    echo   "%~dp0Duster.exe"
    echo ne in:
    echo   "%~dp0build\Duster_artefacts\Release\Standalone\Duster.exe"
    pause
    exit /b 1
)

echo Avvio di Duster (Standalone)...
start "" "%EXE%"
