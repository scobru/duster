#include "PluginProcessor.h"
#include "PluginEditor.h"

SpectraDenoiserAudioProcessorEditor::SpectraDenoiserAudioProcessorEditor(SpectraDenoiserAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      visualizer(p.getSpectrumBridge()),
      reductionSlider("REDUCTION", "dB", juce::Colour(0xffff9f1c)),
      thresholdSlider("THRESHOLD", "dB", juce::Colour(0xff00d2ff)),
      residualSmoothSlider("RESIDUAL SMOOTH", "%", juce::Colour(0xffc084fc)),
      transientBoostSlider("TRANSIENT BOOST", "%", juce::Colour(0xff34d399)),
      mixSlider("MIX", "%", juce::Colour(0xfff8fafc))
{
    setSize(820, 570);

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

SpectraDenoiserAudioProcessorEditor::~SpectraDenoiserAudioProcessorEditor()
{
    stopTimer();
}

void SpectraDenoiserAudioProcessorEditor::timerCallback()
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

void SpectraDenoiserAudioProcessorEditor::paint(juce::Graphics& g)
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
    g.drawText("SPECTRA DENOISER", 20, 16, 260, 24, juce::Justification::left);

    g.setColour(juce::Colour(0xff64748b));
    g.setFont(juce::Font(10.5f, juce::Font::plain));
    g.drawText("SPECTRAL SUBTRACTION NOISE SUPPRESSOR • VST3 / AU", 20, 39, 340, 14, juce::Justification::left);

    // 3. Control Sections Boxes
    auto controlsArea = juce::Rectangle<float>(15.0f, 340.0f, bounds.getWidth() - 30.0f, 215.0f);

    // Left Box: Denoising Engine
    auto leftBox = controlsArea.removeFromLeft(535.0f);
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
}

void SpectraDenoiserAudioProcessorEditor::resized()
{
    // Header Buttons
    learnButton.setBounds(getWidth() - 225, 18, 140, 32);
    clearButton.setBounds(getWidth() - 75, 18, 55, 32);

    // Visualizer
    visualizer.setBounds(15, 62, getWidth() - 30, 265);

    // Bottom Sliders in Left Box
    const int knobY = 370;
    const int knobW = 120;
    const int knobH = 165;

    reductionSlider.setBounds(25, knobY, knobW, knobH);
    thresholdSlider.setBounds(155, knobY, knobW, knobH);
    residualSmoothSlider.setBounds(285, knobY, knobW, knobH);
    transientBoostSlider.setBounds(415, knobY, knobW, knobH);

    // Right Box Components
    const int rightX = 575;
    mixSlider.setBounds(rightX + 10, knobY, 100, knobH);

    // Combo boxes
    const int comboX = rightX + 125;
    listenLabel.setBounds(comboX, 365, 100, 16);
    listenModeBox.setBounds(comboX, 385, 95, 28);

    qualityLabel.setBounds(comboX, 435, 100, 16);
    qualityBox.setBounds(comboX, 455, 95, 28);
}
