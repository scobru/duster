#include "PluginProcessor.h"
#include "PluginEditor.h"

DusterAudioProcessor::DusterAudioProcessor()
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
    formatManager.registerBasicFormats();
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

DusterAudioProcessor::~DusterAudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout DusterAudioProcessor::createParameterLayout()
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

const juce::String DusterAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool DusterAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool DusterAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool DusterAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double DusterAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int DusterAudioProcessor::getNumPrograms()
{
    return 1;
}

int DusterAudioProcessor::getCurrentProgram()
{
    return 0;
}

void DusterAudioProcessor::setCurrentProgram(int)
{
}

const juce::String DusterAudioProcessor::getProgramName(int)
{
    return {};
}

void DusterAudioProcessor::changeProgramName(int, const juce::String&)
{
}

void DusterAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    denoiserEngine.prepare(sampleRate, samplesPerBlock, getTotalNumInputChannels());
    setLatencySamples(denoiserEngine.getLatencySamples());
}

void DusterAudioProcessor::releaseResources()
{
    denoiserEngine.reset();
}

bool DusterAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
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

void DusterAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // If an audio file is playing, stream it into the buffer
    if (fileIsPlaying.load(std::memory_order_relaxed) && fileIsLoaded.load(std::memory_order_relaxed))
    {
        const juce::ScopedTryLock sl(fileAudioLock);
        if (sl.isLocked() && fileAudioBuffer.getNumSamples() > 0)
        {
            const int numSamples = buffer.getNumSamples();
            const int totalFileSamples = fileAudioBuffer.getNumSamples();
            const int fileChannels = fileAudioBuffer.getNumChannels();
            const int outChannels = buffer.getNumChannels();
            int64_t currentPos = filePlayhead.load(std::memory_order_relaxed);

            for (int s = 0; s < numSamples; ++s)
            {
                if (currentPos >= totalFileSamples)
                {
                    if (fileIsLooping.load(std::memory_order_relaxed))
                    {
                        currentPos = 0;
                    }
                    else
                    {
                        fileIsPlaying.store(false, std::memory_order_relaxed);
                        for (int ch = 0; ch < outChannels; ++ch)
                            buffer.setSample(ch, s, 0.0f);
                        continue;
                    }
                }

                for (int ch = 0; ch < outChannels; ++ch)
                {
                    int srcCh = std::min(ch, fileChannels - 1);
                    float smp = fileAudioBuffer.getSample(srcCh, (int)currentPos);
                    buffer.setSample(ch, s, smp);
                }
                currentPos++;
            }
            filePlayhead.store(currentPos, std::memory_order_relaxed);
        }
    }

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

void DusterAudioProcessor::toggleLearning(bool shouldLearn)
{
    if (auto* param = apvts.getParameter("learn"))
    {
        param->setValueNotifyingHost(shouldLearn ? 1.0f : 0.0f);
    }
}

void DusterAudioProcessor::clearProfile()
{
    denoiserEngine.clearNoiseProfile();
    toggleLearning(false);
}

bool DusterAudioProcessor::loadAudioFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr)
        return false;

    stopAudioFile();

    const int numChannels = (int)reader->numChannels;
    const int64_t numSamples = (int64_t)reader->lengthInSamples;
    const double readerSampleRate = reader->sampleRate;

    if (numSamples <= 0 || numChannels <= 0)
        return false;

    juce::AudioBuffer<float> tempBuffer(numChannels, (int)numSamples);
    reader->read(&tempBuffer, 0, (int)numSamples, 0, true, true);

    double currentRate = getSampleRate();
    double targetRate = (currentRate > 8000.0) ? currentRate : readerSampleRate;

    if (std::abs(readerSampleRate - targetRate) > 1.0)
    {
        double ratio = targetRate / readerSampleRate;
        int targetLength = (int)std::ceil((double)numSamples * ratio);
        juce::AudioBuffer<float> resampled(numChannels, targetLength);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            juce::LagrangeInterpolator interpolator;
            interpolator.process(ratio,
                                 tempBuffer.getReadPointer(ch),
                                 resampled.getWritePointer(ch),
                                 targetLength);
        }

        const juce::ScopedLock sl(fileAudioLock);
        fileAudioBuffer = std::move(resampled);
        fileSampleRate = targetRate;
    }
    else
    {
        const juce::ScopedLock sl(fileAudioLock);
        fileAudioBuffer = std::move(tempBuffer);
        fileSampleRate = readerSampleRate;
    }

    loadedFileName = file.getFileName();
    fileTotalSamples.store(fileAudioBuffer.getNumSamples());
    filePlayhead.store(0);
    fileIsLoaded.store(true);

    return true;
}

void DusterAudioProcessor::playAudioFile()
{
    if (fileIsLoaded.load())
        fileIsPlaying.store(true);
}

void DusterAudioProcessor::pauseAudioFile()
{
    fileIsPlaying.store(false);
}

void DusterAudioProcessor::stopAudioFile()
{
    fileIsPlaying.store(false);
    filePlayhead.store(0);
}

void DusterAudioProcessor::togglePlayPause()
{
    if (!fileIsLoaded.load())
        return;
    fileIsPlaying.store(!fileIsPlaying.load());
}

void DusterAudioProcessor::setLooping(bool shouldLoop)
{
    fileIsLooping.store(shouldLoop);
}

void DusterAudioProcessor::setPlayheadNormalized(double normPos)
{
    int64_t total = fileTotalSamples.load();
    if (total > 0)
    {
        normPos = juce::jlimit(0.0, 1.0, normPos);
        filePlayhead.store((int64_t)(normPos * (double)total));
    }
}

double DusterAudioProcessor::getPlayheadNormalized() const
{
    int64_t total = fileTotalSamples.load();
    if (total <= 0)
        return 0.0;
    return juce::jlimit(0.0, 1.0, (double)filePlayhead.load() / (double)total);
}

double DusterAudioProcessor::getPlayheadTimeSeconds() const
{
    if (fileSampleRate <= 0.0)
        return 0.0;
    return (double)filePlayhead.load() / fileSampleRate;
}

double DusterAudioProcessor::getTotalTimeSeconds() const
{
    if (fileSampleRate <= 0.0)
        return 0.0;
    return (double)fileTotalSamples.load() / fileSampleRate;
}

juce::String DusterAudioProcessor::getLoadedFileName() const
{
    return loadedFileName;
}

bool DusterAudioProcessor::isFilePlaying() const
{
    return fileIsPlaying.load();
}

bool DusterAudioProcessor::hasFileLoaded() const
{
    return fileIsLoaded.load();
}

bool DusterAudioProcessor::isLooping() const
{
    return fileIsLooping.load();
}

bool DusterAudioProcessor::exportDenoisedFile(const juce::File& destinationFile,
                                              std::function<void(float)> progressCallback)
{
    juce::AudioBuffer<float> sourceCopy;
    double exportRate = 44100.0;
    {
        const juce::ScopedLock sl(fileAudioLock);
        if (!fileIsLoaded.load() || fileAudioBuffer.getNumSamples() == 0)
            return false;
        sourceCopy = fileAudioBuffer;
        exportRate = fileSampleRate;
    }

    dsp::DusterEngine exportEngine;
    exportEngine.prepare(exportRate, 1024, sourceCopy.getNumChannels());

    dsp::DenoiserParameters params;
    params.reductionDb    = reductionParam->load();
    params.thresholdDb    = thresholdParam->load();
    params.residualSmooth = residualSmoothParam->load();
    params.transientBoost = transientBoostParam->load();
    params.mix            = mixParam->load();
    params.listenMode     = static_cast<dsp::ListenMode>((int)listenModeParam->load());
    params.quality        = static_cast<dsp::QualityMode>((int)qualityParam->load());
    exportEngine.setParameters(params);

    juce::String profileData = denoiserEngine.getSerializedProfile();
    if (profileData.isNotEmpty())
        exportEngine.restoreProfileFromString(profileData);

    const int exportBlockSize = 1024;
    const int totalSamples = sourceCopy.getNumSamples();
    juce::AudioBuffer<float> outputBuffer(sourceCopy.getNumChannels(), totalSamples);

    int processed = 0;
    while (processed < totalSamples)
    {
        int chunk = std::min(exportBlockSize, totalSamples - processed);
        juce::AudioBuffer<float> block(sourceCopy.getNumChannels(), chunk);
        for (int ch = 0; ch < sourceCopy.getNumChannels(); ++ch)
            block.copyFrom(ch, 0, sourceCopy, ch, processed, chunk);

        exportEngine.process(block);

        for (int ch = 0; ch < sourceCopy.getNumChannels(); ++ch)
            outputBuffer.copyFrom(ch, processed, block, ch, 0, chunk);

        processed += chunk;
        if (progressCallback)
            progressCallback((float)processed / (float)totalSamples);
    }

    destinationFile.deleteFile();
    std::unique_ptr<juce::FileOutputStream> fileStream(destinationFile.createOutputStream());
    if (fileStream == nullptr)
        return false;

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(
        fileStream.get(), exportRate, (unsigned int)outputBuffer.getNumChannels(), 24, {}, 0));

    if (writer == nullptr)
        return false;

    fileStream.release(); // writer takes ownership
    writer->writeFromAudioSampleBuffer(outputBuffer, 0, outputBuffer.getNumSamples());
    return true;
}

bool DusterAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* DusterAudioProcessor::createEditor()
{
    return new DusterAudioProcessorEditor(*this);
}

void DusterAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("noiseProfileData", denoiserEngine.getSerializedProfile(), nullptr);

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void DusterAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new DusterAudioProcessor();
}
