#include "SaboteurAlloyEditor.h"

namespace AlloyColours
{
    const juce::Colour background{ 0xff181a20 };
    const juce::Colour panelBg{ 0xff21242d };
    const juce::Colour borderSoft{ 0xff323744 };
    const juce::Colour emberWarm{ 0xffe08a70 }; // Terracotta / Rose Cuivré doux (< 600 Hz)
    const juce::Colour coreSand{ 0xffd8d2c5 }; // Sable Crème neutre (600 Hz - 2 kHz)
    const juce::Colour sparkMist{ 0xff7aa6c2 }; // Bleu Sauge / Lavande doux (> 2 kHz)
    const juce::Colour textPrimary{ 0xffeceae4 };
    const juce::Colour textMuted{ 0xff9499a6 };
}

AlloySoftLookAndFeel::AlloySoftLookAndFeel()
{
    setColour(juce::ComboBox::backgroundColourId, AlloyColours::panelBg);
    setColour(juce::ComboBox::textColourId, AlloyColours::textPrimary);
    setColour(juce::ComboBox::outlineColourId, AlloyColours::borderSoft);
    setColour(juce::PopupMenu::backgroundColourId, AlloyColours::panelBg);
    setColour(juce::PopupMenu::textColourId, AlloyColours::textPrimary);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, AlloyColours::emberWarm.withAlpha(0.35f));
}

void AlloySoftLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, const float rotaryStartAngle,
    const float rotaryEndAngle, juce::Slider& slider)
{
    const float radius = static_cast<float> (juce::jmin(width / 2, height / 2)) - 8.0f;
    const float centreX = static_cast<float> (x) + static_cast<float> (width) * 0.5f;
    const float centreY = static_cast<float> (y) + static_cast<float> (height) * 0.5f;
    const float rx = centreX - radius;
    const float ry = centreY - radius;
    const float rw = radius * 2.0f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    const juce::Colour accent = slider.findColour(juce::Slider::rotarySliderFillColourId);

    // Halo doux externe
    g.setColour(accent.withAlpha(0.08f));
    g.fillEllipse(rx - 3.0f, ry - 3.0f, rw + 6.0f, rw + 6.0f);

    // Corps du potentiomètre
    juce::ColourGradient bodyGrad(AlloyColours::panelBg.brighter(0.08f), centreX, ry,
        AlloyColours::background.darker(0.15f), centreX, ry + rw, false);
    g.setGradientFill(bodyGrad);
    g.fillEllipse(rx + 4.0f, ry + 4.0f, rw - 8.0f, rw - 8.0f);

    g.setColour(AlloyColours::borderSoft);
    g.drawEllipse(rx + 4.0f, ry + 4.0f, rw - 8.0f, rw - 8.0f, 1.5f);

    // Piste de fond
    juce::Path backgroundArc;
    backgroundArc.addCentredArc(centreX, centreY, radius, radius, 0.0f,
        rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(AlloyColours::borderSoft.darker(0.2f));
    g.strokePath(backgroundArc, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Arc de valeur coloré
    juce::Path valueArc;
    valueArc.addCentredArc(centreX, centreY, radius, radius, 0.0f,
        rotaryStartAngle, angle, true);
    g.setColour(accent);
    g.strokePath(valueArc, juce::PathStrokeType(3.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Aiguille indicatrice
    juce::Path pointer;
    const float pointerLength = radius * 0.62f;
    pointer.addRoundedRectangle(-1.8f, -radius + 6.0f, 3.6f, pointerLength, 1.5f);
    pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
    g.setColour(AlloyColours::textPrimary);
    g.fillPath(pointer);
}

SaboteurAlloyEditor::SaboteurAlloyEditor(SaboteurAlloyProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setLookAndFeel(&alloyLookAndFeel);
    setSize(780, 500);

    guiInSpec.assign(SaboteurAlloyProcessor::numBins + 1, 0.0f);
    guiOutSpec.assign(SaboteurAlloyProcessor::numBins + 1, 0.0f);

    // Configuration du sélecteur de Presets
    for (int i = 0; i < SaboteurAlloyProcessor::numFactoryPresets; ++i)
        presetSelector.addItem(SaboteurAlloyProcessor::factoryPresets[i].name, i + 1);

    presetSelector.setSelectedId(processorRef.getCurrentProgram() + 1, juce::dontSendNotification);
    presetSelector.onChange = [this]
        {
            const int idx = presetSelector.getSelectedId() - 1;
            if (idx >= 0)
                processorRef.setCurrentProgram(idx);
        };
    addAndMakeVisible(presetSelector);

    // Bouton Auto Makeup
    autoMakeupButton.setColour(juce::ToggleButton::textColourId, AlloyColours::textPrimary);
    autoMakeupButton.setColour(juce::ToggleButton::tickColourId, AlloyColours::emberWarm);
    addAndMakeVisible(autoMakeupButton);
    autoMakeupAttach = std::make_unique<ButtonAttachment>(processorRef.apvts, "auto_makeup", autoMakeupButton);

    // Configuration des 5 potentiomètres avec leurs couleurs douces dédiées
    setupKnob(inGainSlider, inGainLabel, "IN GAIN", AlloyColours::coreSand);
    setupKnob(emberSlider, emberLabel, "EMBER (< 600 Hz)", AlloyColours::emberWarm);
    setupKnob(sparkSlider, sparkLabel, "SPARK (> 2 kHz)", AlloyColours::sparkMist);
    setupKnob(mixSlider, mixLabel, "WET / DRY MIX", AlloyColours::coreSand);
    setupKnob(outGainSlider, outGainLabel, "OUT GAIN", AlloyColours::coreSand);

    inGainAttach = std::make_unique<SliderAttachment>(processorRef.apvts, "in_gain", inGainSlider);
    emberAttach = std::make_unique<SliderAttachment>(processorRef.apvts, "ember", emberSlider);
    sparkAttach = std::make_unique<SliderAttachment>(processorRef.apvts, "spark", sparkSlider);
    mixAttach = std::make_unique<SliderAttachment>(processorRef.apvts, "mix", mixSlider);
    outGainAttach = std::make_unique<SliderAttachment>(processorRef.apvts, "out_gain", outGainSlider);

    startTimerHz(60);
}

SaboteurAlloyEditor::~SaboteurAlloyEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void SaboteurAlloyEditor::setupKnob(juce::Slider& slider, juce::Label& label,
    const juce::String& text, juce::Colour accentColour)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 20);
    slider.setColour(juce::Slider::rotarySliderFillColourId, accentColour);
    slider.setColour(juce::Slider::textBoxTextColourId, AlloyColours::textPrimary);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::FontOptions(12.5f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, accentColour);
    addAndMakeVisible(label);
}

void SaboteurAlloyEditor::timerCallback()
{
    processorRef.getVisualSnapshot(guiInSpec, guiOutSpec, guiEmberGrDb, guiSparkGrDb, guiSampleRate);
    repaint();
}

void SaboteurAlloyEditor::paint(juce::Graphics& g)
{
    g.fillAll(AlloyColours::background);

    // En-tête doux Saboteur Alloy
    auto headerArea = getLocalBounds().removeFromTop(54).reduced(18, 8);
    g.setColour(AlloyColours::textPrimary);
    g.setFont(juce::FontOptions(21.0f, juce::Font::bold));
    g.drawText("SABOTEUR ALLOY", headerArea.removeFromLeft(240), juce::Justification::centredLeft);

    g.setColour(AlloyColours::textMuted);
    g.setFont(juce::FontOptions(12.0f));
    g.drawText("PARALLEL MULTIBAND OPTO/TUBE & FET PROCESSOR", headerArea.removeFromLeft(310), juce::Justification::centredLeft);

    // Zone de l'analyseur spectral 3 bandes
    juce::Rectangle<int> visBounds(18, 60, getWidth() - 36, 215);
    drawThreeBandVisualizer(g, visBounds);

    // Panneau inférieur des contrôles
    juce::Rectangle<int> controlsPanel(18, 288, getWidth() - 36, 194);
    g.setColour(AlloyColours::panelBg);
    g.fillRoundedRectangle(controlsPanel.toFloat(), 10.0f);
    g.setColour(AlloyColours::borderSoft);
    g.drawRoundedRectangle(controlsPanel.toFloat(), 10.0f, 1.2f);

    // Sous-titres descriptifs sous EMBER et SPARK
    g.setColour(AlloyColours::textMuted);
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("Tube Saturation + Slow Opto", 175, 458, 200, 18, juce::Justification::centred);
    g.drawText("FET Saturation + Fast 1176", 405, 458, 200, 18, juce::Justification::centred);
}

void SaboteurAlloyEditor::drawThreeBandVisualizer(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(AlloyColours::panelBg.darker(0.25f));
    g.fillRoundedRectangle(bounds.toFloat(), 10.0f);

    const float x0 = static_cast<float> (bounds.getX());
    const float y0 = static_cast<float> (bounds.getY());
    const float w = static_cast<float> (bounds.getWidth());
    const float h = static_cast<float> (bounds.getHeight());

    auto freqToX = [&](float freq)
        {
            const float norm = std::log10(std::clamp(freq, 20.0f, 20000.0f) / 20.0f) / 3.0f; // log10(20000/20) = 3
            return x0 + norm * w;
        };

    const float x600 = freqToX(600.0f);
    const float x2000 = freqToX(2000.0f);

    // Coloration pastel douce des 3 zones spectrales
    g.setColour(AlloyColours::emberWarm.withAlpha(0.07f));
    g.fillRect(x0, y0, x600 - x0, h);

    g.setColour(AlloyColours::coreSand.withAlpha(0.04f));
    g.fillRect(x600, y0, x2000 - x600, h);

    g.setColour(AlloyColours::sparkMist.withAlpha(0.07f));
    g.fillRect(x2000, y0, (x0 + w) - x2000, h);

    // Lignes de séparation à 600 Hz et 2 kHz
    g.setColour(AlloyColours::emberWarm.withAlpha(0.45f));
    g.drawVerticalLine(static_cast<int> (x600), y0, y0 + h);

    g.setColour(AlloyColours::sparkMist.withAlpha(0.45f));
    g.drawVerticalLine(static_cast<int> (x2000), y0, y0 + h);

    // Étiquettes des 3 régions
    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    g.setColour(AlloyColours::emberWarm);
    g.drawText("EMBER ZONE (< 600 Hz : Tube + Opto)", static_cast<int> (x0 + 10), static_cast<int> (y0 + 8), 230, 18, juce::Justification::centredLeft);

    g.setColour(AlloyColours::coreSand.withAlpha(0.85f));
    g.drawText("UNTOUCHED CORE (600 Hz - 2 kHz)", static_cast<int> (x600), static_cast<int> (y0 + 8), static_cast<int> (x2000 - x600), 18, juce::Justification::centred);

    g.setColour(AlloyColours::sparkMist);
    g.drawText("SPARK ZONE (> 2 kHz : FET + 1176)", static_cast<int> (x2000 + 10), static_cast<int> (y0 + 8), 230, 18, juce::Justification::centredLeft);

    // Courbes spectrales Entrée (gris doux) et Sortie (crème lumineux)
    if (!guiInSpec.empty() && !guiOutSpec.empty())
    {
        juce::Path inPath, outPath, outFill;
        const float binHz = static_cast<float> (guiSampleRate) / static_cast<float> (SaboteurAlloyProcessor::fftSize);

        bool started = false;
        for (int i = 1; i <= SaboteurAlloyProcessor::numBins; ++i)
        {
            const float freq = i * binHz;
            if (freq < 20.0f || freq > 20000.0f)
                continue;

            const float px = freqToX(freq);
            const float inDb = juce::Decibels::gainToDecibels(guiInSpec[i] + 1e-6f);
            const float outDb = juce::Decibels::gainToDecibels(guiOutSpec[i] + 1e-6f);

            const float pyIn = y0 + h - std::clamp((inDb + 72.0f) / 72.0f, 0.0f, 1.0f) * (h - 34.0f);
            const float pyOut = y0 + h - std::clamp((outDb + 72.0f) / 72.0f, 0.0f, 1.0f) * (h - 34.0f);

            if (!started)
            {
                inPath.startNewSubPath(px, pyIn);
                outPath.startNewSubPath(px, pyOut);
                outFill.startNewSubPath(px, y0 + h);
                outFill.lineTo(px, pyOut);
                started = true;
            }
            else
            {
                inPath.lineTo(px, pyIn);
                outPath.lineTo(px, pyOut);
                outFill.lineTo(px, pyOut);
            }
        }

        if (started)
        {
            outFill.lineTo(x0 + w, y0 + h);
            outFill.closeSubPath();

            g.setColour(AlloyColours::coreSand.withAlpha(0.14f));
            g.fillPath(outFill);

            g.setColour(AlloyColours::textMuted.withAlpha(0.45f));
            g.strokePath(inPath, juce::PathStrokeType(1.4f));

            g.setColour(AlloyColours::textPrimary.withAlpha(0.90f));
            g.strokePath(outPath, juce::PathStrokeType(2.0f));
        }
    }

    // Jauges horizontales de réduction de gain Opto (Ember) et FET (Spark)
    const float emberBarW = std::clamp(guiEmberGrDb / 12.0f, 0.0f, 1.0f) * 140.0f;
    g.setColour(AlloyColours::background.withAlpha(0.7f));
    g.fillRoundedRectangle(x0 + 12.0f, y0 + 30.0f, 140.0f, 10.0f, 4.0f);
    g.setColour(AlloyColours::emberWarm);
    g.fillRoundedRectangle(x0 + 12.0f, y0 + 30.0f, emberBarW, 10.0f, 4.0f);

    const float sparkBarW = std::clamp(guiSparkGrDb / 12.0f, 0.0f, 1.0f) * 140.0f;
    g.setColour(AlloyColours::background.withAlpha(0.7f));
    g.fillRoundedRectangle(x2000 + 12.0f, y0 + 30.0f, 140.0f, 10.0f, 4.0f);
    g.setColour(AlloyColours::sparkMist);
    g.fillRoundedRectangle(x2000 + 12.0f, y0 + 30.0f, sparkBarW, 10.0f, 4.0f);

    g.setColour(AlloyColours::borderSoft);
    g.drawRoundedRectangle(bounds.toFloat(), 10.0f, 1.2f);
}

void SaboteurAlloyEditor::resized()
{
    presetSelector.setBounds(getWidth() - 365, 14, 210, 26);
    autoMakeupButton.setBounds(getWidth() - 140, 14, 122, 26);

    // Positionnement des 5 potentiomètres :
    // IN (petit à gauche) | EMBER (grand) | SPARK (grand) | MIX (petit) | OUT (petit à droite)
    inGainLabel.setBounds(36, 298, 115, 20);
    inGainSlider.setBounds(36, 320, 115, 130);

    emberLabel.setBounds(185, 294, 180, 22);
    emberSlider.setBounds(185, 316, 180, 142);

    sparkLabel.setBounds(415, 294, 180, 22);
    sparkSlider.setBounds(415, 316, 180, 142);

    mixLabel.setBounds(605, 298, 70, 20);
    mixSlider.setBounds(605, 320, 70, 130);

    outGainLabel.setBounds(682, 298, 70, 20);
    outGainSlider.setBounds(682, 320, 70, 130);
}