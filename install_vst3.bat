@echo off
echo ========================================================
echo   Installazione Duster VST3 nella DAW...
echo ========================================================

set "SRC=%~dp0Duster.vst3"
if not exist "%SRC%" (
    set "SRC=%~dp0build\Duster_artefacts\Release\VST3\Duster.vst3"
)

set "DEST=C:\Program Files\Common Files\VST3\Duster.vst3"

if not exist "%SRC%" (
    echo [ERRORE] Il file VST3 non e stato trovato ne in:
    echo   "%~dp0Duster.vst3"
    echo ne in:
    echo   "%~dp0build\Duster_artefacts\Release\VST3\Duster.vst3"
    echo.
    echo Assicurati di aver compilato il progetto o estratto lo zip completo.
    pause
    exit /b 1
)

echo Sorgente trovata: "%SRC%"
echo Copia del plugin in "%DEST%"...
xcopy /E /I /Y "%SRC%" "%DEST%"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================================
    echo [SUCCESSO] Plugin VST3 installato correttamente!
    echo Apri la tua DAW (Ableton, FL Studio, Cubase, Reaper) e fai un rescan dei plugin.
    echo ========================================================
) else (
    echo.
    echo ========================================================
    echo [AVVISO] Copia non riuscita.
    echo Per copiare in "Program Files" e necessario eseguire
    echo questo script come Amministratore:
    echo (Fai click destro su install_vst3.bat e seleziona "Esegui come amministratore")
    echo ========================================================
)

pause
