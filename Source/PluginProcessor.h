#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/DusterEngine.h"
#include "DSP/SpectrumAnalyzerBridge.h"

class DusterAudioProcessor : public juce::AudioProcessor
{
public:
    DusterAudioProcessor();
    ~DusterAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // APVTS
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    dsp::DusterEngine& getEngine() { return denoiserEngine; }
    dsp::SpectrumAnalyzerBridge& getSpectrumBridge() { return spectrumBridge; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Trigger learn action
    void toggleLearning(bool shouldLearn);
    void clearProfile();

    // Audio File Player & Offline Processing (Standalone & DAW)
    bool loadAudioFile(const juce::File& file);
    void playAudioFile();
    void pauseAudioFile();
    void stopAudioFile();
    void togglePlayPause();
    void setLooping(bool shouldLoop);
    void setPlayheadNormalized(double normPos);

    double getPlayheadNormalized() const;
    double getPlayheadTimeSeconds() const;
    double getTotalTimeSeconds() const;
    juce::String getLoadedFileName() const;
    bool isFilePlaying() const;
    bool hasFileLoaded() const;
    bool isLooping() const;

    bool exportDenoisedFile(const juce::File& destinationFile,
                            std::function<void(float)> progressCallback = nullptr);

private:
    juce::AudioProcessorValueTreeState apvts;
    dsp::DusterEngine denoiserEngine;
    dsp::SpectrumAnalyzerBridge spectrumBridge;

    // Audio File playback components
    juce::AudioFormatManager formatManager;
    juce::CriticalSection fileAudioLock;
    juce::AudioBuffer<float> fileAudioBuffer;
    double fileSampleRate { 44100.0 };
    std::atomic<bool> fileIsLoaded { false };
    std::atomic<bool> fileIsPlaying { false };
    std::atomic<bool> fileIsLooping { true };
    std::atomic<int64_t> filePlayhead { 0 };
    std::atomic<int64_t> fileTotalSamples { 0 };
    juce::String loadedFileName;

    // Cached raw parameter pointers for audio thread performance
    std::atomic<float>* reductionParam { nullptr };
    std::atomic<float>* thresholdParam { nullptr };
    std::atomic<float>* residualSmoothParam { nullptr };
    std::atomic<float>* transientBoostParam { nullptr };
    std::atomic<float>* mixParam { nullptr };
    std::atomic<float>* listenModeParam { nullptr };
    std::atomic<float>* qualityParam { nullptr };
    std::atomic<float>* learnParam { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DusterAudioProcessor)
};
