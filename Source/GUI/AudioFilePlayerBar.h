#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

class DusterAudioProcessor;

namespace gui
{

class AudioFilePlayerBar : public juce::Component, public juce::Timer
{
public:
    explicit AudioFilePlayerBar(DusterAudioProcessor& processor);
    ~AudioFilePlayerBar() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void openFileDialog();
    void exportFileDialog();

private:
    DusterAudioProcessor& audioProcessor;

    juce::TextButton openButton;
    juce::TextButton playPauseButton;
    juce::TextButton stopButton;
    juce::TextButton loopButton;
    juce::Slider scrubSlider;
    juce::Label timeLabel;
    juce::Label fileInfoLabel;
    juce::TextButton exportButton;

    std::unique_ptr<juce::FileChooser> fileChooser;
    bool isUserDraggingScrubber { false };

    juce::String formatTime(double seconds) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioFilePlayerBar)
};

} // namespace gui
