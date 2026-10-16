#include "PluginProcessor.h"
#include "PluginEditor.h"

SpectraDenoiserAudioProcessor::SpectraDenoiserAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
    denoiserEngine.setSpectrumBridge(&spectrumBridge);

    reductionParam      = apvts.getRawParameterValue("reduction");
    thresholdParam      = apvts.getRawParameterValue("threshold");
    residualSmoothParam = apvts.getRawParameterValue("residual_smooth");
    transientBoostParam = apvts.getRawParameterValue("transient_boost");
    mixParam            = apvts.getRawParameterValue("mix");
    listenModeParam     = apvts.getRawParameterValue("listen_mode");
    qualityParam        = apvts.getRawParameterValue("quality");
    learnParam          = apvts.getRawParameterValue("learn");
}

SpectraDenoiserAudioProcessor::~SpectraDenoiserAudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout SpectraDenoiserAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Noise Reduction in dB (0 to 40 dB)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "reduction", 1 },
        "Reduction",
        juce::NormalisableRange<float>(0.0f, 40.0f, 0.1f),
        18.0f
    ));

    // Threshold Offset in dB (-18 to +18 dB)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "threshold", 1 },
        "Threshold",
        juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f),
        0.0f
    ));

    // Residual Smoothing / Musical Noise Removal (0 to 100%)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "residual_smooth", 1 },
        "Residual Smooth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.70f
    ));

    // Transient Boost / Attack Preservation (0 to 100%)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "transient_boost", 1 },
        "Transient Boost",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.45f
    ));

    // Dry/Wet Mix (0 to 100%)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "mix", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        1.0f
    ));

    // Listen Mode: Denoised, Noise Delta, Bypass
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "listen_mode", 1 },
        "Listen",
        juce::StringArray { "Denoised", "Noise Delta", "Bypass" },
        0
    ));

    // Quality: Fast, Standard, High
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "quality", 1 },
        "Quality",
        juce::StringArray { "Fast (1024)", "Standard (2048)", "High (4096)" },
        1
    ));

    // Learn Mode (Toggle)
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "learn", 1 },
        "Learn",
        false
    ));

    return { params.begin(), params.end() };
}

const juce::String SpectraDenoiserAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SpectraDenoiserAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SpectraDenoiserAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SpectraDenoiserAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SpectraDenoiserAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SpectraDenoiserAudioProcessor::getNumPrograms()
{
    return 1;
}

int SpectraDenoiserAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SpectraDenoiserAudioProcessor::setCurrentProgram(int)
{
}

const juce::String SpectraDenoiserAudioProcessor::getProgramName(int)
{
    return {};
}

void SpectraDenoiserAudioProcessor::changeProgramName(int, const juce::String&)
{
}

void SpectraDenoiserAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    denoiserEngine.prepare(sampleRate, samplesPerBlock, getTotalNumInputChannels());
    setLatencySamples(denoiserEngine.getLatencySamples());
}

void SpectraDenoiserAudioProcessor::releaseResources()
{
    denoiserEngine.reset();
}

bool SpectraDenoiserAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void SpectraDenoiserAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Update DSP parameters
    dsp::DenoiserParameters params;
    params.reductionDb    = reductionParam->load();
    params.thresholdDb    = thresholdParam->load();
    params.residualSmooth = residualSmoothParam->load();
    params.transientBoost = transientBoostParam->load();
    params.mix            = mixParam->load();

    int listenIdx = (int)listenModeParam->load();
    params.listenMode = static_cast<dsp::ListenMode>(listenIdx);

    int qualityIdx = (int)qualityParam->load();
    params.quality = static_cast<dsp::QualityMode>(qualityIdx);

    params.isLearning = (learnParam->load() > 0.5f);

    denoiserEngine.setParameters(params);

    // Process audio buffer through spectral denoiser
    denoiserEngine.process(buffer);

    // Keep DAW delay compensation updated
    if (getLatencySamples() != denoiserEngine.getLatencySamples())
        setLatencySamples(denoiserEngine.getLatencySamples());
}

void SpectraDenoiserAudioProcessor::toggleLearning(bool shouldLearn)
{
    if (auto* param = apvts.getParameter("learn"))
    {
        param->setValueNotifyingHost(shouldLearn ? 1.0f : 0.0f);
    }
}

void SpectraDenoiserAudioProcessor::clearProfile()
{
    denoiserEngine.clearNoiseProfile();
    toggleLearning(false);
}

bool SpectraDenoiserAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* SpectraDenoiserAudioProcessor::createEditor()
{
    return new SpectraDenoiserAudioProcessorEditor(*this);
}

void SpectraDenoiserAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("noiseProfileData", denoiserEngine.getSerializedProfile(), nullptr);

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SpectraDenoiserAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
    {
        if (xmlState->hasTagName(apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml(*xmlState);
            apvts.replaceState(tree);

            if (tree.hasProperty("noiseProfileData"))
            {
                juce::String profileBase64 = tree.getProperty("noiseProfileData").toString();
                denoiserEngine.restoreProfileFromString(profileBase64);
            }
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpectraDenoiserAudioProcessor();
}
