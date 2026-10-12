#include "PluginEditor.h"

using namespace carat::ui;

static juce::String fmtLufs (float v) { return v <= -69.9f ? juce::String ("--.-") : juce::String (v, 1); }

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Palette::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, Palette::text);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float start, float end, juce::Slider& s)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto c = bounds.getCentre();
    const float radius = size * 0.5f;
    const float angle = start + pos * (end - start);
    const float alpha = s.isEnabled() ? 1.0f : 0.35f;

    juce::Path track;
    track.addCentredArc (c.x, c.y, radius - 2.0f, radius - 2.0f, 0.0f, start, end, true);
    g.setColour (Palette::panelHi.withMultipliedAlpha (alpha));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (c.x, c.y, radius - 2.0f, radius - 2.0f, 0.0f, start, angle, true);
    g.setColour (Palette::gold.withMultipliedAlpha (alpha));
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float body = radius * 0.72f;
    juce::ColourGradient grad (juce::Colour (0xff3b3a3e).withMultipliedAlpha (alpha), c.x, c.y - body,
                               juce::Colour (0xff1a1a1c).withMultipliedAlpha (alpha), c.x, c.y + body, false);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - body, c.y - body, body * 2.0f, body * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f * alpha));
    g.drawEllipse (c.x - body, c.y - body, body * 2.0f, body * 2.0f, 1.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -body + 3.0f, 3.0f, body * 0.45f, 1.5f);
    g.setColour (Palette::text.withMultipliedAlpha (alpha));
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? Palette::gold : (highlighted ? Palette::panelHi : Palette::panel));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? Palette::gold : Palette::panelHi.brighter (0.25f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? Palette::bg : Palette::text);
    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
}

juce::Label* LookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (juce::FontOptions (12.0f));
    l->setColour (juce::Label::textColourId, Palette::text);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    return l;
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (260);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, Palette::text);
    addAndMakeVisible (slider);

    label.setText (name.toUpperCase(), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, Palette::textDim);
    addAndMakeVisible (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

//==============================================================================
ChoiceButtons::ChoiceButtons (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, juce::StringArray labels)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (labels[i]));
        b->onClick = [this, i] { if (attachment) attachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    attachment = std::make_unique<juce::ParameterAttachment> (*state.getParameter (paramId), [this] (float v)
    {
        selected = juce::jlimit (0, buttons.size() - 1, juce::roundToInt (v));
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setToggleState (i == selected, juce::dontSendNotification);
        if (onChange) onChange (selected);
    });
    attachment->sendInitialUpdate();
}

void ChoiceButtons::resized()
{
    auto r = getLocalBounds();
    const int w = r.getWidth() / juce::jmax (1, buttons.size());
    for (auto* b : buttons)
        b->setBounds (r.removeFromLeft (w).reduced (2, 0));
}

//==============================================================================
void TransferDisplay::setCurve (float driveLin, bool satOn, float satAmountDb, float ceilingLin)
{
    auto same = [] (float a, float b) { return std::abs (a - b) < 1.0e-6f; };
    if (same (driveLin, drive) && satOn == sat && same (satAmountDb, amount) && same (ceilingLin, ceiling)) return;
    drive = driveLin; sat = satOn; amount = satAmountDb; ceiling = ceilingLin;
    repaint();
}

void TransferDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (r, 8.0f);

    auto plot = r.reduced (34.0f, 16.0f).withTrimmedBottom (14.0f).withTrimmedLeft (0.0f);
    const float xMin = -30.0f, xMax = 6.0f, yMin = -30.0f, yMax = 3.0f;
    auto px = [&] (float db) { return plot.getX() + (db - xMin) / (xMax - xMin) * plot.getWidth(); };
    auto py = [&] (float db) { return plot.getBottom() - (db - yMin) / (yMax - yMin) * plot.getHeight(); };

    // griglia
    g.setFont (juce::FontOptions (9.5f));
    for (float d = -30.0f; d <= 6.0f; d += 6.0f)
    {
        g.setColour (Palette::panelHi);
        g.drawVerticalLine ((int) px (d), plot.getY(), plot.getBottom());
        g.setColour (Palette::textDim);
        g.drawText (juce::String ((int) d), juce::Rectangle<float> (px (d) - 14.0f, plot.getBottom() + 2.0f, 28.0f, 12.0f), juce::Justification::centred);
    }
    for (float d = -30.0f; d <= 0.0f; d += 6.0f)
    {
        g.setColour (Palette::panelHi);
        g.drawHorizontalLine ((int) py (d), plot.getX(), plot.getRight());
        g.setColour (Palette::textDim);
        g.drawText (juce::String ((int) d), juce::Rectangle<float> (plot.getX() - 30.0f, py (d) - 6.0f, 26.0f, 12.0f), juce::Justification::centredRight);
    }
    // 0 dBFS in ingresso e ceiling in uscita
    g.setColour (Palette::textDim.withAlpha (0.6f));
    g.drawVerticalLine ((int) px (0.0f), plot.getY(), plot.getBottom());
    const float ceilDb = carat::linToDb (ceiling);
    g.setColour (Palette::red.withAlpha (0.55f));
    g.drawHorizontalLine ((int) py (ceilDb), plot.getX(), plot.getRight());

    // linea unitaria
    {
        juce::Path unity;
        unity.startNewSubPath (px (xMin), py (xMin));
        unity.lineTo (px (yMax), py (yMax));
        const float dash[] = { 3.0f, 4.0f };
        juce::Path dashed;
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, unity, dash, 2);
        g.setColour (Palette::textDim.withAlpha (0.5f));
        g.fillPath (dashed);
    }

    // curva
    juce::Path curve;
    bool started = false;
    for (int i = 0; i <= 240; ++i)
    {
        const float xDb = xMin + (xMax - xMin) * (float) i / 240.0f;
        const float y = carat::ConverterStage::staticTransfer (carat::dbToLin (xDb), drive, amount, sat) * ceiling;
        const float yDb = juce::jlimit (yMin - 6.0f, yMax, carat::linToDb (y));
        const float X = px (xDb), Y = juce::jmax (plot.getY(), py (yDb));
        if (! started) { curve.startNewSubPath (X, Y); started = true; }
        else curve.lineTo (X, Y);
    }
    g.saveState();
    g.reduceClipRegion (plot.toNearestInt());
    g.setColour (Palette::gold);
    g.strokePath (curve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // punto live: picco del segnale in ingresso
    if (inputPeakDb > xMin)
    {
        const float xDb = juce::jmin (xMax, inputPeakDb);
        const float y = carat::ConverterStage::staticTransfer (carat::dbToLin (xDb), drive, amount, sat) * ceiling;
        const float X = px (xDb), Y = py (carat::linToDb (y));
        g.setColour (Palette::gold.withAlpha (0.25f));
        g.fillEllipse (X - 9.0f, Y - 9.0f, 18.0f, 18.0f);
        g.setColour (Palette::text);
        g.fillEllipse (X - 4.0f, Y - 4.0f, 8.0f, 8.0f);
    }
    g.restoreState();

    g.setColour (Palette::textDim);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("TRANSFER  (IN dBFS -> OUT dBFS)", r.reduced (10.0f, 4.0f).removeFromTop (14.0f), juce::Justification::centredRight);
}

//==============================================================================
void ReductionBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto top = r.removeFromTop (15.0f);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.setColour (Palette::textDim);
    g.drawText (title, top, juce::Justification::centredLeft);
    g.setColour (Palette::text);
    g.drawText ("-" + juce::String (value, 1) + " dB", top, juce::Justification::centredRight);

    auto bar = r.removeFromTop (9.0f);
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (bar, 3.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, value / range);
    g.setColour (Palette::gold);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * frac), 3.0f);
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto lbl = r.removeFromBottom (14.0f);
    g.setColour (Palette::textDim);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText (title, lbl, juce::Justification::centred);

    g.setColour (Palette::panel);
    g.fillRoundedRectangle (r, 3.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, (level + 48.0f) / 51.0f); // -48..+3 dBFS
    g.setColour (level > -0.05f ? Palette::red : Palette::gold.withAlpha (0.9f));
    g.fillRoundedRectangle (r.withTop (r.getBottom() - r.getHeight() * frac), 3.0f);
    const float y0 = r.getBottom() - r.getHeight() * (48.0f / 51.0f);
    g.setColour (Palette::text.withAlpha (0.5f));
    g.drawHorizontalLine ((int) y0, r.getX(), r.getRight());
}

//==============================================================================
LoudnessPanel::LoudnessPanel()
{
    addAndMakeVisible (resetButton);
}

void LoudnessPanel::setValues (float m, float s, float i, float tp, float gain, float ceilingDb)
{
    mom = m; st = s; integ = i; tpMax = tp; gainDb = gain; ceil = ceilingDb;
    repaint();
}

void LoudnessPanel::resized()
{
    resetButton.setBounds (getWidth() - 70, 10, 60, 20);
}

void LoudnessPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (r, 8.0f);

    auto area = r.reduced (14.0f, 10.0f);
    g.setColour (Palette::textDim);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("LOUDNESS  (OUT)", area.removeFromTop (20.0f), juce::Justification::centredLeft);

    auto row = [&] (const juce::String& name, const juce::String& value, const juce::String& unit, bool big, juce::Colour col)
    {
        auto line = area.removeFromTop (big ? 40.0f : 24.0f);
        g.setColour (Palette::textDim);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (name, line.removeFromLeft (100.0f), juce::Justification::centredLeft);
        auto u = line.removeFromRight (40.0f);
        g.drawText (unit, u, juce::Justification::centredLeft);
        g.setColour (col);
        g.setFont (juce::FontOptions (big ? 30.0f : 17.0f, juce::Font::bold));
        g.drawText (value, line.withTrimmedRight (6.0f), juce::Justification::centredRight);
    };

    row ("INTEGRATED", fmtLufs (integ), "LUFS", true, Palette::text);
    row ("SHORT-TERM", fmtLufs (st), "LUFS", false, Palette::text);
    row ("MOMENTARY", fmtLufs (mom), "LUFS", false, Palette::text);
    row ("TRUE PEAK MAX", tpMax <= -99.0f ? juce::String ("--.-") : juce::String (tpMax, 1), "dBTP", false,
         tpMax > ceil + 0.05f ? Palette::red : Palette::text);
    row ("LOUDER BY", (gainDb >= 0.0f ? "+" : "") + juce::String (gainDb, 1), "LU", false, Palette::gold);
}

//==============================================================================
CaratAudioProcessorEditor::CaratAudioProcessorEditor (CaratAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      input (p.apvts, "input", "Input"),
      satAmount (p.apvts, "satamt", "Amount"),
      ceiling (p.apvts, "ceiling", "Ceiling"),
      satButtons (p.apvts, "sat", { "OFF", "+3 dB", "+6 dB", "VAR" }),
      listenButtons (p.apvts, "listen", { "STEREO", "MID", "SIDE" })
{
    setLookAndFeel (&lnf);

    for (auto* c : std::initializer_list<juce::Component*> { &input, &satAmount, &ceiling, &satButtons, &listenButtons,
                                                             &transfer, &clipBar, &tpBar, &inMeter, &outMeter, &loudness })
        addAndMakeVisible (c);

    for (auto* b : { &tpButton, &deltaButton, &bypassButton, &matchButton })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (b);
    }
    tpAttach     = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "tplimit", tpButton);
    deltaAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "delta", deltaButton);
    bypassAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "bypass", bypassButton);
    matchAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "match", matchButton);

    satButtons.onChange = [this] (int s) { satAmount.setEnabled (s == 3); refreshCurve(); };
    satAmount.setEnabled (satButtons.getSelected() == 3);
    loudness.resetButton.onClick = [this] { proc.resetMeters.store (true); };

    setSize (976, 470);
    refreshCurve();
    startTimerHz (30);
}

CaratAudioProcessorEditor::~CaratAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void CaratAudioProcessorEditor::refreshCurve()
{
    const auto s = CaratAudioProcessor::satFromParams (satButtons.getSelected(),
                                                       proc.apvts.getRawParameterValue ("satamt")->load());
    transfer.setCurve (carat::dbToLin (proc.apvts.getRawParameterValue ("input")->load()), s.on, s.amountDb,
                       carat::dbToLin (proc.apvts.getRawParameterValue ("ceiling")->load()));
}

void CaratAudioProcessorEditor::timerCallback()
{
    refreshCurve();

    const float in = proc.meterInPeakDb.exchange (-100.0f);
    const float out = proc.meterOutPeakDb.exchange (-100.0f);
    const float clip = proc.meterClipDb.exchange (0.0f);
    const float tp = proc.meterTpLimitDb.exchange (0.0f);

    inS = in > inS ? in : inS - 1.2f;
    outS = out > outS ? out : outS - 1.2f;
    clipS = clip > clipS ? clip : clipS + (clip - clipS) * 0.15f;
    tpS = tp > tpS ? tp : tpS + (tp - tpS) * 0.15f;
    curvePeak = in > curvePeak ? in : curvePeak - 0.8f;

    inMeter.setValue (inS);
    outMeter.setValue (outS);
    clipBar.setValue (clipS);
    tpBar.setValue (tpS);
    transfer.setInputPeak (curvePeak);
    loudness.setValues (proc.lufsMomentary.load(), proc.lufsShort.load(), proc.lufsIntegrated.load(),
                        proc.truePeakMaxDb.load(), proc.loudnessGainDb.load(),
                        proc.apvts.getRawParameterValue ("ceiling")->load());
}

void CaratAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::bg);

    auto header = getLocalBounds().removeFromTop (54).reduced (22, 0);
    g.setColour (Palette::text);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("CARAT", header, juce::Justification::centredLeft);
    g.setColour (Palette::gold);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("MASTERING CONVERTER CLIPPER", header.withTrimmedLeft (112), juce::Justification::centredLeft);
    g.setColour (Palette::textDim);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("8x OVERSAMPLING", header, juce::Justification::centredRight);

    // pannello controlli
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (juce::Rectangle<float> (16.0f, 58.0f, 262.0f, 340.0f), 8.0f);
    g.setColour (Palette::textDim);
    g.drawText ("SOFT SATURATION", juce::Rectangle<int> (30, 196, 200, 14), juce::Justification::centredLeft);
    g.drawText ("LISTEN", juce::Rectangle<int> (22, 410, 100, 12), juce::Justification::centredLeft);
}

void CaratAudioProcessorEditor::resized()
{
    // colonna sinistra
    input.setBounds (26, 66, 116, 122);
    ceiling.setBounds (152, 66, 116, 122);
    satButtons.setBounds (26, 214, 242, 26);
    satAmount.setBounds (90, 252, 116, 122);

    // centro: curva e barre di riduzione
    transfer.setBounds (290, 58, 340, 280);
    clipBar.setBounds (292, 348, 336, 24);
    tpBar.setBounds (292, 376, 336, 24);

    // destra: loudness + meter
    loudness.setBounds (642, 58, 254, 250);
    inMeter.setBounds (904, 58, 24, 250);
    outMeter.setBounds (932, 58, 24, 250);
    tpButton.setBounds (642, 318, 314, 26);
    matchButton.setBounds (642, 350, 314, 26);

    // barra inferiore
    listenButtons.setBounds (20, 424, 300, 28);
    deltaButton.setBounds (642, 424, 152, 28);
    bypassButton.setBounds (804, 424, 152, 28);
}
