#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>

class SaboteurScalpelProcessor : public juce::AudioProcessor
{
public:
    SaboteurScalpelProcessor();
    ~SaboteurScalpelProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // --- MÉTHODES STANDARD JUCE OBLIGATOIRES ---
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    juce::AudioProcessorValueTreeState apvts;

    struct FactoryPreset
    {
        const char* name;
        float inGainDb;
        bool phaseInv;
        float hpfFreq;
        int hpfSlope; // 0 = Off, 1 = 12dB, 2 = 24dB
        float b1Freq, b1Gain, b1Q;
        float b2Freq, b2Gain, b2Q;
        float b3Freq, b3Gain, b3Q;
        float lpfFreq;
        int lpfSlope; // 0 = Off, 1 = 12dB, 2 = 24dB
        float outGainDb;
    };

    static constexpr int numFactoryPresets = 8;
    static const std::array<FactoryPreset, numFactoryPresets> factoryPresets;

    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder; // 2048
    static constexpr int numBins = fftSize / 2;   // 1024

    // Calcul exact de la réponse en fréquence (en dB) pour le tracé GUI
    float getFilterMagnitudeDbAtFrequency(double freqHz) const;

    void getVisualSnapshot(std::vector<float>& outInSpec,
        std::vector<float>& outOutSpec,
        float& outInRmsDb,
        float& outOutRmsDb,
        double& outSampleRate);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Biquad en double précision (64-bit) pour éliminer tout bruit de quantification dans le grave
    struct DoubleBiquad
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0;
        double a1 = 0.0, a2 = 0.0;
        double z1 = 0.0, z2 = 0.0;

        void reset() noexcept { z1 = 0.0; z2 = 0.0; }

        double process(double x) noexcept
        {
            double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }

        void setBypass() noexcept
        {
            b0 = 1.0; b1 = 0.0; b2 = 0.0; a1 = 0.0; a2 = 0.0;
        }

        void setHighPass(double sr, double freq, double q = 0.70710678) noexcept
        {
            double w0 = juce::MathConstants<double>::twoPi * std::clamp(freq, 15.0, sr * 0.48) / sr;
            double cosw = std::cos(w0), sinw = std::sin(w0);
            double alpha = sinw / (2.0 * q);
            double a0Inv = 1.0 / (1.0 + alpha);
            b0 = ((1.0 + cosw) * 0.5) * a0Inv;
            b1 = -(1.0 + cosw) * a0Inv;
            b2 = b0;
            a1 = (-2.0 * cosw) * a0Inv;
            a2 = (1.0 - alpha) * a0Inv;
        }

        void setLowPass(double sr, double freq, double q = 0.70710678) noexcept
        {
            double w0 = juce::MathConstants<double>::twoPi * std::clamp(freq, 20.0, sr * 0.48) / sr;
            double cosw = std::cos(w0), sinw = std::sin(w0);
            double alpha = sinw / (2.0 * q);
            double a0Inv = 1.0 / (1.0 + alpha);
            b0 = ((1.0 - cosw) * 0.5) * a0Inv;
            b1 = (1.0 - cosw) * a0Inv;
            b2 = b0;
            a1 = (-2.0 * cosw) * a0Inv;
            a2 = (1.0 - alpha) * a0Inv;
        }

        void setPeakingPropQ(double sr, double freq, double baseQ, double gainDb) noexcept
        {
            if (std::abs(gainDb) < 0.01)
            {
                setBypass();
                return;
            }
            // Facteur Q proportionnel asymétrique : plus étroit en coupure chirurgicale, plus doux en boost
            double effQ = (gainDb < 0.0)
                ? baseQ * (1.0 + 0.09 * std::abs(gainDb))
                : baseQ / (1.0 + 0.04 * gainDb);
            effQ = std::clamp(effQ, 0.25, 18.0);

            double A = std::pow(10.0, gainDb / 40.0);
            double w0 = juce::MathConstants<double>::twoPi * std::clamp(freq, 20.0, sr * 0.48) / sr;
            double cosw = std::cos(w0), sinw = std::sin(w0);
            double alpha = sinw / (2.0 * effQ);
            double a0Inv = 1.0 / (1.0 + alpha / A);
            b0 = (1.0 + alpha * A) * a0Inv;
            b1 = (-2.0 * cosw) * a0Inv;
            b2 = (1.0 - alpha * A) * a0Inv;
            a1 = b1;
            a2 = (1.0 - alpha / A) * a0Inv;
        }

        void setBandPassSolo(double sr, double freq, double q) noexcept
        {
            double effQ = std::clamp(q * 1.4, 0.7, 16.0);
            double w0 = juce::MathConstants<double>::twoPi * std::clamp(freq, 20.0, sr * 0.48) / sr;
            double cosw = std::cos(w0), sinw = std::sin(w0);
            double alpha = sinw / (2.0 * effQ);
            double a0Inv = 1.0 / (1.0 + alpha);
            b0 = alpha * a0Inv;
            b1 = 0.0;
            b2 = -alpha * a0Inv;
            a1 = (-2.0 * cosw) * a0Inv;
            a2 = (1.0 - alpha) * a0Inv;
        }

        double getMagnitudeSquaredAt(double sr, double freqHz) const noexcept
        {
            double w = juce::MathConstants<double>::twoPi * std::clamp(freqHz, 10.0, sr * 0.49) / sr;
            double cos1 = std::cos(w);
            double cos2 = std::cos(2.0 * w);
            double num = b0 * b0 + b1 * b1 + b2 * b2 + 2.0 * (b0 * b1 + b1 * b2) * cos1 + 2.0 * b0 * b2 * cos2;
            double den = 1.0 + a1 * a1 + a2 * a2 + 2.0 * (a1 + a1 * a2) * cos1 + 2.0 * a2 * cos2;
            return std::max(1e-12, num / std::max(1e-12, den));
        }
    };

    struct ChannelState
    {
        DoubleBiquad hpf1, hpf2;
        DoubleBiquad band1, band2, band3;
        DoubleBiquad lpf1, lpf2;
        DoubleBiquad soloFilter;

        void reset() noexcept
        {
            hpf1.reset(); hpf2.reset();
            band1.reset(); band2.reset(); band3.reset();
            lpf1.reset(); lpf2.reset();
            soloFilter.reset();
        }
    };

    double currentSampleRate = 48000.0;
    int currentPresetIndex = 0;
    std::vector<ChannelState> channelStates;

    // Analyseur FFT 60 FPS
    juce::dsp::FFT visFft;
    juce::dsp::WindowingFunction<float> visWindow;
    std::vector<float> visInFifo;
    std::vector<float> visOutFifo;
    std::vector<float> fftWorkspaceIn;
    std::vector<float> fftWorkspaceOut;
    int visFifoIndex = 0;

    juce::SpinLock visualLock;
    std::vector<float> visInSpectrum;
    std::vector<float> visOutSpectrum;
    float visInRmsDb = -90.0f;
    float visOutRmsDb = -90.0f;

    void updateChannelFilters(ChannelState& st,
        double hpfFreq, int hpfSlope,
        double b1F, double b1G, double b1Q,
        double b2F, double b2G, double b2Q,
        double b3F, double b3G, double b3Q,
        double lpfFreq, int lpfSlope,
        int soloChoice) noexcept;
    void pushVisualSample(float inSample, float outSample, float inRmsDb, float outRmsDb);
    void computeVisualFft();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SaboteurScalpelProcessor)
};