# Duster (VST3 / AU / AAX / Standalone)

[![Build Audio Plugins](https://github.com/scobru/duster/actions/workflows/build.yml/badge.svg)](https://github.com/scobru/duster/actions/workflows/build.yml)
[![GitHub Release](https://img.shields.io/github/v/release/scobru/duster?color=blue)](https://github.com/scobru/duster/releases)

> **Plugin professionale di riduzione del rumore spettrale ispirato a BlueLab (BL) Denoiser, con cattura del profilo di rumore (Learn), soppressione del rumore musicale (Residual Smooth), preservazione dei transienti (Transient Boost) e visualizzatore FFT in tempo reale.**

---

## 🎧 Caratteristiche Principali (Confronto con BL Denoiser)

| Funzionalità | BL Denoiser (v5.1.2) | Duster |
| :--- | :--- | :--- |
| **Apprendimento Rumore** | Tasto Learn su sezione rumorosa | ✅ Tasto `LEARN NOISE` con radar di scansione e salvataggio stato |
| **Ascolto Rumore Estratto** | Listen noise only | ✅ Modalità `Noise Only (Delta)` con ritardo compensato al campione |
| **Soppressione Musical Noise** | Residual noise removal | ✅ Algoritmo Wiener Decision-Directed (Ephraim-Malah) + Smoothing frequenziale |
| **Transient Boost** | Aumento attacco/transienti | ✅ Rivelatore di flusso spettrale con boost dinamico degli attacchi |
| **Qualità FFT** | Parametro Fast / High quality | ✅ Selettore FFT: Fast (1024), Standard (2048), High (4096) |
| **Interfaccia Grafica** | Analizzatore standard | ✅ UI moderna Dark Mode con spettro FFT in tempo reale e curva profilo |
| **Supporto Formati** | VST, AU | ✅ **VST3**, **AU (AudioUnit)**, **AAX (Pro Tools)** e **Standalone** |

---

## 🎛️ Controlli e Parametri

1. **LEARN NOISE (Pulsante Principale)**
   - Fai partire la riproduzione su un punto della traccia in cui c'è **soltanto rumore** (ad es. l'inizio o la fine della registrazione, prima che inizi la voce o lo strumento).
   - Clicca su `● LEARN NOISE`: il pulsante diventerà rosso (`■ STOP LEARNING`) e registrerà la firma spettrale del rumore.
   - Dopo 1–2 secondi riclicca per salvare il profilo. Lo spettro mostrerà la curva dorata del rumore appreso!
   - Tasto `CLEAR`: cancella il profilo memorizzato.

2. **REDUCTION (0 dB - 40 dB)**
   - Determina l'attenuazione del rumore. Integra un floor spettrale adattivo per evitare che la stanza sembri improvvisamente "morta" o che si crei gating innaturale.

3. **THRESHOLD (-18 dB - +18 dB)**
   - Alza o abbassa la sensibilità del profilo di rumore appreso.

4. **RESIDUAL SMOOTH (0% - 100%)**
   - Elimina i tipici artefatti da sottrazione spettrale noti come *"musical noise"* (quei cinguettii/tintinnii metallici di fondo). Applica un filtro Decision-Directed con smoothing armonico.

5. **TRANSIENT BOOST (0% - 100%)**
   - La sottrazione spettrale tende spesso ad ammorbidire le consonanti della voce ("s", "t", "k") o l'attacco percussivo (chitarra acustica, rullante). Questo parametro rileva i transienti e apre istantaneamente il gain per conservare la brillantezza e il punch.

6. **LISTEN MODE**
   - **Denoised**: Output pulito normale.
   - **Noise Only (Delta)**: Ascolta esclusivamente il rumore sottratto ($X_{in} - X_{out}$). Fondamentale in fase di mix per assicurarsi che non si stia tagliando parte della voce o dello strumento.
   - **Bypass**: Trasparente pass-through a latenza identica.

7. **MIX (0% - 100%)**
   - Crossfade dry/wet allineato in fase.

8. **QUALITY**
   - **Fast (1024)**: Bassa latenza, ideale per registrazione live e tracking.
   - **Standard (2048)**: Ottimale per missaggio vocale e strumenti.
   - **High (4096)**: Massima risoluzione spettrale nelle basse frequenze (hum a 50/60 Hz, rumori costanti).

---

## 📁 Posizione dei File Compilati

Troverai i binari nella cartella di build (oppure precompilati su [GitHub Releases](https://github.com/scobru/duster/releases)):
- **Plugin VST3 (Windows)**:
  `build\Duster_artefacts\Release\VST3\Duster.vst3`
- **Applicazione Standalone (Windows .exe)**:
  `build\Duster_artefacts\Release\Standalone\Duster.exe`
- **Plugin AAX per Pro Tools (se SDK presente)**:
  `build\Duster_artefacts\Release\AAX\Duster.aaxplugin`

### Come installarlo nella tua DAW su Windows (Ableton, FL Studio, Cubase, Reaper, etc.)
Copia l'intera cartella o il file `Duster.vst3` nella cartella VST3 standard di Windows:
```
C:\Program Files\Common Files\VST3\
```
Oppure esegui `install_vst3.bat` come amministratore. Fai una scansione dei plugin nella tua DAW e troverai **Duster** sotto la categoria "AcousticLab" o "Restoration/Denoiser".

---

## 🎚️ Compatibilità con Avid Pro Tools (AAX)

Il progetto supporta la generazione nativa del target **AAX** per Avid Pro Tools tramite CMake e JUCE:

1. **Compilazione Nativa AAX**:
   - Scarica l'AAX SDK da [Avid Developer Central](https://developer.avid.com) e collocalo nella directory radice come `aax-sdk/` oppure imposta la variabile d'ambiente `JUCE_AAX_SDK_PATH`.
   - Il target `Duster_AAX` verrà abilitato automaticamente durante la configurazione CMake (`set(DUSTER_FORMATS VST3 AU Standalone AAX)`).
   - In macOS il file prodotto è `Duster.aaxplugin` (destinazione `/Library/Application Support/Avid/Audio/Plug-Ins/`).
   - In Windows il bundle `Duster.aaxplugin` va in `C:\Program Files\Common Files\Avid\Audio\Plug-Ins\`.
   - *Nota per Pro Tools Retail*: Per caricare binari AAX in Pro Tools commerciale è necessaria la firma digitale PACE Eden (`wraptool` con certificato Avid/iLok). Nelle edizioni Pro Tools Developer Edition è possibile caricare anche il binario senza firma PACE.

2. **Utilizzo immediato in Pro Tools tramite wrapper VST3**:
   - Puoi caricare direttamente **Duster.vst3** dentro qualsiasi versione di Pro Tools usando wrapper professionali AAX come **Blue Cat's PatchWork** o **DDMF Metaplugin**, senza richiedere alcuna firma PACE proprietaria.

---

## 🍎 Compilazione su macOS per AU (AudioUnit), VST3 e AAX

Il progetto è basato su CMake nativo con supporto integrato per macOS:
1. Clona la cartella su Mac con Git (`git clone https://github.com/scobru/duster.git`).
2. Esegui nel terminale:
   ```bash
   cmake -B build -G Xcode
   cmake --build build --config Release
   ```
3. Troverai il componente **AU** (`Duster.component`) per Logic Pro e GarageBand, il **VST3** (`Duster.vst3`) e **AAX** (`Duster.aaxplugin`).

---

## 🧱 Struttura del Codice

```
duster/
├── CMakeLists.txt                    # Configurazione CMake per VST3, AU, Standalone
├── JUCE\                             # Framework JUCE 7.0.12
├── Source\
│   ├── PluginProcessor.h / .cpp      # Gestione parametri APVTS, latenza e stato DAW
│   ├── PluginEditor.h / .cpp         # Layout interfaccia grafica e controlli
│   ├── DSP\
│   │   ├── DusterEngine.h / .cpp     # Motore STFT WOLA, Decision-Directed Wiener & Learn
│   │   └── SpectrumAnalyzerBridge.h  # Bridge lock-free tra audio thread e GUI
│   └── GUI\
│       ├── CustomRotarySlider.h      # Manopole rotative personalizzate
│       ├── DusterVisualizer.h / .cpp # Visualizzatore spettrale FFT a 40 FPS
└── build\                            # Output di compilazione
```
