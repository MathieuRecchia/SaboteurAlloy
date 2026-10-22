#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>

class SaboteurAlloyProcessor : public juce::AudioProcessor
{
public:
    SaboteurAlloyProcessor();
    ~SaboteurAlloyProcessor() override = default;

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
        float emberPercent;
        float sparkPercent;
        float mixPercent;
        float outGainDb;
        bool autoMakeup;
    };

    static constexpr int numFactoryPresets = 8;
    static const std::array<FactoryPreset, numFactoryPresets> factoryPresets;

    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder; // 2048
    static constexpr int numBins = fftSize / 2;   // 1024

    void getVisualSnapshot(std::vector<float>& outInSpec,
        std::vector<float>& outOutSpec,
        float& outEmberGrDb,
        float& outSparkGrDb,
        double& outSampleRate);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    struct SimpleBiquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;

        void reset() noexcept { z1 = 0.0f; z2 = 0.0f; }

        float process(float x) noexcept
        {
            float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }

        void setLowPass(double sr, float freq, float q = 0.7071f) noexcept
        {
            float w0 = juce::MathConstants<float>::twoPi * std::clamp(freq, 20.0f, static_cast<float> (sr * 0.48)) / static_cast<float> (sr);
            float cosw = std::cos(w0), sinw = std::sin(w0);
            float alpha = sinw / (2.0f * q);
            float a0Inv = 1.0f / (1.0f + alpha);
            b0 = ((1.0f - cosw) * 0.5f) * a0Inv;
            b1 = (1.0f - cosw) * a0Inv;
            b2 = b0;
            a1 = (-2.0f * cosw) * a0Inv;
            a2 = (1.0f - alpha) * a0Inv;
        }

        void setHighPass(double sr, float freq, float q = 0.7071f) noexcept
        {
            float w0 = juce::MathConstants<float>::twoPi * std::clamp(freq, 20.0f, static_cast<float> (sr * 0.48)) / static_cast<float> (sr);
            float cosw = std::cos(w0), sinw = std::sin(w0);
            float alpha = sinw / (2.0f * q);
            float a0Inv = 1.0f / (1.0f + alpha);
            b0 = ((1.0f + cosw) * 0.5f) * a0Inv;
            b1 = -(1.0f + cosw) * a0Inv;
            b2 = b0;
            a1 = (-2.0f * cosw) * a0Inv;
            a2 = (1.0f - alpha) * a0Inv;
        }
    };

    struct ChannelState
    {
        // Crossover < 600 Hz (Ember) et > 2000 Hz (Spark)
        SimpleBiquad lowLPF1, lowLPF2;
        SimpleBiquad highHPF1, highHPF2;

        // Post-filtrage doux des harmoniques générées pour protéger les médiums (600 Hz - 2 kHz)
        SimpleBiquad emberPostLPF;
        SimpleBiquad sparkPostHPF;

        // Enveloppes des compresseurs Opto (Ember) et FET 1176 (Spark)
        float optoEnv = 0.0f;
        float fetEnv = 0.0f;
        float dcBlockEmber = 0.0f;

        void reset() noexcept
        {
            lowLPF1.reset(); lowLPF2.reset();
            highHPF1.reset(); highHPF2.reset();
            emberPostLPF.reset();
            sparkPostHPF.reset();
            optoEnv = 0.0f;
            fetEnv = 0.0f;
            dcBlockEmber = 0.0f;
        }
    };

    double currentSampleRate = 48000.0;
    int currentPresetIndex = 0;
    std::vector<ChannelState> channelStates;

    // Analyseur FFT pour l'interface graphique à 60 FPS
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
    float visEmberGrDb = 0.0f;
    float visSparkGrDb = 0.0f;

    void updateFilterCoefficients(ChannelState& state);
    void pushVisualSample(float inSample, float outSample, float emberGrDb, float sparkGrDb);
    void computeVisualFft();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SaboteurAlloyProcessor)
};