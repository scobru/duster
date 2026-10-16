#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include "../DSP/SpectrumAnalyzerBridge.h"

namespace gui
{

class SpectraVisualizer : public juce::Component, public juce::Timer
{
public:
    SpectraVisualizer(dsp::SpectrumAnalyzerBridge& bridge);
    ~SpectraVisualizer() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void setLearningState(bool learning) { isLearning = learning; repaint(); }
    void setProfileReady(bool ready) { hasProfile = ready; repaint(); }

private:
    float mapFreqToX(float freq, float width) const;
    float mapDbToY(float db, float height) const;

    dsp::SpectrumAnalyzerBridge& spectrumBridge;

    std::vector<float> inputMagnitudes;
    std::vector<float> outputMagnitudes;
    std::vector<float> noiseProfileMagnitudes;

    std::vector<float> smoothedInput;
    std::vector<float> smoothedOutput;

    int numBins { 1025 };
    float sampleRate { 44100.0f };

    bool isLearning { false };
    bool hasProfile { false };
    float scanPhase { 0.0f };

    juce::Path inputPath;
    juce::Path outputPath;
    juce::Path noisePath;
};

} // namespace gui
