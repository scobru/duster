@echo off
set "EXE=%~dp0Spectra Denoiser.exe"
if not exist "%EXE%" (
    set "EXE=%~dp0build\SpectraDenoiser_artefacts\Release\Standalone\Spectra Denoiser.exe"
)

if not exist "%EXE%" (
    echo [ERRORE] Eseguibile non trovato ne in:
    echo   "%~dp0Spectra Denoiser.exe"
    echo ne in:
    echo   "%~dp0build\SpectraDenoiser_artefacts\Release\Standalone\Spectra Denoiser.exe"
    pause
    exit /b 1
)

echo Avvio di Spectra Denoiser (Standalone)...
start "" "%EXE%"
