@echo off
echo ========================================================
echo   Installazione Spectra Denoiser VST3 nella DAW...
echo ========================================================

set "SRC=%~dp0build\SpectraDenoiser_artefacts\Release\VST3\Spectra Denoiser.vst3"
set "DEST=C:\Program Files\Common Files\VST3\Spectra Denoiser.vst3"

if not exist "%SRC%" (
    echo [ERRORE] Il file VST3 non e stato trovato in %SRC%.
    pause
    exit /b 1
)

echo Copia del plugin in %DEST%...
xcopy /E /I /Y "%SRC%" "%DEST%"

if %ERRORLEVEL% EQU 0 (
    echo [SUCCESSO] Plugin VST3 installato correttamente!
    echo Apri la tua DAW (Ableton, FL Studio, Cubase, Reaper) e fai un rescan dei plugin.
) else (
    echo [AVVISO] Per copiare in Program Files potrebbe essere necessario eseguire questo script come Amministratore.
)

pause
