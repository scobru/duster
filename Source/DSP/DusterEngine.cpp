#include "DusterEngine.h"

namespace dsp
{

DusterEngine::DusterEngine()
{
    updateFftSize(11); // 2048 Default Standard
}

void DusterEngine::prepare(double sampleRate, int maxBlockSize, int numChannels)
{
    currentSampleRate = sampleRate;
    currentChannels = std::max(1, numChannels);
    maxBlock = maxBlockSize;

    channels.resize(currentChannels);
    for (auto& ch : channels)
    {
        ch.inputHistory.assign(fftSize, 0.0f);

        ch.outputBuffer.assign(fftSize * 4, 0.0f);
        ch.outputReadIndex = 0;

        ch.delayBuffer.assign(fftSize * 4, 0.0f);
        ch.delayWriteIndex = 0;

        ch.fftTimeDomain.assign(fftSize * 2, 0.0f);

        ch.magnitude.assign(numBins, 0.0f);
        ch.phase.assign(numBins, 0.0f);
        ch.prevMagnitude.assign(numBins, 0.0f);
        ch.prevEstimatedCleanMag.assign(numBins, 0.0f);
        ch.spectralGain.assign(numBins, 1.0f);

        ch.prevEnergySum = 0.0f;
        ch.transientOnset = 0.0f;
    }

    hopCounter = 0;

    tempInputMag.assign(numBins, -100.0f);
    tempOutputMag.assign(numBins, -100.0f);
    tempProfileMag.assign(numBins, -100.0f);

    reset();
}

void DusterEngine::reset()
{
    for (auto& ch : channels)
    {
        std::fill(ch.inputHistory.begin(), ch.inputHistory.end(), 0.0f);
        std::fill(ch.outputBuffer.begin(), ch.outputBuffer.end(), 0.0f);
        ch.outputReadIndex = 0;

        std::fill(ch.delayBuffer.begin(), ch.delayBuffer.end(), 0.0f);
        ch.delayWriteIndex = 0;

        std::fill(ch.prevMagnitude.begin(), ch.prevMagnitude.end(), 0.0f);
        std::fill(ch.prevEstimatedCleanMag.begin(), ch.prevEstimatedCleanMag.end(), 0.0f);
        std::fill(ch.spectralGain.begin(), ch.spectralGain.end(), 1.0f);
        ch.prevEnergySum = 0.0f;
        ch.transientOnset = 0.0f;
    }
    hopCounter = 0;
}

void DusterEngine::updateFftSize(int newFftOrder)
{
    fftOrder = juce::jlimit(10, 12, newFftOrder); // 1024 to 4096
    fftSize = 1 << fftOrder;
    hopSize = fftSize / 4; // 75% overlap
    numBins = fftSize / 2 + 1;

    forwardFFT = std::make_unique<juce::dsp::FFT>(fftOrder);
    inverseFFT = std::make_unique<juce::dsp::FFT>(fftOrder);

    // Generate Hann analysis & synthesis window
    window.resize(fftSize);
    for (int i = 0; i < fftSize; ++i)
    {
        // Periodic Hann window
        window[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * (float)i / (float)fftSize));
    }

    // Hann squared 75% overlap sum = 1.5
    windowGainCompensation = 1.0f / 1.5f;

    noiseProfile.assign(numBins, 0.0f);
    noiseAccumulator.assign(numBins, 0.0f);
    noiseVariance.assign(numBins, 0.0f);

    if (currentChannels > 0)
        prepare(currentSampleRate, maxBlock, currentChannels);
}

void DusterEngine::setParameters(const DenoiserParameters& params)
{
    // Check if quality setting changed
    int targetOrder = 11;
    if (params.quality == QualityMode::Fast)
        targetOrder = 10; // 1024
    else if (params.quality == QualityMode::High)
        targetOrder = 12; // 4096

    if (targetOrder != fftOrder)
    {
        updateFftSize(targetOrder);
    }

    currentParams = params;

    if (params.isLearning && !isLearningActive)
    {
        startLearning();
    }
    else if (!params.isLearning && isLearningActive)
    {
        stopLearning();
    }
}

void DusterEngine::startLearning()
{
    isLearningActive = true;
    learnedFrameCount = 0;
    std::fill(noiseAccumulator.begin(), noiseAccumulator.end(), 0.0f);
}

void DusterEngine::stopLearning()
{
    if (isLearningActive && learnedFrameCount > 0)
    {
        float invCount = 1.0f / (float)learnedFrameCount;
        for (int i = 0; i < numBins; ++i)
        {
            noiseProfile[i] = noiseAccumulator[i] * invCount;
            // dB conversion for visualizer
            float db = juce::Decibels::gainToDecibels(noiseProfile[i] + 1e-9f, -100.0f);
            if (i < (int)tempProfileMag.size())
                tempProfileMag[i] = db;
        }

        profileReady = true;

        if (spectrumBridge != nullptr)
            spectrumBridge->pushNoiseProfile(tempProfileMag.data(), numBins);
    }

    isLearningActive = false;
}

void DusterEngine::clearNoiseProfile()
{
    std::fill(noiseProfile.begin(), noiseProfile.end(), 0.0f);
    std::fill(noiseAccumulator.begin(), noiseAccumulator.end(), 0.0f);
    profileReady = false;
    learnedFrameCount = 0;
    isLearningActive = false;

    if (spectrumBridge != nullptr)
    {
        std::vector<float> emptyProfile(numBins, -100.0f);
        spectrumBridge->pushNoiseProfile(emptyProfile.data(), numBins);
    }
}

void DusterEngine::process(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numActiveChannels = std::min(buffer.getNumChannels(), (int)channels.size());
    if (numActiveChannels <= 0)
        return;

    const int latencySamples = getLatencySamples();

    for (int smp = 0; smp < numSamples; ++smp)
    {
        for (int ch = 0; ch < numActiveChannels; ++ch)
        {
            auto& channel = channels[ch];
            const float inSample = buffer.getSample(ch, smp);

            // 1. Delay compensation ring buffer for time-aligned dry and noise delta
            const int delayBufSize = (int)channel.delayBuffer.size();
            channel.delayBuffer[channel.delayWriteIndex] = inSample;
            int delayedReadIndex = (channel.delayWriteIndex - latencySamples + delayBufSize) % delayBufSize;
            float delayedDrySample = channel.delayBuffer[delayedReadIndex];
            channel.delayWriteIndex = (channel.delayWriteIndex + 1) % delayBufSize;

            // 2. Retrieve sample from overlap-add output circular buffer and clear
            const int outBufSize = (int)channel.outputBuffer.size();
            float outSample = channel.outputBuffer[channel.outputReadIndex];
            channel.outputBuffer[channel.outputReadIndex] = 0.0f;
            channel.outputReadIndex = (channel.outputReadIndex + 1) % outBufSize;

            // 3. Store sample into new incoming hop section of input history
            channel.inputHistory[fftSize - hopSize + hopCounter] = inSample;

            // 4. Route audio according to Listen Mode and Mix
            float finalSample = outSample;

            if (currentParams.listenMode == ListenMode::NoiseOnly)
            {
                // Exact extracted noise (Delayed Dry - Denoised)
                finalSample = (delayedDrySample - outSample) * currentParams.mix;
            }
            else if (currentParams.listenMode == ListenMode::Bypass)
            {
                finalSample = delayedDrySample;
            }
            else // Denoised
            {
                // Dry / Wet crossfade
                finalSample = outSample * currentParams.mix + delayedDrySample * (1.0f - currentParams.mix);
            }

            buffer.setSample(ch, smp, finalSample);
        }

        hopCounter++;
        if (hopCounter >= hopSize)
        {
            hopCounter = 0;
            for (int ch = 0; ch < numActiveChannels; ++ch)
            {
                processStftFrame(ch);
            }
        }
    }
}

void DusterEngine::processStftFrame(int ch)
{
    auto& channel = channels[ch];

    // Window the input samples into FFT buffer
    for (int i = 0; i < fftSize; ++i)
    {
        channel.fftTimeDomain[i] = channel.inputHistory[i] * window[i];
    }
    // Zero imaginary parts
    std::fill(channel.fftTimeDomain.begin() + fftSize, channel.fftTimeDomain.end(), 0.0f);

    // Forward FFT: produces interleaved complex values in fftTimeDomain
    forwardFFT->performRealOnlyForwardTransform(channel.fftTimeDomain.data());

    // Calculate magnitude and phase for each bin
    for (int k = 0; k < numBins; ++k)
    {
        float real = channel.fftTimeDomain[2 * k];
        float imag = channel.fftTimeDomain[2 * k + 1];

        float mag = std::sqrt(real * real + imag * imag);
        if (std::isnan(mag) || std::isinf(mag))
            mag = 0.0f;

        channel.magnitude[k] = mag;
        channel.phase[k] = std::atan2(imag, real);

        if (ch == 0)
        {
            tempInputMag[k] = juce::Decibels::gainToDecibels(mag + 1e-9f, -100.0f);
        }
    }

    if (ch == 0 && spectrumBridge != nullptr)
    {
        spectrumBridge->pushInputSpectrum(tempInputMag.data(), numBins);
    }

    // Noise learning accumulator (averaged across channels)
    if (isLearningActive)
    {
        float chanDiv = 1.0f / (float)std::max(1, (int)channels.size());
        for (int k = 0; k < numBins; ++k)
        {
            noiseAccumulator[k] += channel.magnitude[k] * chanDiv;
        }
        if (ch == 0)
            learnedFrameCount++;
    }

    // Transient detection
    computeTransientFlux(ch);

    // Apply spectral subtraction / denoising gain
    applyDenoising(ch);

    // Synthesize back to complex FFT buffer with modified magnitudes
    for (int k = 0; k < numBins; ++k)
    {
        float cleanMag = channel.spectralGain[k] * channel.magnitude[k];
        channel.fftTimeDomain[2 * k]     = cleanMag * std::cos(channel.phase[k]);
        channel.fftTimeDomain[2 * k + 1] = cleanMag * std::sin(channel.phase[k]);

        if (ch == 0)
        {
            tempOutputMag[k] = juce::Decibels::gainToDecibels(cleanMag + 1e-9f, -100.0f);
        }
    }

    if (ch == 0 && spectrumBridge != nullptr)
    {
        spectrumBridge->pushOutputSpectrum(tempOutputMag.data(), numBins);
    }

    // Inverse FFT: transforms back to real time-domain
    inverseFFT->performRealOnlyInverseTransform(channel.fftTimeDomain.data());

    // Synthesis windowing & Overlap-Add into circular output buffer
    const int outSize = (int)channel.outputBuffer.size();
    for (int i = 0; i < fftSize; ++i)
    {
        float synthSample = channel.fftTimeDomain[i] * window[i] * windowGainCompensation;
        if (std::isnan(synthSample) || std::isinf(synthSample))
            synthSample = 0.0f;
        int idx = (channel.outputReadIndex + i) % outSize;
        channel.outputBuffer[idx] += synthSample;
    }

    // Shift input history by hopSize for next hop
    std::copy(channel.inputHistory.begin() + hopSize, channel.inputHistory.begin() + fftSize, channel.inputHistory.begin());
}

void DusterEngine::computeTransientFlux(int ch)
{
    auto& channel = channels[ch];
    float flux = 0.0f;
    float currentEnergy = 0.0f;

    // Detect high-frequency onset energy
    for (int k = 4; k < numBins; ++k)
    {
        float diff = channel.magnitude[k] - channel.prevMagnitude[k];
        if (diff > 0.0f)
            flux += diff;
        currentEnergy += channel.magnitude[k];
    }

    float energyRatio = (channel.prevEnergySum > 1e-6f) ? (flux / channel.prevEnergySum) : 0.0f;
    
    // Smooth onset envelope: rapid attack, fast decay
    if (energyRatio > 0.35f)
        channel.transientOnset = std::min(1.0f, channel.transientOnset + energyRatio * 1.5f);
    else
        channel.transientOnset *= 0.70f;

    channel.prevEnergySum = currentEnergy;
    std::copy(channel.magnitude.begin(), channel.magnitude.end(), channel.prevMagnitude.begin());
}

void DusterEngine::applyDenoising(int ch)
{
    auto& channel = channels[ch];

    if (!profileReady)
    {
        // No noise profile learned yet: pass audio through transparently
        std::fill(channel.spectralGain.begin(), channel.spectralGain.end(), 1.0f);
        return;
    }

    // Threshold sensitivity multiplier
    const float thresholdGain = juce::Decibels::decibelsToGain(currentParams.thresholdDb);

    // Reduction factor (dB to linear floor)
    const float spectralFloor = juce::Decibels::decibelsToGain(-currentParams.reductionDb);

    // Over-subtraction factor alpha based on reduction depth (1.0 to 3.5)
    const float overSub = 1.0f + (currentParams.reductionDb / 20.0f) * 1.2f;

    // Decision-Directed smoothing coefficient for musical noise suppression
    // Higher residualSmooth => silky noise reduction without chirps
    const float alphaDd = 0.88f + (currentParams.residualSmooth * 0.10f); // 0.88 to 0.98

    for (int k = 0; k < numBins; ++k)
    {
        float mag = channel.magnitude[k];
        float noiseEst = noiseProfile[k] * thresholdGain;
        float noisePower = noiseEst * noiseEst + 1e-12f;
        float signalPower = mag * mag;

        // Instantaneous a-posteriori SNR
        float gamma = signalPower / noisePower;

        // A-priori SNR with Decision-Directed tracking
        float prevCleanPower = channel.prevEstimatedCleanMag[k] * channel.prevEstimatedCleanMag[k];
        float xi = alphaDd * (prevCleanPower / noisePower) + (1.0f - alphaDd) * std::max(0.0f, gamma - 1.0f);

        // Wiener Filter Gain
        float gWiener = xi / (1.0f + xi);

        // Power Spectral Subtraction Gain
        float gSub = 0.0f;
        float subVal = 1.0f - overSub * (noisePower / (signalPower + 1e-12f));
        if (subVal > 0.0f)
            gSub = std::sqrt(subVal);

        // Blend between Wiener filtering and Spectral Subtraction based on residual smoothing
        float gCombined = (1.0f - currentParams.residualSmooth) * gSub + currentParams.residualSmooth * gWiener;

        // Spectral floor clamping
        float gain = std::max(spectralFloor, gCombined);

        // Transient preservation & attack boost:
        // When an onset is detected, relax suppression on upper harmonics
        if (currentParams.transientBoost > 0.01f && channel.transientOnset > 0.05f)
        {
            float boostFactor = currentParams.transientBoost * channel.transientOnset;
            gain = gain + (1.0f - gain) * boostFactor;
        }

        gain = juce::jlimit(spectralFloor, 1.0f, gain);
        if (std::isnan(gain) || std::isinf(gain))
            gain = 1.0f;

        channel.spectralGain[k] = gain;
        channel.prevEstimatedCleanMag[k] = gain * mag;
    }

    // Inter-frequency smoothing on the spectral gains to eliminate isolated sharp bins (musical chirps)
    if (currentParams.residualSmooth > 0.2f)
    {
        const float smoothAmount = (currentParams.residualSmooth - 0.2f) * 0.5f;
        for (int k = 1; k < numBins - 1; ++k)
        {
            float smoothed = 0.25f * channel.spectralGain[k - 1] + 0.5f * channel.spectralGain[k] + 0.25f * channel.spectralGain[k + 1];
            channel.spectralGain[k] = (1.0f - smoothAmount) * channel.spectralGain[k] + smoothAmount * smoothed;
        }
    }
}

juce::String DusterEngine::getSerializedProfile() const
{
    if (!profileReady || noiseProfile.empty())
        return {};

    juce::MemoryOutputStream stream;
    stream.writeInt(fftOrder);
    stream.writeInt(numBins);
    for (int i = 0; i < numBins; ++i)
        stream.writeFloat(noiseProfile[i]);

    return juce::Base64::toBase64(stream.getData(), stream.getDataSize());
}

void DusterEngine::restoreProfileFromString(const juce::String& base64)
{
    if (base64.isEmpty())
        return;

    juce::MemoryOutputStream stream;
    if (!juce::Base64::convertFromBase64(stream, base64))
        return;

    juce::MemoryInputStream inputStream(stream.getData(), stream.getDataSize(), false);
    int savedOrder = inputStream.readInt();
    int savedBins = inputStream.readInt();

    if (savedOrder == fftOrder && savedBins == numBins)
    {
        for (int i = 0; i < numBins; ++i)
            noiseProfile[i] = inputStream.readFloat();

        profileReady = true;

        for (int i = 0; i < numBins; ++i)
        {
            float db = juce::Decibels::gainToDecibels(noiseProfile[i] + 1e-9f, -100.0f);
            if (i < (int)tempProfileMag.size())
                tempProfileMag[i] = db;
        }

        if (spectrumBridge != nullptr)
            spectrumBridge->pushNoiseProfile(tempProfileMag.data(), numBins);
    }
}

} // namespace dsp
