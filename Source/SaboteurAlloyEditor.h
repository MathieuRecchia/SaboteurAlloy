#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "SaboteurAlloyProcessor.h"

class AlloySoftLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AlloySoftLookAndFeel();
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, const float rotaryStartAngle,
        const float rotaryEndAngle, juce::Slider& slider) override;
};

class SaboteurAlloyEditor : public juce::AudioProcessorEditor,
    private juce::Timer
{
public:
    explicit SaboteurAlloyEditor(SaboteurAlloyProcessor&);
    ~SaboteurAlloyEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawThreeBandVisualizer(juce::Graphics& g, juce::Rectangle<int> bounds);
    void setupKnob(juce::Slider& slider, juce::Label& label, const juce::String& text, juce::Colour accentColour);

    SaboteurAlloyProcessor& processorRef;
    AlloySoftLookAndFeel alloyLookAndFeel;

    // Barre supérieure : Sélecteur de Presets & Auto Makeup
    juce::ComboBox presetSelector;
    juce::ToggleButton autoMakeupButton{ "AUTO MAKEUP" };

    // 2 Macro-Contrôles Centraux (Ember & Spark) + 3 Contrôles Périphériques (In, Mix, Out)
    juce::Slider inGainSlider, emberSlider, sparkSlider, mixSlider, outGainSlider;
    juce::Label  inGainLabel, emberLabel, sparkLabel, mixLabel, outGainLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ButtonAttachment> autoMakeupAttach;
    std::unique_ptr<SliderAttachment> inGainAttach, emberAttach, sparkAttach, mixAttach, outGainAttach;

    // Données graphiques 60 FPS
    std::vector<float> guiInSpec;
    std::vector<float> guiOutSpec;
    float guiEmberGrDb = 0.0f;
    float guiSparkGrDb = 0.0f;
    double guiSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SaboteurAlloyEditor)
};