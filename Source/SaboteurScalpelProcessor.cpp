#include "SaboteurScalpelProcessor.h"
#include "SaboteurScalpelEditor.h"

const std::array<SaboteurScalpelProcessor::FactoryPreset, SaboteurScalpelProcessor::numFactoryPresets> SaboteurScalpelProcessor::factoryPresets{ {
        // Name,                              In,   Phase, HPF,   HSlp, B1F,   B1G,  B1Q,  B2F,    B2G,  B2Q,  B3F,    B3G,  B3Q,  LPF,     LSlp, Out
        { "1. Init - Zero Color Flat",        0.0f, false, 20.0f,  0,   120.0f, 0.0f, 1.0f, 650.0f,  0.0f, 1.0f, 3800.0f, 0.0f, 1.0f, 20000.0f, 0,   0.0f },
        { "2. Vocal Hygiene - De-Proximity",  0.0f, false, 85.0f,  2,   195.0f,-3.5f, 2.2f, 480.0f, -2.0f, 1.8f, 3400.0f,-1.5f, 2.5f, 18500.0f, 1,   0.5f },
        { "3. Kick Clean - Sub & Box Cut",    0.0f, false, 32.0f,  2,   160.0f,-2.5f, 2.0f, 380.0f, -5.5f, 2.6f, 2800.0f, 1.5f, 1.2f, 14000.0f, 1,   0.5f },
        { "4. Snare Surgical - Ring & Bleed", 0.0f, false, 75.0f,  1,   210.0f, 1.0f, 1.4f, 680.0f, -4.8f, 3.8f, 4200.0f,-2.2f, 3.0f, 16000.0f, 1,   0.0f },
        { "5. Bass DI - Mud & Clank Trim",    0.0f, false, 36.0f,  2,   220.0f,-4.0f, 2.1f, 820.0f,  1.8f, 1.3f, 4500.0f,-3.0f, 2.0f,  7500.0f, 2,   0.5f },
        { "6. Electric Guitar - Cab Cleanup", 0.0f, false, 95.0f,  2,   145.0f,-3.2f, 2.4f, 420.0f, -2.0f, 1.6f, 4100.0f,-4.2f, 4.5f,  8500.0f, 2,   0.5f },
        { "7. Acoustic - Room Boom Remover",  0.0f, false, 80.0f,  1,   180.0f,-4.5f, 2.8f, 540.0f, -2.5f, 2.2f, 2900.0f,-1.5f, 2.0f, 19000.0f, 1,   1.0f },
        { "8. Mix Prep - Sub & Ultrasonic",   0.0f, false, 24.0f,  2,   130.0f,-1.0f, 1.5f, 340.0f, -1.5f, 1.8f, 3200.0f,-0.8f, 2.0f, 19500.0f, 2,   0.5f }
    } };

SaboteurScalpelProcessor::SaboteurScalpelProcessor()
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

juce::AudioProcessorValueTreeState::ParameterLayout SaboteurScalpelProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Gain Staging & Polarité
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "in_gain", "Input Trim",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f, "dB"));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "phase_inv", "Phase Invert 180", false));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "listen_mode", "Monitor Mode",
        juce::StringArray{ "Stereo", "Mono Sum", "Mid Only", "Side Only" }, 0));

    // Filtre Passe-Haut (HPF)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "hpf_freq", "HPF Frequency",
        juce::NormalisableRange<float>(20.0f, 400.0f, 0.5f, 0.5f), 20.0f, "Hz"));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "hpf_slope", "HPF Slope",
        juce::StringArray{ "Off", "12 dB/oct", "24 dB/oct" }, 0));

    // Bande 1 (Low / Low-Mid : 40 - 500 Hz)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "b1_freq", "Band 1 Freq",
        juce::NormalisableRange<float>(40.0f, 500.0f, 0.5f, 0.55f), 120.0f, "Hz"));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "b1_gain", "Band 1 Gain",
        juce::NormalisableRange<float>(-18.0f, 12.0f, 0.1f), 0.0f, "dB"));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "b1_q", "Band 1 Q",
        juce::NormalisableRange<float>(0.3f, 10.0f, 0.05f, 0.6f), 1.0f));

    // Bande 2 (Mid / Boxiness : 200 - 2500 Hz)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "b2_freq", "Band 2 Freq",
        juce::NormalisableRange<float>(200.0f, 2500.0f, 1.0f, 0.55f), 650.0f, "Hz"));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "b2_gain", "Band 2 Gain",
        juce::NormalisableRange<float>(-18.0f, 12.0f, 0.1f), 0.0f, "dB"));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "b2_q", "Band 2 Q",
        juce::NormalisableRange<float>(0.3f, 10.0f, 0.05f, 0.6f), 1.0f));

    // Bande 3 (High-Mid / Harshness : 1200 - 16000 Hz)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "b3_freq", "Band 3 Freq",
        juce::NormalisableRange<float>(1200.0f, 16000.0f, 5.0f, 0.55f), 3800.0f, "Hz"));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "b3_gain", "Band 3 Gain",
        juce::NormalisableRange<float>(-18.0f, 12.0f, 0.1f), 0.0f, "dB"));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "b3_q", "Band 3 Q",
        juce::NormalisableRange<float>(0.3f, 10.0f, 0.05f, 0.6f), 1.0f));

    // Filtre Passe-Bas (LPF)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "lpf_freq", "LPF Frequency",
        juce::NormalisableRange<float>(3000.0f, 20000.0f, 10.0f, 0.6f), 20000.0f, "Hz"));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "lpf_slope", "LPF Slope",
        juce::StringArray{ "Off", "12 dB/oct", "24 dB/oct" }, 0));

    // Mode Loupe (Band-Solo) & Gain de Sortie
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "solo_band", "Band Solo Loupe",
        juce::StringArray{ "Off", "Solo B1 (Low)", "Solo B2 (Mid)", "Solo B3 (High)" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "out_gain", "Output Trim",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f, "dB"));

    return { params.begin(), params.end() };
}

bool SaboteurScalpelProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

void SaboteurScalpelProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = (sampleRate > 0.0) ? sampleRate : 48000.0;

    const int numChannels = std::max(1, getTotalNumInputChannels());
    channelStates.resize(numChannels);
    for (auto& st : channelStates)
        st.reset();

    visFifoIndex = 0;
    std::fill(visInFifo.begin(), visInFifo.end(), 0.0f);
    std::fill(visOutFifo.begin(), visOutFifo.end(), 0.0f);

    setLatencySamples(0);
}

void SaboteurScalpelProcessor::releaseResources()
{
}

void SaboteurScalpelProcessor::updateChannelFilters(ChannelState& st,
    double hpfFreq, int hpfSlope,
    double b1F, double b1G, double b1Q,
    double b2F, double b2G, double b2Q,
    double b3F, double b3G, double b3Q,
    double lpfFreq, int lpfSlope,
    int soloChoice) noexcept
{
    // Butterworth 12 dB/oct (1 biquad Q=0.7071) ou 24 dB/oct (cascade Q1=0.5412, Q2=1.3065)
    if (hpfSlope == 1)
    {
        st.hpf1.setHighPass(currentSampleRate, hpfFreq, 0.70710678);
        st.hpf2.setBypass();
    }
    else if (hpfSlope == 2)
    {
        st.hpf1.setHighPass(currentSampleRate, hpfFreq, 0.54119610);
        st.hpf2.setHighPass(currentSampleRate, hpfFreq, 1.30656296);
    }
    else
    {
        st.hpf1.setBypass();
        st.hpf2.setBypass();
    }

    st.band1.setPeakingPropQ(currentSampleRate, b1F, b1Q, b1G);
    st.band2.setPeakingPropQ(currentSampleRate, b2F, b2Q, b2G);
    st.band3.setPeakingPropQ(currentSampleRate, b3F, b3Q, b3G);

    if (lpfSlope == 1)
    {
        st.lpf1.setLowPass(currentSampleRate, lpfFreq, 0.70710678);
        st.lpf2.setBypass();
    }
    else if (lpfSlope == 2)
    {
        st.lpf1.setLowPass(currentSampleRate, lpfFreq, 0.54119610);
        st.lpf2.setLowPass(currentSampleRate, lpfFreq, 1.30656296);
    }
    else
    {
        st.lpf1.setBypass();
        st.lpf2.setBypass();
    }

    if (soloChoice == 1)      st.soloFilter.setBandPassSolo(currentSampleRate, b1F, b1Q);
    else if (soloChoice == 2) st.soloFilter.setBandPassSolo(currentSampleRate, b2F, b2Q);
    else if (soloChoice == 3) st.soloFilter.setBandPassSolo(currentSampleRate, b3F, b3Q);
    else                      st.soloFilter.setBypass();
}

float SaboteurScalpelProcessor::getFilterMagnitudeDbAtFrequency(double freqHz) const
{
    if (channelStates.empty())
        return 0.0f;

    const auto& st = channelStates[0];
    double magSq = 1.0;
    magSq *= st.hpf1.getMagnitudeSquaredAt(currentSampleRate, freqHz);
    magSq *= st.hpf2.getMagnitudeSquaredAt(currentSampleRate, freqHz);
    magSq *= st.band1.getMagnitudeSquaredAt(currentSampleRate, freqHz);
    magSq *= st.band2.getMagnitudeSquaredAt(currentSampleRate, freqHz);
    magSq *= st.band3.getMagnitudeSquaredAt(currentSampleRate, freqHz);
    magSq *= st.lpf1.getMagnitudeSquaredAt(currentSampleRate, freqHz);
    magSq *= st.lpf2.getMagnitudeSquaredAt(currentSampleRate, freqHz);

    double db = 10.0 * std::log10(std::max(1e-12, magSq));
    return static_cast<float> (std::clamp(db, -36.0, 24.0));
}

void SaboteurScalpelProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    if (static_cast<int>(channelStates.size()) < totalNumInputChannels)
        channelStates.resize(totalNumInputChannels);

    // Lecture atomique des paramètres
    const double inGainDb = apvts.getRawParameterValue("in_gain")->load();
    const bool phaseInv = apvts.getRawParameterValue("phase_inv")->load() > 0.5f;
    const int listenMode = static_cast<int>(apvts.getRawParameterValue("listen_mode")->load());

    const double hpfFreq = apvts.getRawParameterValue("hpf_freq")->load();
    const int hpfSlope = static_cast<int> (apvts.getRawParameterValue("hpf_slope")->load());

    const double b1F = apvts.getRawParameterValue("b1_freq")->load();
    const double b1G = apvts.getRawParameterValue("b1_gain")->load();
    const double b1Q = apvts.getRawParameterValue("b1_q")->load();

    const double b2F = apvts.getRawParameterValue("b2_freq")->load();
    const double b2G = apvts.getRawParameterValue("b2_gain")->load();
    const double b2Q = apvts.getRawParameterValue("b2_q")->load();

    const double b3F = apvts.getRawParameterValue("b3_freq")->load();
    const double b3G = apvts.getRawParameterValue("b3_gain")->load();
    const double b3Q = apvts.getRawParameterValue("b3_q")->load();

    const double lpfFreq = apvts.getRawParameterValue("lpf_freq")->load();
    const int lpfSlope = static_cast<int> (apvts.getRawParameterValue("lpf_slope")->load());

    const int soloBand = static_cast<int> (apvts.getRawParameterValue("solo_band")->load());
    const double outGainDb = apvts.getRawParameterValue("out_gain")->load();

    const double inGainLin = std::pow(10.0, inGainDb / 20.0) * (phaseInv ? -1.0 : 1.0);
    const double outGainLin = std::pow(10.0, outGainDb / 20.0);

    for (int ch = 0; ch < totalNumInputChannels; ++ch)
    {
        updateChannelFilters(channelStates[ch],
            hpfFreq, hpfSlope,
            b1F, b1G, b1Q,
            b2F, b2G, b2Q,
            b3F, b3G, b3Q,
            lpfFreq, lpfSlope,
            soloBand);
    }

    // Matrice d'écoute pré-traitement (Stereo, Mono, Mid, Side)
    if (totalNumInputChannels >= 2 && listenMode > 0)
    {
        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getWritePointer(1);
        for (int i = 0; i < numSamples; ++i)
        {
            const float l = left[i];
            const float r = right[i];
            if (listenMode == 1 || listenMode == 2) // Mono Sum ou Mid
            {
                const float mid = 0.5f * (l + r);
                left[i] = mid;
                right[i] = mid;
            }
            else if (listenMode == 3) // Side Only
            {
                const float side = 0.5f * (l - r);
                left[i] = side;
                right[i] = side;
            }
        }
    }

    double sumSqIn = 0.0;
    double sumSqOut = 0.0;

    for (int ch = 0; ch < totalNumInputChannels; ++ch)
    {
        auto& st = channelStates[ch];
        auto* channelData = buffer.getWritePointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            const double rawIn = static_cast<double>(channelData[i]);
            double x = rawIn * inGainLin;

            if (soloBand > 0)
            {
                // Mode Loupe : écoute isolée de la bande ciblée
                x = st.soloFilter.process(x) * 1.4;
            }
            else
            {
                // Chemin chirurgical 64-bit 100 % linéaire (zéro distorsion harmonique)
                x = st.hpf1.process(x);
                x = st.hpf2.process(x);
                x = st.band1.process(x);
                x = st.band2.process(x);
                x = st.band3.process(x);
                x = st.lpf1.process(x);
                x = st.lpf2.process(x);
            }

            const double finalOut = x * outGainLin;
            channelData[i] = static_cast<float> (finalOut);

            if (ch == 0)
            {
                sumSqIn += rawIn * rawIn;
                sumSqOut += finalOut * finalOut;
            }
        }
    }

    if (numSamples > 0 && totalNumInputChannels > 0)
    {
        const float blockInRmsDb = juce::Decibels::gainToDecibels(static_cast<float> (std::sqrt(sumSqIn / numSamples)), -90.0f);
        const float blockOutRmsDb = juce::Decibels::gainToDecibels(static_cast<float> (std::sqrt(sumSqOut / numSamples)), -90.0f);

        const auto* ch0 = buffer.getReadPointer(0);
        for (int i = 0; i < numSamples; ++i)
        {
            const float outS = ch0[i];
            const float inS = outS / static_cast<float>(std::max(0.001, std::abs(outGainLin)));
            pushVisualSample(inS, outS, blockInRmsDb, blockOutRmsDb);
        }
    }
}

void SaboteurScalpelProcessor::pushVisualSample(float inSample, float outSample, float inRmsDb, float outRmsDb)
{
    visInFifo[visFifoIndex] = inSample;
    visOutFifo[visFifoIndex] = outSample;
    ++visFifoIndex;

    {
        const juce::SpinLock::ScopedTryLockType lock(visualLock);
        if (lock.isLocked())
        {
            visInRmsDb = 0.90f * visInRmsDb + 0.10f * inRmsDb;
            visOutRmsDb = 0.90f * visOutRmsDb + 0.10f * outRmsDb;
        }
    }

    if (visFifoIndex >= fftSize)
    {
        visFifoIndex = 0;
        computeVisualFft();
    }
}

void SaboteurScalpelProcessor::computeVisualFft()
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

void SaboteurScalpelProcessor::getVisualSnapshot(std::vector<float>& outInSpec,
    std::vector<float>& outOutSpec,
    float& outInRmsDb,
    float& outOutRmsDb,
    double& outSampleRate)
{
    const juce::SpinLock::ScopedLockType lock(visualLock);
    outInSpec = visInSpectrum;
    outOutSpec = visOutSpectrum;
    outInRmsDb = visInRmsDb;
    outOutRmsDb = visOutRmsDb;
    outSampleRate = currentSampleRate;
}

// --- GESTION DES PRESETS & MÉTHODES STANDARD JUCE ---
const juce::String SaboteurScalpelProcessor::getName() const { return "Saboteur Scalpel"; }
bool SaboteurScalpelProcessor::acceptsMidi() const { return false; }
bool SaboteurScalpelProcessor::producesMidi() const { return false; }
bool SaboteurScalpelProcessor::isMidiEffect() const { return false; }
double SaboteurScalpelProcessor::getTailLengthSeconds() const { return 0.0; }

int SaboteurScalpelProcessor::getNumPrograms() { return numFactoryPresets; }
int SaboteurScalpelProcessor::getCurrentProgram() { return currentPresetIndex; }

void SaboteurScalpelProcessor::setCurrentProgram(int index)
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
    setParam("phase_inv", p.phaseInv ? 1.0f : 0.0f);
    setParam("hpf_freq", p.hpfFreq);
    setParam("hpf_slope", static_cast<float> (p.hpfSlope));
    setParam("b1_freq", p.b1Freq);
    setParam("b1_gain", p.b1Gain);
    setParam("b1_q", p.b1Q);
    setParam("b2_freq", p.b2Freq);
    setParam("b2_gain", p.b2Gain);
    setParam("b2_q", p.b2Q);
    setParam("b3_freq", p.b3Freq);
    setParam("b3_gain", p.b3Gain);
    setParam("b3_q", p.b3Q);
    setParam("lpf_freq", p.lpfFreq);
    setParam("lpf_slope", static_cast<float> (p.lpfSlope));
    setParam("out_gain", p.outGainDb);
}

const juce::String SaboteurScalpelProcessor::getProgramName(int index)
{
    if (index >= 0 && index < numFactoryPresets)
        return factoryPresets[index].name;
    return {};
}

void SaboteurScalpelProcessor::changeProgramName(int, const juce::String&) {}

void SaboteurScalpelProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SaboteurScalpelProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorEditor* SaboteurScalpelProcessor::createEditor()
{
    return new SaboteurScalpelEditor(*this);
}

bool SaboteurScalpelProcessor::hasEditor() const { return true; }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SaboteurScalpelProcessor();
}