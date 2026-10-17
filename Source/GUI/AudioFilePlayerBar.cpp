#include "AudioFilePlayerBar.h"
#include "../PluginProcessor.h"

namespace gui
{

AudioFilePlayerBar::AudioFilePlayerBar(DusterAudioProcessor& p)
    : audioProcessor(p)
{
    // Open Button
    openButton.setButtonText("📁 LOAD AUDIO");
    openButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    openButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff38bdf8));
    openButton.onClick = [this]() { openFileDialog(); };
    addAndMakeVisible(openButton);

    // Play / Pause Button
    playPauseButton.setButtonText("▶ PLAY");
    playPauseButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    playPauseButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff4ade80));
    playPauseButton.setEnabled(false);
    playPauseButton.onClick = [this]() { audioProcessor.togglePlayPause(); };
    addAndMakeVisible(playPauseButton);

    // Stop Button
    stopButton.setButtonText("⏹");
    stopButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    stopButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff94a3b8));
    stopButton.setEnabled(false);
    stopButton.onClick = [this]() { audioProcessor.stopAudioFile(); };
    addAndMakeVisible(stopButton);

    // Loop Toggle Button
    loopButton.setButtonText("🔁 LOOP");
    loopButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    loopButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff59e0b));
    loopButton.onClick = [this]() { audioProcessor.setLooping(!audioProcessor.isLooping()); };
    addAndMakeVisible(loopButton);

    // Scrub Slider
    scrubSlider.setSliderStyle(juce::Slider::LinearBar);
    scrubSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    scrubSlider.setRange(0.0, 1.0, 0.0001);
    scrubSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff0284c7));
    scrubSlider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff0f172a));
    scrubSlider.onDragStart = [this]() { isUserDraggingScrubber = true; };
    scrubSlider.onDragEnd = [this]()
    {
        isUserDraggingScrubber = false;
        audioProcessor.setPlayheadNormalized(scrubSlider.getValue());
    };
    scrubSlider.onValueChange = [this]()
    {
        if (isUserDraggingScrubber)
            audioProcessor.setPlayheadNormalized(scrubSlider.getValue());
    };
    addAndMakeVisible(scrubSlider);

    // Time Label
    timeLabel.setText("00:00 / 00:00", juce::dontSendNotification);
    timeLabel.setFont(juce::Font(11.0f, juce::Font::plain));
    timeLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    timeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(timeLabel);

    // File Info Label
    fileInfoLabel.setText("No file loaded • Drag & drop audio (.wav, .mp3, .flac) or click Load", juce::dontSendNotification);
    fileInfoLabel.setFont(juce::Font(10.5f, juce::Font::plain));
    fileInfoLabel.setColour(juce::Label::textColourId, juce::Colour(0xff64748b));
    fileInfoLabel.setJustificationType(juce::Justification::left);
    addAndMakeVisible(fileInfoLabel);

    // Export Button
    exportButton.setButtonText("💾 EXPORT WAV");
    exportButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    exportButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe2e8f0));
    exportButton.setEnabled(false);
    exportButton.onClick = [this]() { exportFileDialog(); };
    addAndMakeVisible(exportButton);

    startTimerHz(25);
}

AudioFilePlayerBar::~AudioFilePlayerBar()
{
    stopTimer();
}

void AudioFilePlayerBar::openFileDialog()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select an Audio File to Denoise...",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*.wav;*.mp3;*.flac;*.aiff;*.aif;*.ogg");

    auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
    {
        auto result = fc.getResult();
        if (result.existsAsFile())
        {
            if (audioProcessor.loadAudioFile(result))
            {
                audioProcessor.playAudioFile();
            }
        }
    });
}

void AudioFilePlayerBar::exportFileDialog()
{
    if (!audioProcessor.hasFileLoaded())
        return;

    juce::String defaultName = audioProcessor.getLoadedFileName();
    if (defaultName.contains("."))
        defaultName = defaultName.upToLastOccurrenceOf(".", false, false) + "_denoised.wav";
    else
        defaultName += "_denoised.wav";

    fileChooser = std::make_unique<juce::FileChooser>(
        "Save Denoised Audio File...",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(defaultName),
        "*.wav");

    auto chooserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting;

    fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
    {
        auto result = fc.getResult();
        if (result != juce::File())
        {
            exportButton.setEnabled(false);
            exportButton.setButtonText("EXPORTING...");

            // Run offline export in background thread
            juce::Thread::launch([this, result]()
            {
                bool success = audioProcessor.exportDenoisedFile(result, [this](float progress)
                {
                    juce::MessageManager::callAsync([this, progress]()
                    {
                        int pct = (int)(progress * 100.0f);
                        exportButton.setButtonText(juce::String(pct) + "%");
                    });
                });

                juce::MessageManager::callAsync([this, success, result]()
                {
                    exportButton.setEnabled(true);
                    exportButton.setButtonText("💾 EXPORT WAV");
                    if (success)
                    {
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::AlertWindow::InfoIcon,
                            "Export Complete",
                            "Denoised file successfully saved to:\n" + result.getFullPathName());
                    }
                    else
                    {
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::AlertWindow::WarningIcon,
                            "Export Failed",
                            "Could not write denoised audio file.");
                    }
                });
            });
        }
    });
}

juce::String AudioFilePlayerBar::formatTime(double seconds) const
{
    if (seconds < 0.0)
        seconds = 0.0;
    int totalSec = (int)seconds;
    int mins = totalSec / 60;
    int secs = totalSec % 60;
    return juce::String::formatted("%02d:%02d", mins, secs);
}

void AudioFilePlayerBar::timerCallback()
{
    bool hasFile = audioProcessor.hasFileLoaded();
    bool isPlaying = audioProcessor.isFilePlaying();
    bool isLooping = audioProcessor.isLooping();

    playPauseButton.setEnabled(hasFile);
    stopButton.setEnabled(hasFile);
    exportButton.setEnabled(hasFile);
    scrubSlider.setEnabled(hasFile);

    playPauseButton.setButtonText(isPlaying ? "⏸ PAUSE" : "▶ PLAY");
    playPauseButton.setColour(juce::TextButton::textColourOffId,
                              isPlaying ? juce::Colour(0xff38bdf8) : juce::Colour(0xff4ade80));

    loopButton.setColour(juce::TextButton::buttonColourId,
                         isLooping ? juce::Colour(0xff451a03) : juce::Colour(0xff1e293b));
    loopButton.setColour(juce::TextButton::textColourOffId,
                         isLooping ? juce::Colour(0xfffbbf24) : juce::Colour(0xff64748b));

    if (!isUserDraggingScrubber && hasFile)
    {
        scrubSlider.setValue(audioProcessor.getPlayheadNormalized(), juce::dontSendNotification);
    }

    if (hasFile)
    {
        timeLabel.setText(formatTime(audioProcessor.getPlayheadTimeSeconds()) + " / " + formatTime(audioProcessor.getTotalTimeSeconds()),
                          juce::dontSendNotification);
        fileInfoLabel.setText(audioProcessor.getLoadedFileName() + " • Ready for Denoising",
                              juce::dontSendNotification);
        fileInfoLabel.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
    }
    else
    {
        timeLabel.setText("00:00 / 00:00", juce::dontSendNotification);
        fileInfoLabel.setText("No file loaded • Drag & drop audio (.wav, .mp3, .flac) or click Load",
                              juce::dontSendNotification);
        fileInfoLabel.setColour(juce::Label::textColourId, juce::Colour(0xff64748b));
    }
}

void AudioFilePlayerBar::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Container background
    g.setColour(juce::Colour(0xff111827));
    g.fillRoundedRectangle(bounds, 6.0f);

    g.setColour(juce::Colour(0xff1f2937));
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
}

void AudioFilePlayerBar::resized()
{
    auto bounds = getLocalBounds().reduced(6);

    // Left controls
    openButton.setBounds(bounds.removeFromLeft(95));
    bounds.removeFromLeft(6);

    playPauseButton.setBounds(bounds.removeFromLeft(72));
    bounds.removeFromLeft(4);

    stopButton.setBounds(bounds.removeFromLeft(30));
    bounds.removeFromLeft(4);

    loopButton.setBounds(bounds.removeFromLeft(65));
    bounds.removeFromLeft(10);

    // Right controls
    exportButton.setBounds(bounds.removeFromRight(105));
    bounds.removeFromRight(8);

    timeLabel.setBounds(bounds.removeFromRight(95));
    bounds.removeFromRight(8);

    // Center area: Scrubber on top half, File Info on bottom half
    auto centerArea = bounds;
    scrubSlider.setBounds(centerArea.removeFromTop(18));
    centerArea.removeFromTop(2);
    fileInfoLabel.setBounds(centerArea);
}

} // namespace gui
