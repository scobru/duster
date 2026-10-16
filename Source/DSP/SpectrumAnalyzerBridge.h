#pragma once

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <atomic>

namespace dsp
{

/**
 * Thread-safe, lock-free bridge to pass spectral magnitudes
 * from the real-time audio thread to the GUI spectrum analyzer.
 */
class SpectrumAnalyzerBridge
{
public:
    static constexpr int kMaxBins = 2048;

    SpectrumAnalyzerBridge()
    {
        inputMagnitudes.resize(kMaxBins, -100.0f);
        outputMagnitudes.resize(kMaxBins, -100.0f);
        noiseProfileMagnitudes.resize(kMaxBins, -100.0f);
        
        guiInputMagnitudes.resize(kMaxBins, -100.0f);
        guiOutputMagnitudes.resize(kMaxBins, -100.0f);
        guiNoiseProfileMagnitudes.resize(kMaxBins, -100.0f);
    }

    void pushInputSpectrum(const float* magnitudes, int numBins)
    {
        int count = std::min(numBins, kMaxBins);
        for (int i = 0; i < count; ++i)
            inputMagnitudes[i] = magnitudes[i];
        
        currentInputBins.store(count, std::memory_order_release);
        newInputDataReady.store(true, std::memory_order_release);
    }

    void pushOutputSpectrum(const float* magnitudes, int numBins)
    {
        int count = std::min(numBins, kMaxBins);
        for (int i = 0; i < count; ++i)
            outputMagnitudes[i] = magnitudes[i];
        
        currentOutputBins.store(count, std::memory_order_release);
        newOutputDataReady.store(true, std::memory_order_release);
    }

    void pushNoiseProfile(const float* magnitudes, int numBins)
    {
        int count = std::min(numBins, kMaxBins);
        for (int i = 0; i < count; ++i)
            noiseProfileMagnitudes[i] = magnitudes[i];
        
        currentNoiseBins.store(count, std::memory_order_release);
        newNoiseDataReady.store(true, std::memory_order_release);
    }

    bool fetchSpectra(std::vector<float>& destInput,
                      std::vector<float>& destOutput,
                      std::vector<float>& destNoise,
                      int& outNumBins)
    {
        bool hasNew = false;
        if (newInputDataReady.exchange(false, std::memory_order_acquire))
        {
            int bins = currentInputBins.load(std::memory_order_acquire);
            if (destInput.size() < (size_t)bins)
                destInput.resize(bins);
            std::copy(inputMagnitudes.begin(), inputMagnitudes.begin() + bins, destInput.begin());
            outNumBins = bins;
            hasNew = true;
        }

        if (newOutputDataReady.exchange(false, std::memory_order_acquire))
        {
            int bins = currentOutputBins.load(std::memory_order_acquire);
            if (destOutput.size() < (size_t)bins)
                destOutput.resize(bins);
            std::copy(outputMagnitudes.begin(), outputMagnitudes.begin() + bins, destOutput.begin());
            hasNew = true;
        }

        if (newNoiseDataReady.exchange(false, std::memory_order_acquire))
        {
            int bins = currentNoiseBins.load(std::memory_order_acquire);
            if (destNoise.size() < (size_t)bins)
                destNoise.resize(bins);
            std::copy(noiseProfileMagnitudes.begin(), noiseProfileMagnitudes.begin() + bins, destNoise.begin());
            hasNew = true;
        }

        return hasNew;
    }

private:
    std::vector<float> inputMagnitudes;
    std::vector<float> outputMagnitudes;
    std::vector<float> noiseProfileMagnitudes;

    std::vector<float> guiInputMagnitudes;
    std::vector<float> guiOutputMagnitudes;
    std::vector<float> guiNoiseProfileMagnitudes;

    std::atomic<int> currentInputBins { 0 };
    std::atomic<int> currentOutputBins { 0 };
    std::atomic<int> currentNoiseBins { 0 };

    std::atomic<bool> newInputDataReady { false };
    std::atomic<bool> newOutputDataReady { false };
    std::atomic<bool> newNoiseDataReady { false };
};

} // namespace dsp
