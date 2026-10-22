#include "SaboteurAlloyProcessor.h"
#include "SaboteurAlloyEditor.h"

const std::array<SaboteurAlloyProcessor::FactoryPreset, SaboteurAlloyProcessor::numFactoryPresets> SaboteurAlloyProcessor::factoryPresets{ {
    { "Init - Pure Core",           0.0f,  0.0f,  0.0f, 100.0f, 0.0f, true  },
    { "Drums - Parallel Smash",     2.5f, 68.0f, 72.0f,  55.0f, 0.0f, true  },
    { "Drums - Kick & Snare Punch", 1.5f, 58.0f, 52.0f,  75.0f, 0.0f, true  },
    { "Bass - Opto Tube Anchor",    1.5f, 78.0f, 22.0f,  85.0f, 0.0f, true  },
    { "Bass - Pick & FET Growl",    2.5f, 50.0f, 68.0f,  70.0f, 0.0f, true  },
    { "Vocals - Silk & Air",        0.0f, 38.0f, 56.0f,  65.0f, 0.5f, true  },
    { "Vocals - Rock Frontman",     3.0f, 32.0f, 82.0f,  60.0f, 0.0f, true  },
    { "Mix Bus - Velvet Glue",      0.0f, 28.0f, 26.0f,  42.0f, 0.0f, true  }
} };

SaboteurAlloyProcessor::SaboteurAlloyProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "Parameters", createParameterLayout()),
    visFft(fftOrder),
    visWindow(fftSize, juce::dsp::WindowingFunction<float>::hann, false)
{
    visInFifo.assign(fftSize, 0.0f);
    visOutFifo.assign(fftSize, 0.0f);
    fftWorkspaceIn.assign(fftSize * 2, 0.0f);
    fftWorkspaceOut.assign(fftSize * 2, 0.0f);

    visInSpectrum.assign(numBins + 1, 0.0f);
    visOutSpectrum.assign(numBins + 1, 0.0f);
}

juce::AudioProcessorValueTreeState::ParameterLayout SaboteurAlloyProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // 1. Gain d'entrée (In)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "in_gain", "Input Gain",
        juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f), 0.0f, "dB"));

    // 2. EMBER (< 600 Hz : Lampes + Compression Opto lente)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "ember", "Ember (< 600 Hz)",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));

    // 3. SPARK (> 2 kHz : Saturation FET + Compression 1176 rapide)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "spark", "Spark (> 2 kHz)",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));

    // 4. Mélange Wet/Dry global (Mix)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "mix", "Mix (Wet/Dry)",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f, "%"));

    // 5. Gain de sortie (Out)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "out_gain", "Output Gain",
        juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f), 0.0f, "dB"));

    // 6. Compensation automatique du niveau (Auto Makeup)
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "auto_makeup", "Auto Makeup", true));

    return { params.begin(), params.end() };
}

bool SaboteurAlloyProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

void SaboteurAlloyProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = (sampleRate > 0.0) ? sampleRate : 48000.0;

    const int numChannels = std::max(1, getTotalNumInputChannels());
    channelStates.resize(numChannels);
    for (auto& state : channelStates)
    {
        state.reset();
        updateFilterCoefficients(state);
    }

    visFifoIndex = 0;
    std::fill(visInFifo.begin(), visInFifo.end(), 0.0f);
    std::fill(visOutFifo.begin(), visOutFifo.end(), 0.0f);

    setLatencySamples(0);
}

void SaboteurAlloyProcessor::releaseResources()
{
}

void SaboteurAlloyProcessor::updateFilterCoefficients(ChannelState& state)
{
    // Bande EMBER : sous 600 Hz
    state.lowLPF1.setLowPass(currentSampleRate, 600.0f, 0.7071f);
    state.lowLPF2.setLowPass(currentSampleRate, 600.0f, 0.7071f);
    state.emberPostLPF.setLowPass(currentSampleRate, 720.0f, 0.7071f);

    // Bande SPARK : au-dessus de 2000 Hz
    state.highHPF1.setHighPass(currentSampleRate, 2000.0f, 0.7071f);
    state.highHPF2.setHighPass(currentSampleRate, 2000.0f, 0.7071f);
    state.sparkPostHPF.setHighPass(currentSampleRate, 1850.0f, 0.7071f);
}

void SaboteurAlloyProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    if (static_cast<int>(channelStates.size()) < totalNumInputChannels)
    {
        channelStates.resize(totalNumInputChannels);
        for (auto& st : channelStates)
            updateFilterCoefficients(st);
    }

    // Lecture atomique des paramètres
    const float inGainDb = apvts.getRawParameterValue("in_gain")->load();
    const float emberAmt = apvts.getRawParameterValue("ember")->load() * 0.01f;
    const float sparkAmt = apvts.getRawParameterValue("spark")->load() * 0.01f;
    const float wetMix = apvts.getRawParameterValue("mix")->load() * 0.01f;
    const float outGainDb = apvts.getRawParameterValue("out_gain")->load();
    const bool  autoMakeup = apvts.getRawParameterValue("auto_makeup")->load() > 0.5f;

    const float inGainLin = juce::Decibels::decibelsToGain(inGainDb);
    const float outGainLin = juce::Decibels::decibelsToGain(outGainDb);

    // Constantes de temps Opto (< 600 Hz : lent et organique) et FET 1176 (> 2 kHz : ultra-rapide)
    const float sr = static_cast<float> (currentSampleRate);
    const float optoAttack = std::exp(-1.0f / (0.025f * sr));  // 25 ms
    const float optoRelease = std::exp(-1.0f / (0.180f * sr));  // 180 ms
    const float fetAttack = std::exp(-1.0f / (0.0004f * sr)); // 0.4 ms
    const float fetRelease = std::exp(-1.0f / (0.045f * sr));  // 45 ms

    // Seuils et drives des étages parallèles
    const float optoThreshold = 0.14f;
    const float fetThreshold = 0.09f;
    const float tubeDrive = 1.0f + emberAmt * 3.2f;
    const float fetDrive = 1.0f + sparkAmt * 4.2f;

    // Calcul de la compensation automatique de niveau (Auto Makeup)
    float autoMakeupDb = 0.0f;
    if (autoMakeup)
    {
        // Compense l'énergie ajoutée par les branches parallèles et une fraction du gain d'entrée
        const float parallelLiftDb = (emberAmt * 3.6f) + (sparkAmt * 3.2f);
        const float inputCompDb = inGainDb * 0.55f;
        autoMakeupDb = -(parallelLiftDb + inputCompDb);
    }
    const float totalPostGain = outGainLin * juce::Decibels::decibelsToGain(autoMakeupDb);

    float maxEmberGrDb = 0.0f;
    float maxSparkGrDb = 0.0f;

    for (int ch = 0; ch < totalNumInputChannels; ++ch)
    {
        auto& st = channelStates[ch];
        auto* channelData = buffer.getWritePointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            const float rawInput = channelData[i];
            const float drivenIn = rawInput * inGainLin;

            // --- 1. EXTRACTION DES BANDES (< 600 Hz et > 2 kHz) ---
            // Les médiums (600 Hz - 2 kHz) restent intacts dans le signal principal drivenIn
            float lowBand = st.lowLPF1.process(drivenIn);
            lowBand = st.lowLPF2.process(lowBand);

            float highBand = st.highHPF1.process(drivenIn);
            highBand = st.highHPF2.process(highBand);

            // --- 2. MOTEUR "EMBER" (< 600 Hz : Compression Opto lente + Saturation à lampes) ---
            const float absLow = std::abs(lowBand);
            st.optoEnv = (absLow > st.optoEnv)
                ? optoAttack * st.optoEnv + (1.0f - optoAttack) * absLow
                : optoRelease * st.optoEnv + (1.0f - optoRelease) * absLow;

            float optoGain = 1.0f;
            if (st.optoEnv > optoThreshold)
            {
                // Compression Opto douce (Ratio ~ 3.2:1)
                const float over = st.optoEnv / optoThreshold;
                optoGain = std::pow(over, -0.68f);
            }

            float emberSig = lowBand * optoGain * tubeDrive;
            // Saturation asymétrique à lampes (harmoniques paires chaleureuses + rondeur)
            const float asymBias = 0.16f * emberAmt;
            float shapedTube = std::tanh(emberSig + asymBias) - std::tanh(asymBias);
            shapedTube += 0.18f * emberAmt * (emberSig * emberSig);

            // Suppression de l'offset continu (DC block) et confinement sous 600 Hz
            st.dcBlockEmber = 0.996f * st.dcBlockEmber + 0.004f * shapedTube;
            shapedTube -= st.dcBlockEmber;
            shapedTube = st.emberPostLPF.process(shapedTube);

            // Contribution parallèle Ember
            const float emberParallel = shapedTube * (emberAmt * 1.65f);

            // --- 3. MOTEUR "SPARK" (> 2 kHz : Compression FET 1176 rapide + Saturation FET) ---
            const float absHigh = std::abs(highBand);
            st.fetEnv = (absHigh > st.fetEnv)
                ? fetAttack * st.fetEnv + (1.0f - fetAttack) * absHigh
                : fetRelease * st.fetEnv + (1.0f - fetRelease) * absHigh;

            float fetGain = 1.0f;
            if (st.fetEnv > fetThreshold)
            {
                // Compression FET mordante (Ratio ~ 6:1)
                const float over = st.fetEnv / fetThreshold;
                fetGain = std::pow(over, -0.83f);
            }

            float sparkSig = highBand * fetGain * fetDrive;
            // Saturation FET nerveuse (harmoniques impaires + présence)
            float shapedFet = sparkSig / (1.0f + std::abs(sparkSig * 0.85f));
            shapedFet = std::tanh(shapedFet * 1.35f);
            shapedFet = st.sparkPostHPF.process(shapedFet);

            // Contribution parallèle Spark
            const float sparkParallel = shapedFet * (sparkAmt * 1.75f);

            // --- 4. SOMMATION PARALLÈLE (Médiums 600 Hz - 2 kHz préservés à 100 %) ---
            const float wetProcessed = (drivenIn + emberParallel + sparkParallel) * totalPostGain;
            float finalOut = (1.0f - wetMix) * rawInput + (wetMix * wetProcessed);

            // Protection douce contre l'écrêtage numérique
            finalOut = std::tanh(finalOut * 0.96f) / 0.96f;
            channelData[i] = finalOut;

            if (ch == 0)
            {
                const float currentEmberGr = (emberAmt > 0.001f)
                    ? std::abs(juce::Decibels::gainToDecibels(std::max(0.1f, optoGain))) * emberAmt
                    : 0.0f;
                const float currentSparkGr = (sparkAmt > 0.001f)
                    ? std::abs(juce::Decibels::gainToDecibels(std::max(0.1f, fetGain))) * sparkAmt
                    : 0.0f;

                maxEmberGrDb = std::max(maxEmberGrDb, currentEmberGr);
                maxSparkGrDb = std::max(maxSparkGrDb, currentSparkGr);

                pushVisualSample(rawInput, finalOut, maxEmberGrDb, maxSparkGrDb);
            }
        }
    }
}

void SaboteurAlloyProcessor::pushVisualSample(float inSample, float outSample, float emberGrDb, float sparkGrDb)
{
    visInFifo[visFifoIndex] = inSample;
    visOutFifo[visFifoIndex] = outSample;
    ++visFifoIndex;

    {
        const juce::SpinLock::ScopedTryLockType lock(visualLock);
        if (lock.isLocked())
        {
            visEmberGrDb = 0.92f * visEmberGrDb + 0.08f * emberGrDb;
            visSparkGrDb = 0.92f * visSparkGrDb + 0.08f * sparkGrDb;
        }
    }

    if (visFifoIndex >= fftSize)
    {
        visFifoIndex = 0;
        computeVisualFft();
    }
}

void SaboteurAlloyProcessor::computeVisualFft()
{
    std::fill(fftWorkspaceIn.begin(), fftWorkspaceIn.end(), 0.0f);
    std::fill(fftWorkspaceOut.begin(), fftWorkspaceOut.end(), 0.0f);

    std::copy(visInFifo.begin(), visInFifo.end(), fftWorkspaceIn.begin());
    std::copy(visOutFifo.begin(), visOutFifo.end(), fftWorkspaceOut.begin());

    visWindow.multiplyWithWindowingTable(fftWorkspaceIn.data(), fftSize);
    visWindow.multiplyWithWindowingTable(fftWorkspaceOut.data(), fftSize);

    visFft.performRealOnlyForwardTransform(fftWorkspaceIn.data());
    visFft.performRealOnlyForwardTransform(fftWorkspaceOut.data());

    const juce::SpinLock::ScopedTryLockType lock(visualLock);
    if (lock.isLocked())
    {
        const float invNorm = 2.0f / static_cast<float> (fftSize);
        for (int i = 1; i <= numBins; ++i)
        {
            float ir = fftWorkspaceIn[i * 2];
            float ii = fftWorkspaceIn[i * 2 + 1];
            float inMag = std::sqrt(ir * ir + ii * ii) * invNorm;

            float or_ = fftWorkspaceOut[i * 2];
            float oi = fftWorkspaceOut[i * 2 + 1];
            float outMag = std::sqrt(or_ * or_ + oi * oi) * invNorm;

            visInSpectrum[i] = 0.65f * visInSpectrum[i] + 0.35f * inMag;
            visOutSpectrum[i] = 0.65f * visOutSpectrum[i] + 0.35f * outMag;
        }
    }
}

void SaboteurAlloyProcessor::getVisualSnapshot(std::vector<float>& outInSpec,
    std::vector<float>& outOutSpec,
    float& outEmberGrDb,
    float& outSparkGrDb,
    double& outSampleRate)
{
    const juce::SpinLock::ScopedLockType lock(visualLock);
    outInSpec = visInSpectrum;
    outOutSpec = visOutSpectrum;
    outEmberGrDb = visEmberGrDb;
    outSparkGrDb = visSparkGrDb;
    outSampleRate = currentSampleRate;
}

// --- GESTION DES PRESETS & MÉTHODES STANDARD JUCE ---
const juce::String SaboteurAlloyProcessor::getName() const { return "Saboteur Alloy"; }
bool SaboteurAlloyProcessor::acceptsMidi() const { return false; }
bool SaboteurAlloyProcessor::producesMidi() const { return false; }
bool SaboteurAlloyProcessor::isMidiEffect() const { return false; }
double SaboteurAlloyProcessor::getTailLengthSeconds() const { return 0.0; }

int SaboteurAlloyProcessor::getNumPrograms() { return numFactoryPresets; }
int SaboteurAlloyProcessor::getCurrentProgram() { return currentPresetIndex; }

void SaboteurAlloyProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= numFactoryPresets)
        return;

    currentPresetIndex = index;
    const auto& p = factoryPresets[index];

    auto setParam = [this](const char* id, float plainValue)
        {
            if (auto* param = apvts.getParameter(id))
                param->setValueNotifyingHost(param->convertTo0to1(plainValue));
        };

    setParam("in_gain", p.inGainDb);
    setParam("ember", p.emberPercent);
    setParam("spark", p.sparkPercent);
    setParam("mix", p.mixPercent);
    setParam("out_gain", p.outGainDb);
    setParam("auto_makeup", p.autoMakeup ? 1.0f : 0.0f);
}

const juce::String SaboteurAlloyProcessor::getProgramName(int index)
{
    if (index >= 0 && index < numFactoryPresets)
        return factoryPresets[index].name;
    return {};
}

void SaboteurAlloyProcessor::changeProgramName(int, const juce::String&) {}

void SaboteurAlloyProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SaboteurAlloyProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorEditor* SaboteurAlloyProcessor::createEditor()
{
    return new SaboteurAlloyEditor(*this);
}

bool SaboteurAlloyProcessor::hasEditor() const { return true; }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SaboteurAlloyProcessor();
}