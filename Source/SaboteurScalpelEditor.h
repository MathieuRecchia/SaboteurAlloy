#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "SaboteurScalpelProcessor.h"

class ScalpelTitaniumLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ScalpelTitaniumLookAndFeel();
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, const float rotaryStartAngle,
        const float rotaryEndAngle, juce::Slider& slider) override;
};

class SaboteurScalpelEditor : public juce::AudioProcessorEditor,
    private juce::Timer
{
public:
    explicit SaboteurScalpelEditor(SaboteurScalpelProcessor&);
    ~SaboteurScalpelEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawSurgicalVisualizer(juce::Graphics& g, juce::Rectangle<int> bounds);
    void setupKnob(juce::Slider& slider, juce::Label& label, const juce::String& text, juce::Colour accent);

    SaboteurScalpelProcessor& processorRef;
    ScalpelTitaniumLookAndFeel titaniumLookAndFeel;

    // Barre supérieure
    juce::ComboBox presetSelector;
    juce::ComboBox listenModeBox;
    juce::ComboBox soloBandBox;
    juce::ToggleButton phaseInvButton{ "180 PHASE" };

    // Section Gauche : Input + HPF / LPF
    juce::Slider inGainSlider, hpfFreqSlider, lpfFreqSlider, outGainSlider;
    juce::Label  inGainLabel, hpfFreqLabel, lpfFreqLabel, outGainLabel;
    juce::ComboBox hpfSlopeBox, lpfSlopeBox;

    // Section Centrale : 3 Bandes Chirurgicales (Freq, Gain, Q)
    juce::Slider b1FreqSlider, b1GainSlider, b1QSlider;
    juce::Slider b2FreqSlider, b2GainSlider, b2QSlider;
    juce::Slider b3FreqSlider, b3GainSlider, b3QSlider;

    juce::Label b1FreqLabel, b1GainLabel, b1QLabel;
    juce::Label b2FreqLabel, b2GainLabel, b2QLabel;
    juce::Label b3FreqLabel, b3GainLabel, b3QLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ComboAttachment>  listenModeAttach, soloBandAttach, hpfSlopeAttach, lpfSlopeAttach;
    std::unique_ptr<ButtonAttachment> phaseInvAttach;
    std::unique_ptr<SliderAttachment> inGainAttach, hpfFreqAttach, lpfFreqAttach, outGainAttach;
    std::unique_ptr<SliderAttachment> b1FreqAttach, b1GainAttach, b1QAttach;
    std::unique_ptr<SliderAttachment> b2FreqAttach, b2GainAttach, b2QAttach;
    std::unique_ptr<SliderAttachment> b3FreqAttach, b3GainAttach, b3QAttach;

    // Données graphiques 60 FPS
    std::vector<float> guiInSpec;
    std::vector<float> guiOutSpec;
    float guiInRmsDb = -90.0f;
    float guiOutRmsDb = -90.0f;
    double guiSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SaboteurScalpelEditor)
};