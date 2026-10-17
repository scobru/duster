#include "PluginProcessor.h"
#include "PluginEditor.h"

DusterAudioProcessorEditor::DusterAudioProcessorEditor(DusterAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      playerBar(p),
      visualizer(p.getSpectrumBridge()),
      reductionSlider("REDUCTION", "dB", juce::Colour(0xffff9f1c)),
      thresholdSlider("THRESHOLD", "dB", juce::Colour(0xff00d2ff)),
      residualSmoothSlider("RESIDUAL SMOOTH", "%", juce::Colour(0xffc084fc)),
      transientBoostSlider("TRANSIENT BOOST", "%", juce::Colour(0xff34d399)),
      mixSlider("MIX", "%", juce::Colour(0xfff8fafc))
{
    setSize(840, 630);

    // Audio File Player Bar
    addAndMakeVisible(playerBar);

    // Visualizer
    addAndMakeVisible(visualizer);

    // Learn Button
    learnButton.setButtonText("● LEARN NOISE");
    learnButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a1810));
    learnButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff7f1d1d));
    learnButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffa600));
    learnButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    learnButton.onClick = [this]()
    {
        bool currentState = audioProcessor.getAPVTS().getRawParameterValue("learn")->load() > 0.5f;
        audioProcessor.toggleLearning(!currentState);
    };
    addAndMakeVisible(learnButton);

    // Clear Button
    clearButton.setButtonText("CLEAR");
    clearButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e2430));
    clearButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff94a3b8));
    clearButton.onClick = [this]()
    {
        audioProcessor.clearProfile();
    };
    addAndMakeVisible(clearButton);

    // Sliders
    addAndMakeVisible(reductionSlider);
    addAndMakeVisible(thresholdSlider);
    addAndMakeVisible(residualSmoothSlider);
    addAndMakeVisible(transientBoostSlider);
    addAndMakeVisible(mixSlider);

    // Attachments
    auto& apvts = audioProcessor.getAPVTS();
    reductionAttachment      = std::make_unique<SliderAttachment>(apvts, "reduction", reductionSlider.getSlider());
    thresholdAttachment      = std::make_unique<SliderAttachment>(apvts, "threshold", thresholdSlider.getSlider());
    residualSmoothAttachment = std::make_unique<SliderAttachment>(apvts, "residual_smooth", residualSmoothSlider.getSlider());
    transientBoostAttachment = std::make_unique<SliderAttachment>(apvts, "transient_boost", transientBoostSlider.getSlider());
    mixAttachment            = std::make_unique<SliderAttachment>(apvts, "mix", mixSlider.getSlider());

    // Listen ComboBox
    listenLabel.setText("LISTEN MODE", juce::dontSendNotification);
    listenLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    listenLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(listenLabel);

    listenModeBox.addItem("Denoised", 1);
    listenModeBox.addItem("Noise Only (Delta)", 2);
    listenModeBox.addItem("Bypass", 3);
    listenModeBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff181d28));
    listenModeBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff334155));
    listenModeBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
    addAndMakeVisible(listenModeBox);
    listenModeAttachment = std::make_unique<ComboBoxAttachment>(apvts, "listen_mode", listenModeBox);

    // Quality ComboBox
    qualityLabel.setText("QUALITY (FFT)", juce::dontSendNotification);
    qualityLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    qualityLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(qualityLabel);

    qualityBox.addItem("Fast (1024)", 1);
    qualityBox.addItem("Standard (2048)", 2);
    qualityBox.addItem("High (4096)", 3);
    qualityBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff181d28));
    qualityBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff334155));
    qualityBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
    addAndMakeVisible(qualityBox);
    qualityAttachment = std::make_unique<ComboBoxAttachment>(apvts, "quality", qualityBox);

    startTimerHz(25); // UI status polling
}

DusterAudioProcessorEditor::~DusterAudioProcessorEditor()
{
    stopTimer();
}

void DusterAudioProcessorEditor::timerCallback()
{
    bool learning = audioProcessor.getAPVTS().getRawParameterValue("learn")->load() > 0.5f;
    bool hasProfile = audioProcessor.getEngine().hasLearnedProfile();

    if (learning != isLearningActive)
    {
        isLearningActive = learning;
        visualizer.setLearningState(learning);

        if (learning)
        {
            learnButton.setButtonText("■ STOP LEARNING");
            learnButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffb91c1c));
            learnButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        }
        else
        {
            learnButton.setButtonText("● LEARN NOISE");
            learnButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a1810));
            learnButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffa600));
        }
    }

    visualizer.setProfileReady(hasProfile);

    // Update labels value display
    reductionSlider.updateValueText();
    thresholdSlider.updateValueText();
    residualSmoothSlider.updateValueText();
    transientBoostSlider.updateValueText();
    mixSlider.updateValueText();
}

void DusterAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // 1. Sleek Gradient Dark Background
    juce::ColourGradient bgGrad(juce::Colour(0xff141721), 0, 0,
                               juce::Colour(0xff090c12), 0, bounds.getBottom(), false);
    g.setGradientFill(bgGrad);
    g.fillAll();

    // 2. Header Branding
    g.setColour(juce::Colour(0xff00d2ff));
    g.setFont(juce::Font(20.0f, juce::Font::bold));
    g.drawText("DUSTER", 20, 14, 260, 24, juce::Justification::left);

    g.setColour(juce::Colour(0xff64748b));
    g.setFont(juce::Font(10.5f, juce::Font::plain));
    g.drawText("SPECTRAL SUBTRACTION NOISE SUPPRESSOR • STANDALONE / VST3", 20, 36, 420, 14, juce::Justification::left);

    // 3. Control Sections Boxes
    auto controlsArea = juce::Rectangle<float>(15.0f, 385.0f, bounds.getWidth() - 30.0f, 230.0f);

    // Left Box: Denoising Engine
    auto leftBox = controlsArea.removeFromLeft(545.0f);
    g.setColour(juce::Colour(0xff12151f));
    g.fillRoundedRectangle(leftBox, 8.0f);
    g.setColour(juce::Colour(0xff222938));
    g.drawRoundedRectangle(leftBox, 8.0f, 1.2f);

    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.setColour(juce::Colour(0xff94a3b8));
    g.drawText("DENOISING & ARTIFACT SUPPRESSION", (int)leftBox.getX() + 16, (int)leftBox.getY() + 10, 250, 16, juce::Justification::left);

    // Right Box: Output & Audition
    controlsArea.removeFromLeft(15.0f); // Gap
    auto rightBox = controlsArea;
    g.setColour(juce::Colour(0xff12151f));
    g.fillRoundedRectangle(rightBox, 8.0f);
    g.setColour(juce::Colour(0xff222938));
    g.drawRoundedRectangle(rightBox, 8.0f, 1.2f);

    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.setColour(juce::Colour(0xff94a3b8));
    g.drawText("OUTPUT & AUDITION", (int)rightBox.getX() + 16, (int)rightBox.getY() + 10, 200, 16, juce::Justification::left);

    // Drag-and-Drop Overlay Highlight
    if (isDraggingOver)
    {
        g.setColour(juce::Colour(0xd00284c7));
        g.fillRoundedRectangle(bounds.reduced(10.0f), 12.0f);

        g.setColour(juce::Colour(0xff38bdf8));
        g.drawRoundedRectangle(bounds.reduced(10.0f), 12.0f, 2.5f);

        g.setFont(juce::Font(22.0f, juce::Font::bold));
        g.setColour(juce::Colours::white);
        g.drawText("DROP AUDIO FILE HERE TO LOAD AND DENOISE", bounds, juce::Justification::centred);
    }
}

void DusterAudioProcessorEditor::resized()
{
    // Header Buttons
    learnButton.setBounds(getWidth() - 225, 14, 140, 32);
    clearButton.setBounds(getWidth() - 75, 14, 55, 32);

    // Player Bar
    playerBar.setBounds(15, 54, getWidth() - 30, 44);

    // Visualizer
    visualizer.setBounds(15, 106, getWidth() - 30, 265);

    // Bottom Sliders in Left Box
    const int knobY = 415;
    const int knobW = 125;
    const int knobH = 175;

    reductionSlider.setBounds(25, knobY, knobW, knobH);
    thresholdSlider.setBounds(155, knobY, knobW, knobH);
    residualSmoothSlider.setBounds(285, knobY, knobW, knobH);
    transientBoostSlider.setBounds(415, knobY, knobW, knobH);

    // Right Box Components
    const int rightX = 585;
    mixSlider.setBounds(rightX + 5, knobY, 105, knobH);

    // Combo boxes
    const int comboX = rightX + 115;
    listenLabel.setBounds(comboX, 415, 100, 16);
    listenModeBox.setBounds(comboX, 435, 105, 28);

    qualityLabel.setBounds(comboX, 490, 100, 16);
    qualityBox.setBounds(comboX, 510, 105, 28);
}

bool DusterAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& file : files)
    {
        juce::String ext = juce::File(file).getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".mp3" || ext == ".flac" || ext == ".aiff" || ext == ".aif" || ext == ".ogg")
            return true;
    }
    return false;
}

void DusterAudioProcessorEditor::fileDragEnter(const juce::StringArray&, int, int)
{
    isDraggingOver = true;
    repaint();
}

void DusterAudioProcessorEditor::fileDragExit(const juce::StringArray&)
{
    isDraggingOver = false;
    repaint();
}

void DusterAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int)
{
    isDraggingOver = false;
    repaint();

    for (const auto& filePath : files)
    {
        juce::File file(filePath);
        if (file.existsAsFile())
        {
            if (audioProcessor.loadAudioFile(file))
            {
                audioProcessor.playAudioFile();
                break;
            }
        }
    }
}
