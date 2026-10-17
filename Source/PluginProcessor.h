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

private:
    juce::AudioProcessorValueTreeState apvts;
    dsp::DusterEngine denoiserEngine;
    dsp::SpectrumAnalyzerBridge spectrumBridge;

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
