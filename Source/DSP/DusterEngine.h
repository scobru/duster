#pragma once

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>
#include <memory>
#include "SpectrumAnalyzerBridge.h"

namespace dsp
{

enum class ListenMode
{
    Denoised = 0,
    NoiseOnly,
    Bypass
};

enum class QualityMode
{
    Fast = 0,     // 1024 FFT
    Standard,     // 2048 FFT
    High          // 4096 FFT
};

struct DenoiserParameters
{
    float reductionDb { 20.0f };       // 0 to 40 dB
    float thresholdDb { 0.0f };        // -20 to +20 dB
    float residualSmooth { 0.70f };    // 0.0 to 1.0 (musical noise suppression)
    float transientBoost { 0.50f };    // 0.0 to 1.0 (attack preservation)
    float mix { 1.0f };                // 0.0 to 1.0 (Dry/Wet)
    ListenMode listenMode { ListenMode::Denoised };
    QualityMode quality { QualityMode::Standard };
    bool isLearning { false };
};

/**
 * High-performance Dual-Channel (Stereo) Spectral Subtraction Denoiser.
 * Uses WOLA (Weighted Overlap-Add) STFT with Decision-Directed Wiener
 * filtering and Spectral Subtraction for artifact-free noise reduction.
 */
class DusterEngine
{
public:
    DusterEngine();
    ~DusterEngine() = default;

    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    void setParameters(const DenoiserParameters& params);
    void process(juce::AudioBuffer<float>& buffer);

    // Noise profile learning control
    void startLearning();
    void stopLearning();
    void clearNoiseProfile();
    bool hasLearnedProfile() const { return profileReady; }
    int getLearnedFrameCount() const { return learnedFrameCount; }

    // Serialization for DAW state persistence
    juce::String getSerializedProfile() const;
    void restoreProfileFromString(const juce::String& base64);

    int getLatencySamples() const { return fftSize - hopSize; }

    void setSpectrumBridge(SpectrumAnalyzerBridge* bridge) { spectrumBridge = bridge; }

private:
    void updateFftSize(int newFftOrder);
    void processStftFrame(int channel);
    void applyDenoising(int channel);
    void computeTransientFlux(int channel);

    double currentSampleRate { 44100.0 };
    int currentChannels { 2 };
    int maxBlock { 512 };

    // FFT configuration
    int fftOrder { 11 };       // 2^11 = 2048 (Standard)
    int fftSize { 2048 };
    int hopSize { 512 };        // 75% overlap
    int numBins { 1025 };       // fftSize / 2 + 1

    std::unique_ptr<juce::dsp::FFT> forwardFFT;
    std::unique_ptr<juce::dsp::FFT> inverseFFT;
    std::vector<float> window;
    float windowGainCompensation { 1.0f };

    // Ring buffers for STFT overlap-add
    struct ChannelBuffers
    {
        std::vector<float> inputFifo;
        int inputFifoIndex { 0 };

        std::vector<float> outputFifo;
        int outputFifoIndex { 0 };

        std::vector<float> delayFifo;
        int delayFifoIndex { 0 };

        // Time domain FFT scratchpads (size 2 * fftSize)
        std::vector<float> fftTimeDomain;
        std::vector<float> ifftTimeDomain;

        // Spectral representations
        std::vector<float> magnitude;
        std::vector<float> phase;
        std::vector<float> prevMagnitude;
        std::vector<float> prevEstimatedCleanMag;

        // Spectral gains
        std::vector<float> spectralGain;

        // Transient tracking
        float prevEnergySum { 0.0f };
        float transientOnset { 0.0f };
    };

    std::vector<ChannelBuffers> channels;

    // Noise Profile (shared across channels or averaged)
    std::vector<float> noiseProfile;          // Linear magnitude per bin
    std::vector<float> noiseAccumulator;      // Sum for averaging
    std::vector<float> noiseVariance;         // Variance estimate
    int learnedFrameCount { 0 };
    bool isLearningActive { false };
    bool profileReady { false };

    // Parameters
    DenoiserParameters currentParams;

    // UI Bridge
    SpectrumAnalyzerBridge* spectrumBridge { nullptr };

    // Temp buffers for thread-safe spectrum sharing
    std::vector<float> tempInputMag;
    std::vector<float> tempOutputMag;
    std::vector<float> tempProfileMag;
};

} // namespace dsp
