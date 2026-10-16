#include "SpectraVisualizer.h"
#include <cmath>

namespace gui
{

SpectraVisualizer::SpectraVisualizer(dsp::SpectrumAnalyzerBridge& bridge)
    : spectrumBridge(bridge)
{
    inputMagnitudes.resize(2048, -100.0f);
    outputMagnitudes.resize(2048, -100.0f);
    noiseProfileMagnitudes.resize(2048, -100.0f);

    smoothedInput.resize(2048, -100.0f);
    smoothedOutput.resize(2048, -100.0f);

    startTimerHz(40); // 40 FPS silky visualizer
}

SpectraVisualizer::~SpectraVisualizer()
{
    stopTimer();
}

void SpectraVisualizer::timerCallback()
{
    int updatedBins = numBins;
    bool hasNewData = spectrumBridge.fetchSpectra(inputMagnitudes, outputMagnitudes, noiseProfileMagnitudes, updatedBins);
    if (updatedBins > 0)
        numBins = updatedBins;

    if (isLearning)
    {
        scanPhase += 0.05f;
        if (scanPhase > 1.0f)
            scanPhase -= 1.0f;
    }

    // Ballistics decay smoothing for visualizer (fast attack, smooth release)
    const float decay = 0.82f;
    for (int i = 0; i < numBins; ++i)
    {
        float inDb = (i < (int)inputMagnitudes.size()) ? inputMagnitudes[i] : -100.0f;
        if (inDb > smoothedInput[i])
            smoothedInput[i] = inDb;
        else
            smoothedInput[i] = smoothedInput[i] * decay + inDb * (1.0f - decay);

        float outDb = (i < (int)outputMagnitudes.size()) ? outputMagnitudes[i] : -100.0f;
        if (outDb > smoothedOutput[i])
            smoothedOutput[i] = outDb;
        else
            smoothedOutput[i] = smoothedOutput[i] * decay + outDb * (1.0f - decay);
    }

    repaint();
}

void SpectraVisualizer::resized()
{
}

float SpectraVisualizer::mapFreqToX(float freq, float width) const
{
    const float minFreq = 20.0f;
    const float maxFreq = 20000.0f;
    if (freq <= minFreq) return 0.0f;
    if (freq >= maxFreq) return width;

    return width * (std::log10(freq / minFreq) / std::log10(maxFreq / minFreq));
}

float SpectraVisualizer::mapDbToY(float db, float height) const
{
    const float maxDb = 0.0f;
    const float minDb = -90.0f;
    float clamped = juce::jlimit(minDb, maxDb, db);
    return height * (maxDb - clamped) / (maxDb - minDb);
}

void SpectraVisualizer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();

    // 1. Sleek Background with subtle inner gradient
    juce::ColourGradient bgGrad(juce::Colour(0xff121620), 0, 0,
                               juce::Colour(0xff0b0e14), 0, h, false);
    g.setGradientFill(bgGrad);
    g.fillRoundedRectangle(bounds, 8.0f);

    g.setColour(juce::Colour(0xff2a3245));
    g.drawRoundedRectangle(bounds, 8.0f, 1.2f);

    // 2. Frequency and dB Grid Lines
    const std::vector<float> gridFreqs = { 50.0f, 100.0f, 250.0f, 500.0f, 1000.0f, 2500.0f, 5000.0f, 10000.0f, 20000.0f };
    const std::vector<juce::String> gridFreqLabels = { "50", "100", "250", "500", "1k", "2.5k", "5k", "10k", "20k" };

    g.setFont(juce::Font(10.0f, juce::Font::plain));
    for (size_t i = 0; i < gridFreqs.size(); ++i)
    {
        float x = mapFreqToX(gridFreqs[i], w);
        g.setColour(juce::Colour(0x1affffff));
        g.drawVerticalLine((int)x, 0.0f, h);

        g.setColour(juce::Colour(0x45ffffff));
        g.drawText(gridFreqLabels[i], (int)x + 2, (int)h - 14, 30, 12, juce::Justification::left);
    }

    const std::vector<float> gridDbs = { -12.0f, -24.0f, -36.0f, -48.0f, -60.0f, -72.0f, -84.0f };
    for (float db : gridDbs)
    {
        float y = mapDbToY(db, h);
        g.setColour(juce::Colour(0x12ffffff));
        g.drawHorizontalLine((int)y, 0.0f, w);

        g.setColour(juce::Colour(0x40ffffff));
        g.drawText(juce::String((int)db) + "dB", 4, (int)y - 12, 40, 12, juce::Justification::left);
    }

    // 3. Construct Paths for Input, Output, and Learned Noise Profile
    inputPath.clear();
    outputPath.clear();
    noisePath.clear();

    const float binFreqStep = (sampleRate * 0.5f) / (float)numBins;

    bool inputStarted = false;
    bool outputStarted = false;
    bool noiseStarted = false;

    for (int k = 1; k < numBins; ++k)
    {
        float freq = (float)k * binFreqStep;
        if (freq < 20.0f || freq > 20000.0f)
            continue;

        float x = mapFreqToX(freq, w);

        // Input
        float inDb = smoothedInput[k];
        float inY = mapDbToY(inDb, h);
        if (!inputStarted) { inputPath.startNewSubPath(x, inY); inputStarted = true; }
        else { inputPath.lineTo(x, inY); }

        // Output
        float outDb = smoothedOutput[k];
        float outY = mapDbToY(outDb, h);
        if (!outputStarted) { outputPath.startNewSubPath(x, outY); outputStarted = true; }
        else { outputPath.lineTo(x, outY); }

        // Noise Profile
        if (k < (int)noiseProfileMagnitudes.size() && noiseProfileMagnitudes[k] > -95.0f)
        {
            float noiseDb = noiseProfileMagnitudes[k];
            float noiseY = mapDbToY(noiseDb, h);
            if (!noiseStarted) { noisePath.startNewSubPath(x, noiseY); noiseStarted = true; }
            else { noisePath.lineTo(x, noiseY); }
        }
    }

    // 4. Render Input Spectrum (Translucent Cyan Fill + Curve)
    if (inputStarted)
    {
        juce::Path filledInput = inputPath;
        filledInput.lineTo(w, h);
        filledInput.lineTo(0.0f, h);
        filledInput.closeSubPath();

        juce::ColourGradient fillGrad(juce::Colour(0x2800d2ff), 0, 0,
                                      juce::Colour(0x0200d2ff), 0, h, false);
        g.setGradientFill(fillGrad);
        g.fillPath(filledInput);

        g.setColour(juce::Colour(0x9900d2ff));
        g.strokePath(inputPath, juce::PathStrokeType(1.2f));
    }

    // 5. Render Learned Noise Profile (Glow Amber Curve)
    if (noiseStarted)
    {
        // Outer soft glow
        g.setColour(juce::Colour(0x40ff9500));
        g.strokePath(noisePath, juce::PathStrokeType(4.0f));

        // Core line
        g.setColour(juce::Colour(0xffff9f1c));
        g.strokePath(noisePath, juce::PathStrokeType(2.0f));
    }

    // 6. Render Denoised Output Spectrum (Vibrant Neon Green)
    if (outputStarted)
    {
        g.setColour(juce::Colour(0xff00ff9d));
        g.strokePath(outputPath, juce::PathStrokeType(1.8f));
    }

    // 7. Visual Feedback during "LEARNING NOISE PROFILE"
    if (isLearning)
    {
        // Animated scanning radar line
        float scanX = scanPhase * w;
        juce::ColourGradient scanGrad(juce::Colour(0x00ff3b30), scanX - 40.0f, 0,
                                      juce::Colour(0xaaff3b30), scanX, 0, false);
        g.setGradientFill(scanGrad);
        g.fillRect(scanX - 40.0f, 0.0f, 40.0f, h);

        g.setColour(juce::Colour(0xffff453a));
        g.drawLine(scanX, 0.0f, scanX, h, 2.0f);

        // Center Pulsing Badge
        auto badgeRect = juce::Rectangle<float>(w * 0.5f - 140.0f, 15.0f, 280.0f, 30.0f);
        g.setColour(juce::Colour(0xe67f1d1d));
        g.fillRoundedRectangle(badgeRect, 6.0f);
        g.setColour(juce::Colour(0xffff453a));
        g.drawRoundedRectangle(badgeRect, 6.0f, 1.5f);

        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.setColour(juce::Colours::white);
        g.drawText("● LEARNING NOISE PROFILE (PLAY NOISE)", badgeRect, juce::Justification::centred);
    }
    else
    {
        // Status Badge Top Right
        auto badge = juce::Rectangle<float>(w - 150.0f, 12.0f, 138.0f, 22.0f);
        if (noiseStarted || hasProfile)
        {
            g.setColour(juce::Colour(0xcc143826));
            g.fillRoundedRectangle(badge, 4.0f);
            g.setColour(juce::Colour(0xff00ff9d));
            g.drawRoundedRectangle(badge, 4.0f, 1.0f);
            g.setFont(juce::Font(10.5f, juce::Font::bold));
            g.drawText("✓ NOISE PROFILE READY", badge, juce::Justification::centred);
        }
        else
        {
            g.setColour(juce::Colour(0xcc332a15));
            g.fillRoundedRectangle(badge, 4.0f);
            g.setColour(juce::Colour(0xffffa600));
            g.drawRoundedRectangle(badge, 4.0f, 1.0f);
            g.setFont(juce::Font(10.5f, juce::Font::plain));
            g.drawText("NO PROFILE (CLICK LEARN)", badge, juce::Justification::centred);
        }
    }

    // 8. Legend in Top Left
    auto legend = juce::Rectangle<float>(12.0f, 12.0f, 240.0f, 20.0f);
    g.setColour(juce::Colour(0xaa0f131a));
    g.fillRoundedRectangle(legend, 4.0f);

    float legX = 18.0f;
    // Input item
    g.setColour(juce::Colour(0xff00d2ff));
    g.fillEllipse(legX, 18.0f, 7.0f, 7.0f);
    legX += 12.0f;
    g.setColour(juce::Colour(0xffc5cee0));
    g.setFont(juce::Font(10.0f, juce::Font::plain));
    g.drawText("Input", (int)legX, 12, 35, 18, juce::Justification::left);
    legX += 45.0f;

    // Noise Profile item
    g.setColour(juce::Colour(0xffff9f1c));
    g.fillEllipse(legX, 18.0f, 7.0f, 7.0f);
    legX += 12.0f;
    g.setColour(juce::Colour(0xffc5cee0));
    g.drawText("Noise Profile", (int)legX, 12, 65, 18, juce::Justification::left);
    legX += 75.0f;

    // Denoised item
    g.setColour(juce::Colour(0xff00ff9d));
    g.fillEllipse(legX, 18.0f, 7.0f, 7.0f);
    legX += 12.0f;
    g.setColour(juce::Colour(0xffc5cee0));
    g.drawText("Denoised", (int)legX, 12, 50, 18, juce::Justification::left);
}

} // namespace gui
