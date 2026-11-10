#include "SaboteurScalpelEditor.h"

ScalpelTitaniumLookAndFeel::ScalpelTitaniumLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffd8dee9));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff181c24));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff2e3440));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff181c24));
    setColour(juce::ComboBox::textColourId, juce::Colour(0xffeceff4));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff3b4252));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff181c24));
    setColour(juce::PopupMenu::textColourId, juce::Colour(0xffeceff4));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff5e81ac));
}

void ScalpelTitaniumLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, const float rotaryStartAngle,
    const float rotaryEndAngle, juce::Slider& slider)
{
    const float radius = static_cast<float> (juce::jmin(width / 2, height / 2)) - 5.0f;
    const float centreX = static_cast<float> (x) + static_cast<float> (width) * 0.5f;
    const float centreY = static_cast<float> (y) + static_cast<float> (height) * 0.5f;
    const float rx = centreX - radius;
    const float ry = centreY - radius;
    const float rw = radius * 2.0f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    const juce::Colour accent = slider.findColour(juce::Slider::rotarySliderFillColourId);

    // Corps Titane Brossé
    g.setColour(juce::Colour(0xff1b2029));
    g.fillEllipse(rx, ry, rw, rw);

    g.setColour(juce::Colour(0xff2e3440));
    g.drawEllipse(rx, ry, rw, rw, 1.5f);

    // Piste de fond
    juce::Path bgArc;
    bgArc.addCentredArc(centreX, centreY, radius - 3.0f, radius - 3.0f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(0xff252b37));
    g.strokePath(bgArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Arc actif Bleu Glacier
    juce::Path valArc;
    valArc.addCentredArc(centreX, centreY, radius - 3.0f, radius - 3.0f, 0.0f, rotaryStartAngle, angle, true);
    g.setColour(accent);
    g.strokePath(valArc, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Aiguille de précision
    juce::Path pointer;
    const float ptrLen = radius * 0.68f;
    pointer.addRoundedRectangle(-1.5f, -radius + 4.0f, 3.0f, ptrLen, 1.0f);
    pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
    g.setColour(juce::Colour(0xffeceff4));
    g.fillPath(pointer);
}

SaboteurScalpelEditor::SaboteurScalpelEditor(SaboteurScalpelProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setLookAndFeel(&titaniumLookAndFeel);

    // Sélecteur de presets d'usine
    for (int i = 0; i < SaboteurScalpelProcessor::numFactoryPresets; ++i)
        presetSelector.addItem(processorRef.getProgramName(i), i + 1);
    presetSelector.setSelectedId(processorRef.getCurrentProgram() + 1, juce::dontSendNotification);
    presetSelector.onChange = [this]
        {
            processorRef.setCurrentProgram(presetSelector.getSelectedId() - 1);
        };
    addAndMakeVisible(presetSelector);

    listenModeBox.addItemList({ "STEREO", "MONO SUM", "MID ONLY", "SIDE ONLY" }, 1);
    addAndMakeVisible(listenModeBox);
    listenModeAttach = std::make_unique<ComboAttachment>(processorRef.apvts, "listen_mode", listenModeBox);

    soloBandBox.addItemList({ "LOUPE: OFF", "SOLO B1 (LOW)", "SOLO B2 (MID)", "SOLO B3 (HIGH)" }, 1);
    addAndMakeVisible(soloBandBox);
    soloBandAttach = std::make_unique<ComboAttachment>(processorRef.apvts, "solo_band", soloBandBox);

    phaseInvButton.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffd8dee9));
    phaseInvButton.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff88c0d0));
    addAndMakeVisible(phaseInvButton);
    phaseInvAttach = std::make_unique<ButtonAttachment>(processorRef.apvts, "phase_inv", phaseInvButton);

    const juce::Colour iceBlue(0xff88c0d0);
    const juce::Colour frostCyan(0xff8fbcbb);
    const juce::Colour steelBlue(0xff5e81ac);

    // Contrôles d'extrémités (In / HPF / LPF / Out)
    setupKnob(inGainSlider, inGainLabel, "IN TRIM", steelBlue);
    setupKnob(hpfFreqSlider, hpfFreqLabel, "HPF FREQ", iceBlue);
    setupKnob(lpfFreqSlider, lpfFreqLabel, "LPF FREQ", iceBlue);
    setupKnob(outGainSlider, outGainLabel, "OUT TRIM", steelBlue);

    hpfSlopeBox.addItemList({ "HPF OFF", "12 dB/oct", "24 dB/oct" }, 1);
    lpfSlopeBox.addItemList({ "LPF OFF", "12 dB/oct", "24 dB/oct" }, 1);
    addAndMakeVisible(hpfSlopeBox);
    addAndMakeVisible(lpfSlopeBox);

    inGainAttach = std::make_unique<SliderAttachment>(processorRef.apvts, "in_gain", inGainSlider);
    hpfFreqAttach = std::make_unique<SliderAttachment>(processorRef.apvts, "hpf_freq", hpfFreqSlider);
    hpfSlopeAttach = std::make_unique<ComboAttachment>(processorRef.apvts, "hpf_slope", hpfSlopeBox);
    lpfFreqAttach = std::make_unique<SliderAttachment>(processorRef.apvts, "lpf_freq", lpfFreqSlider);
    lpfSlopeAttach = std::make_unique<ComboAttachment>(processorRef.apvts, "lpf_slope", lpfSlopeBox);
    outGainAttach = std::make_unique<SliderAttachment>(processorRef.apvts, "out_gain", outGainSlider);

    // 3 Bandes Paramétriques à Q Proportionnel
    setupKnob(b1FreqSlider, b1FreqLabel, "B1 FREQ", frostCyan);
    setupKnob(b1GainSlider, b1GainLabel, "B1 GAIN", iceBlue);
    setupKnob(b1QSlider, b1QLabel, "B1 Q", steelBlue);

    setupKnob(b2FreqSlider, b2FreqLabel, "B2 FREQ", frostCyan);
    setupKnob(b2GainSlider, b2GainLabel, "B2 GAIN", iceBlue);
    setupKnob(b2QSlider, b2QLabel, "B2 Q", steelBlue);

    setupKnob(b3FreqSlider, b3FreqLabel, "B3 FREQ", frostCyan);
    setupKnob(b3GainSlider, b3GainLabel, "B3 GAIN", iceBlue);
    setupKnob(b3QSlider, b3QLabel, "B3 Q", steelBlue);

    b1FreqAttach = std::make_unique<SliderAttachment>(processorRef.apvts, "b1_freq", b1FreqSlider);
    b1GainAttach = std::make_unique<SliderAttachment>(processorRef.apvts, "b1_gain", b1GainSlider);
    b1QAttach = std::make_unique<SliderAttachment>(processorRef.apvts, "b1_q", b1QSlider);

    b2FreqAttach = std::make_unique<SliderAttachment>(processorRef.apvts, "b2_freq", b2FreqSlider);
    b2GainAttach = std::make_unique<SliderAttachment>(processorRef.apvts, "b2_gain", b2GainSlider);
    b2QAttach = std::make_unique<SliderAttachment>(processorRef.apvts, "b2_q", b2QSlider);

    b3FreqAttach = std::make_unique<SliderAttachment>(processorRef.apvts, "b3_freq", b3FreqSlider);
    b3GainAttach = std::make_unique<SliderAttachment>(processorRef.apvts, "b3_gain", b3GainSlider);
    b3QAttach = std::make_unique<SliderAttachment>(processorRef.apvts, "b3_q", b3QSlider);

    setSize(920, 560);
    startTimerHz(60);
}

SaboteurScalpelEditor::~SaboteurScalpelEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void SaboteurScalpelEditor::setupKnob(juce::Slider& slider, juce::Label& label,
    const juce::String& text, juce::Colour accent)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 18);
    slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, juce::Colour(0xffd8dee9));
    addAndMakeVisible(label);
}

void SaboteurScalpelEditor::timerCallback()
{
    processorRef.getVisualSnapshot(guiInSpec, guiOutSpec, guiInRmsDb, guiOutRmsDb, guiSampleRate);
    repaint();
}

void SaboteurScalpelEditor::paint(juce::Graphics& g)
{
    // Fond Arctic Titanium (#12151B)
    g.fillAll(juce::Colour(0xff12151b));

    // En-tête
    g.setColour(juce::Colour(0xff181c24));
    g.fillRect(0, 0, getWidth(), 48);
    g.setColour(juce::Colour(0xff2e3440));
    g.drawLine(0.0f, 48.0f, static_cast<float> (getWidth()), 48.0f, 1.0f);

    g.setColour(juce::Colour(0xff88c0d0));
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawText("SABOTEUR SCALPEL", 18, 8, 240, 20, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff81a1c1));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("64-BIT ZERO-COLOR SURGICAL EQUALIZER", 18, 26, 260, 16, juce::Justification::centredLeft);

    // Analyseur Spectral + Courbe de Filtre 64-bit
    drawSurgicalVisualizer(g, juce::Rectangle<int>(18, 58, getWidth() - 36, 220));

    // Cadres des 5 modules (In/HPF, Band 1, Band 2, Band 3, LPF/Out)
    const int panelY = 290;
    const int panelH = 254;
    const int colW = (getWidth() - 36 - 32) / 5;

    const char* titles[5] = {
        "1. INPUT & LOW CUT",
        "2. BAND 1 (40-500 Hz)",
        "3. BAND 2 (200-2.5k Hz)",
        "4. BAND 3 (1.2k-16k Hz)",
        "5. HIGH CUT & OUTPUT"
    };

    for (int i = 0; i < 5; ++i)
    {
        int px = 18 + i * (colW + 8);
        juce::Rectangle<float> box(static_cast<float>(px), static_cast<float>(panelY),
            static_cast<float>(colW), static_cast<float>(panelH));
        g.setColour(juce::Colour(0xff181c24));
        g.fillRoundedRectangle(box, 6.0f);
        g.setColour(juce::Colour(0xff2e3440));
        g.drawRoundedRectangle(box, 6.0f, 1.2f);

        g.setColour(juce::Colour(0xff88c0d0));
        g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        g.drawText(titles[i], px + 6, panelY + 6, colW - 12, 18, juce::Justification::centred);
    }
}

void SaboteurScalpelEditor::drawSurgicalVisualizer(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(juce::Colour(0xff0d1015));
    g.fillRoundedRectangle(bounds.toFloat(), 6.0f);
    g.setColour(juce::Colour(0xff2e3440));
    g.drawRoundedRectangle(bounds.toFloat(), 6.0f, 1.2f);

    const float x0 = static_cast<float> (bounds.getX());
    const float y0 = static_cast<float> (bounds.getY());
    const float w = static_cast<float> (bounds.getWidth());
    const float h = static_cast<float> (bounds.getHeight());
    const float midY = y0 + h * 0.5f;

    // Grille fréquentielle (20 Hz -> 20 kHz) et ligne 0 dB
    g.setColour(juce::Colour(0xff1f242f));
    g.drawHorizontalLine(static_cast<int> (midY), x0, x0 + w);
    g.drawHorizontalLine(static_cast<int> (y0 + h * 0.25f), x0, x0 + w);
    g.drawHorizontalLine(static_cast<int> (y0 + h * 0.75f), x0, x0 + w);

    const float gridFreqs[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
    g.setFont(juce::FontOptions(10.0f));
    for (float gf : gridFreqs)
    {
        float normX = std::log10(gf / 20.0f) / 3.0f;
        float gx = x0 + normX * w;
        g.setColour(juce::Colour(0xff1f242f));
        g.drawVerticalLine(static_cast<int> (gx), y0, y0 + h);
        g.setColour(juce::Colour(0xff4c566a));
        juce::String lbl = (gf >= 1000.0f) ? juce::String(gf / 1000.0f, 0) + "k" : juce::String(static_cast<int> (gf));
        g.drawText(lbl, static_cast<int> (gx) + 3, static_cast<int> (y0 + h - 16.0f), 32, 14, juce::Justification::left);
    }

    // Tracé des spectres FFT (Entrée en gris acier, Sortie en cyan glacier translucide)
    if (guiInSpec.size() > 2 && guiOutSpec.size() > 2)
    {
        juce::Path inPath, outPath;
        inPath.startNewSubPath(x0, y0 + h);
        outPath.startNewSubPath(x0, y0 + h);

        const int numBins = static_cast<int> (guiInSpec.size()) - 1;
        const float binHz = static_cast<float> (guiSampleRate) / static_cast<float> (numBins * 2);

        for (int i = 1; i <= numBins; ++i)
        {
            float freq = std::max(20.0f, i * binHz);
            if (freq > 20000.0f) break;

            float normX = std::clamp(std::log10(freq / 20.0f) / 3.0f, 0.0f, 1.0f);
            float px = x0 + normX * w;

            float inDb = juce::Decibels::gainToDecibels(guiInSpec[i], -80.0f);
            float outDb = juce::Decibels::gainToDecibels(guiOutSpec[i], -80.0f);

            float inY = y0 + h - std::clamp((inDb + 80.0f) / 80.0f, 0.0f, 1.0f) * (h - 10.0f);
            float outY = y0 + h - std::clamp((outDb + 80.0f) / 80.0f, 0.0f, 1.0f) * (h - 10.0f);

            inPath.lineTo(px, inY);
            outPath.lineTo(px, outY);
        }

        inPath.lineTo(x0 + w, y0 + h);
        inPath.closeSubPath();
        outPath.lineTo(x0 + w, y0 + h);
        outPath.closeSubPath();

        g.setColour(juce::Colour(0x334c566a));
        g.fillPath(inPath);

        g.setColour(juce::Colour(0x3888c0d0));
        g.fillPath(outPath);
    }

    // Courbe analytique 64-bit des filtres (-24 dB à +24 dB)
    juce::Path eqCurve;
    const int numPoints = 240;
    for (int i = 0; i <= numPoints; ++i)
    {
        float normX = static_cast<float> (i) / static_cast<float> (numPoints);
        double freq = 20.0 * std::pow(1000.0, static_cast<double> (normX));
        float gainDb = processorRef.getFilterMagnitudeDbAtFrequency(freq);

        float px = x0 + normX * w;
        float py = midY - (gainDb / 24.0f) * (h * 0.45f);
        py = std::clamp(py, y0 + 4.0f, y0 + h - 4.0f);

        if (i == 0) eqCurve.startNewSubPath(px, py);
        else        eqCurve.lineTo(px, py);
    }

    g.setColour(juce::Colour(0xff88c0d0));
    g.strokePath(eqCurve, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Témoins RMS Entrée / Sortie en haut à droite du graphe
    g.setColour(juce::Colour(0xffd8dee9));
    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    juce::String rmsTxt = "IN: " + juce::String(guiInRmsDb, 1) + " dB   |   OUT: " + juce::String(guiOutRmsDb, 1) + " dB";
    g.drawText(rmsTxt, bounds.getRight() - 230, bounds.getY() + 8, 220, 18, juce::Justification::centredRight);
}

void SaboteurScalpelEditor::resized()
{
    presetSelector.setBounds(290, 10, 230, 28);
    listenModeBox.setBounds(530, 10, 120, 28);
    soloBandBox.setBounds(660, 10, 135, 28);
    phaseInvButton.setBounds(805, 10, 100, 28);

    const int panelY = 290;
    const int colW = (getWidth() - 36 - 32) / 5;

    auto placeCol1And5 = [&](int colIdx, juce::Slider& topKnob, juce::Label& topLbl,
        juce::Slider& botKnob, juce::Label& botLbl, juce::ComboBox& slopeCombo)
        {
            int px = 18 + colIdx * (colW + 8);
            topLbl.setBounds(px + 8, panelY + 26, colW - 16, 16);
            topKnob.setBounds(px + 12, panelY + 42, colW - 24, 82);

            botLbl.setBounds(px + 8, panelY + 128, colW - 16, 16);
            botKnob.setBounds(px + 12, panelY + 144, colW - 24, 74);
            slopeCombo.setBounds(px + 18, panelY + 222, colW - 36, 24);
        };

    placeCol1And5(0, inGainSlider, inGainLabel, hpfFreqSlider, hpfFreqLabel, hpfSlopeBox);
    placeCol1And5(4, outGainSlider, outGainLabel, lpfFreqSlider, lpfFreqLabel, lpfSlopeBox);

    auto placeBandCol = [&](int colIdx,
        juce::Slider& fKnob, juce::Label& fLbl,
        juce::Slider& gKnob, juce::Label& gLbl,
        juce::Slider& qKnob, juce::Label& qLbl)
        {
            int px = 18 + colIdx * (colW + 8);
            gLbl.setBounds(px + 8, panelY + 26, colW - 16, 16);
            gKnob.setBounds(px + 16, panelY + 42, colW - 32, 92);

            int halfW = (colW - 12) / 2;
            fLbl.setBounds(px + 4, panelY + 140, halfW, 16);
            fKnob.setBounds(px + 4, panelY + 156, halfW, 88);

            qLbl.setBounds(px + 8 + halfW, panelY + 140, halfW, 16);
            qKnob.setBounds(px + 8 + halfW, panelY + 156, halfW, 88);
        };

    placeBandCol(1, b1FreqSlider, b1FreqLabel, b1GainSlider, b1GainLabel, b1QSlider, b1QLabel);
    placeBandCol(2, b2FreqSlider, b2FreqLabel, b2GainSlider, b2GainLabel, b2QSlider, b2QLabel);
    placeBandCol(3, b3FreqSlider, b3FreqLabel, b3GainSlider, b3GainLabel, b3QSlider, b3QLabel);
}