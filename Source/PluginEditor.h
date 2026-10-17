#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "GUI/DusterVisualizer.h"
#include "GUI/CustomRotarySlider.h"

class DusterAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   public juce::Timer
{
public:
    DusterAudioProcessorEditor(DusterAudioProcessor&);
    ~DusterAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    DusterAudioProcessor& audioProcessor;

    // Visualizer
    gui::DusterVisualizer visualizer;

    // Header Controls
    juce::TextButton learnButton;
    juce::TextButton clearButton;

    // Rotary Knobs
    gui::CustomRotarySlider reductionSlider;
    gui::CustomRotarySlider thresholdSlider;
    gui::CustomRotarySlider residualSmoothSlider;
    gui::CustomRotarySlider transientBoostSlider;
    gui::CustomRotarySlider mixSlider;

    // Dropdown / Segment selectors
    juce::ComboBox listenModeBox;
    juce::ComboBox qualityBox;

    juce::Label listenLabel;
    juce::Label qualityLabel;

    // APVTS Attachments
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> reductionAttachment;
    std::unique_ptr<SliderAttachment> thresholdAttachment;
    std::unique_ptr<SliderAttachment> residualSmoothAttachment;
    std::unique_ptr<SliderAttachment> transientBoostAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;

    std::unique_ptr<ComboBoxAttachment> listenModeAttachment;
    std::unique_ptr<ComboBoxAttachment> qualityAttachment;

    bool isLearningActive { false };
    float learnGlowPulse { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DusterAudioProcessorEditor)
};
