@echo off
setlocal enabledelayedexpansion
title Duster Audio Plugin Installer for Windows

echo ========================================================
echo          Duster - Audio Plugin Installer
echo ========================================================
echo.

:: Check for Administrator Privileges
net session >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ATTENZIONE] Sono richiesti i privilegi di Amministratore per installare
    echo i plugin nelle cartelle di sistema ("Program Files").
    echo.
    echo Riavvio dello script come Amministratore in corso...
    powershell -Command "Start-Process cmd -ArgumentList '/c \"\"%~f0\"\"' -Verb RunAs"
    exit /b
)

set "BASE_DIR=%~dp0"
set "INSTALLED_ANY=0"

:: ----------------------------------------------------------
:: 1. VST3 Installation
:: ----------------------------------------------------------
set "VST3_SRC=%BASE_DIR%Duster.vst3"
if not exist "%VST3_SRC%" set "VST3_SRC=%BASE_DIR%build\Duster_artefacts\Release\VST3\Duster.vst3"
set "VST3_DEST=C:\Program Files\Common Files\VST3\Duster.vst3"

if exist "%VST3_SRC%" (
    echo [1/3] Installazione VST3...
    if not exist "C:\Program Files\Common Files\VST3" mkdir "C:\Program Files\Common Files\VST3"
    xcopy /E /I /Y "%VST3_SRC%" "%VST3_DEST%" >nul
    if !ERRORLEVEL! EQU 0 (
        echo      - VST3 installato con successo in:
        echo        "%VST3_DEST%"
        set "INSTALLED_ANY=1"
    ) else (
        echo      - [ERRORE] Impossibile copiare VST3.
    )
) else (
    echo [1/3] Duster.vst3 non presente nel pacchetto (saltato).
)
echo.

:: ----------------------------------------------------------
:: 2. AAX (Pro Tools) Installation
:: ----------------------------------------------------------
set "AAX_SRC=%BASE_DIR%Duster.aaxplugin"
if not exist "%AAX_SRC%" set "AAX_SRC=%BASE_DIR%build\Duster_artefacts\Release\AAX\Duster.aaxplugin"
set "AAX_DEST=C:\Program Files\Common Files\Avid\Audio\Plug-Ins\Duster.aaxplugin"

if exist "%AAX_SRC%" (
    echo [2/3] Installazione AAX (Pro Tools)...
    if not exist "C:\Program Files\Common Files\Avid\Audio\Plug-Ins" mkdir "C:\Program Files\Common Files\Avid\Audio\Plug-Ins"
    xcopy /E /I /Y "%AAX_SRC%" "%AAX_DEST%" >nul
    if !ERRORLEVEL! EQU 0 (
        echo      - AAX installato con successo in:
        echo        "%AAX_DEST%"
        set "INSTALLED_ANY=1"
    ) else (
        echo      - [ERRORE] Impossibile copiare AAX.
    )
) else (
    echo [2/3] Duster.aaxplugin non presente (saltato).
)
echo.

:: ----------------------------------------------------------
:: 3. Standalone Application
:: ----------------------------------------------------------
set "EXE_SRC=%BASE_DIR%Duster.exe"
if not exist "%EXE_SRC%" set "EXE_SRC=%BASE_DIR%build\Duster_artefacts\Release\Standalone\Duster.exe"

if exist "%EXE_SRC%" (
    echo [3/3] Applicazione Standalone trovata in:
    echo      "%EXE_SRC%"
    echo      (Eseguibile standalone pronto all'uso, non richiede installazione).
) else (
    echo [3/3] Applicazione Standalone non presente (saltata).
)
echo.

:: ----------------------------------------------------------
:: Conclusion
:: ----------------------------------------------------------
echo ========================================================
if !INSTALLED_ANY! EQU 1 (
    echo   [SUCCESSO] Installazione completata con successo!
    echo.
    echo   Prossimi passi:
    echo   1. Apri la tua DAW (Ableton, FL Studio, Cubase, Reaper, Pro Tools, Studio One).
    echo   2. Esegui un rescan dei plugin VST3 / AAX.
    echo   3. Troverai "Duster" nella lista degli effetti (categoria AcousticLab/Restoration).
) else (
    echo   [AVVISO] Nessun file di plugin trovato da installare.
    echo   Assicurati di aver estratto l'intero archivio zip o di aver compilato il progetto.
)
echo ========================================================
echo.
pause
