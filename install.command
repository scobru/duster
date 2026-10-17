#!/bin/bash
set -e

# ========================================================
#   Duster - macOS Audio Plugin Installer
#   Supports: AU (.component), VST3, AAX, Standalone (.app)
# ========================================================

echo "========================================================"
echo "          Duster - macOS Plugin Installer"
echo "========================================================"
echo ""

if [[ "$OSTYPE" != "darwin"* ]]; then
    echo "❌ Errore: questo script e progettato esclusivamente per macOS."
    exit 1
fi

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
INSTALLED_ANY=0

echo "🔍 Rilevamento file in: $SCRIPT_DIR"
echo ""

# ----------------------------------------------------------
# 0. Rimuovi attributi di quarantena Apple (Gatekeeper)
# ----------------------------------------------------------
echo "🔓 Rimozione attributi di quarantena macOS (Gatekeeper)..."
xattr -cr "$SCRIPT_DIR" 2>/dev/null || true
echo "   Fatto."
echo ""

# ----------------------------------------------------------
# 1. Installazione AU (Audio Unit - Logic Pro, GarageBand)
# ----------------------------------------------------------
AU_SRC="$SCRIPT_DIR/Duster.component"
if [ ! -d "$AU_SRC" ]; then AU_SRC="$SCRIPT_DIR/build/Duster_artefacts/Release/AU/Duster.component"; fi
AU_DEST="$HOME/Library/Audio/Plug-Ins/Components"

if [ -d "$AU_SRC" ]; then
    echo "📦 [1/4] Installazione Audio Unit (AU)..."
    mkdir -p "$AU_DEST"
    rm -rf "$AU_DEST/Duster.component"
    cp -R "$AU_SRC" "$AU_DEST/"
    xattr -cr "$AU_DEST/Duster.component" 2>/dev/null || true
    echo "   ✅ Installato in: $AU_DEST/Duster.component"
    INSTALLED_ANY=1
else
    echo "ℹ️  [1/4] Duster.component non trovato (saltato)."
fi
echo ""

# ----------------------------------------------------------
# 2. Installazione VST3 (Ableton, Cubase, Reaper, Studio One, etc.)
# ----------------------------------------------------------
VST3_SRC="$SCRIPT_DIR/Duster.vst3"
if [ ! -d "$VST3_SRC" ]; then VST3_SRC="$SCRIPT_DIR/build/Duster_artefacts/Release/VST3/Duster.vst3"; fi
VST3_DEST="$HOME/Library/Audio/Plug-Ins/VST3"

if [ -d "$VST3_SRC" ]; then
    echo "📦 [2/4] Installazione VST3..."
    mkdir -p "$VST3_DEST"
    rm -rf "$VST3_DEST/Duster.vst3"
    cp -R "$VST3_SRC" "$VST3_DEST/"
    xattr -cr "$VST3_DEST/Duster.vst3" 2>/dev/null || true
    echo "   ✅ Installato in: $VST3_DEST/Duster.vst3"
    INSTALLED_ANY=1
else
    echo "ℹ️  [2/4] Duster.vst3 non trovato (saltato)."
fi
echo ""

# ----------------------------------------------------------
# 3. Installazione AAX (Avid Pro Tools)
# ----------------------------------------------------------
AAX_SRC="$SCRIPT_DIR/Duster.aaxplugin"
if [ ! -d "$AAX_SRC" ]; then AAX_SRC="$SCRIPT_DIR/build/Duster_artefacts/Release/AAX/Duster.aaxplugin"; fi
AAX_DEST="/Library/Application Support/Avid/Audio/Plug-Ins"

if [ -d "$AAX_SRC" ]; then
    echo "📦 [3/4] Installazione AAX (Pro Tools)..."
    echo "   (La cartella Avid di sistema richiede privilegi amministratore):"
    sudo mkdir -p "$AAX_DEST"
    sudo rm -rf "$AAX_DEST/Duster.aaxplugin"
    sudo cp -R "$AAX_SRC" "$AAX_DEST/"
    sudo xattr -cr "$AAX_DEST/Duster.aaxplugin" 2>/dev/null || true
    echo "   ✅ Installato in: $AAX_DEST/Duster.aaxplugin"
    INSTALLED_ANY=1
else
    echo "ℹ️  [3/4] Duster.aaxplugin non trovato (saltato)."
fi
echo ""

# ----------------------------------------------------------
# 4. Installazione Standalone (.app)
# ----------------------------------------------------------
APP_SRC="$SCRIPT_DIR/Duster.app"
if [ ! -d "$APP_SRC" ]; then APP_SRC="$SCRIPT_DIR/build/Duster_artefacts/Release/Standalone/Duster.app"; fi
APP_DEST="/Applications"

if [ -d "$APP_SRC" ]; then
    echo "📦 [4/4] Installazione Applicazione Standalone..."
    rm -rf "$APP_DEST/Duster.app"
    cp -R "$APP_SRC" "$APP_DEST/"
    xattr -cr "$APP_DEST/Duster.app" 2>/dev/null || true
    echo "   ✅ Installato in: $APP_DEST/Duster.app"
    INSTALLED_ANY=1
else
    echo "ℹ️  [4/4] Duster.app non trovato (saltato)."
fi
echo ""

# ----------------------------------------------------------
# Riepilogo e istruzioni DAW
# ----------------------------------------------------------
echo "========================================================"
if [ $INSTALLED_ANY -eq 1 ]; then
    echo "🎉 Installazione completata con successo!"
    echo ""
    echo "Prossimi passi per la tua DAW:"
    echo "• Logic Pro / GarageBand: Preferences → Plug-In Manager → Full AudioUnit Reset"
    echo "• Ableton Live: Preferences → Plug-ins → Rescan"
    echo "• Cubase / Studio One / REAPER: Rescan VST3 plugins"
    echo "• Pro Tools: Riavvia Pro Tools (riconoscera automaticamente Duster)"
else
    echo "⚠️  Nessun plugin trovato da installare."
    echo "Assicurati di aver estratto l'intero pacchetto zip o compilato il progetto."
fi
echo "========================================================"
echo ""
read -p "Premi Invio per chiudere questa finestra..."
