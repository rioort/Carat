#include "PluginEditor.h"

using namespace carat::ui;

static juce::String fmtLufs (float v) { return v <= -69.9f ? juce::String ("--.-") : juce::String (v, 1); }

void carat::ui::drawEngraved (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r,
                              juce::Justification j, float size, bool bold)
{
    g.setFont (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    g.setColour (Palette::highlight);
    g.drawText (text, r.translated (0.0f, 1.0f), j, false);
    g.setColour (Palette::engrave);
    g.drawText (text, r, j, false);
}

void carat::ui::drawWindow (juce::Graphics& g, juce::Rectangle<float> r, float corner)
{
    // bordo chiaro in basso (luce) e scuro in alto (ombra): finestra incassata
    g.setColour (Palette::highlight);
    g.fillRoundedRectangle (r.translated (0.0f, 1.0f), corner);
    g.setColour (juce::Colour (0xff3a2c10));
    g.fillRoundedRectangle (r.translated (0.0f, -1.0f), corner);
    juce::ColourGradient grad (Palette::window, r.getX(), r.getY(), Palette::window.brighter (0.08f), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, corner);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
}

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Palette::engrave);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, Palette::engrave);
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

    // scala incisa
    for (int i = 0; i <= 10; ++i)
    {
        const float a = start + (end - start) * (float) i / 10.0f;
        const float r1 = radius - 1.0f, r2 = radius - (i % 5 == 0 ? 7.0f : 4.5f);
        juce::Line<float> tick (c.x + r1 * std::sin (a), c.y - r1 * std::cos (a), c.x + r2 * std::sin (a), c.y - r2 * std::cos (a));
        g.setColour (Palette::engrave.withAlpha (0.75f * alpha));
        g.drawLine (tick, i % 5 == 0 ? 1.6f : 1.0f);
    }
    // arco del valore
    juce::Path value;
    value.addCentredArc (c.x, c.y, radius - 10.0f, radius - 10.0f, 0.0f, start, angle, true);
    g.setColour (Palette::engrave.withAlpha (0.85f * alpha));
    g.strokePath (value, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // manopola nera anodizzata con anello metallico
    const float body = radius - 14.0f;
    g.setColour (juce::Colours::black.withAlpha (0.35f * alpha));
    g.fillEllipse (c.x - body, c.y - body + 3.0f, body * 2.0f, body * 2.0f);   // ombra
    juce::ColourGradient ring (juce::Colour (0xfff7e3a6).withMultipliedAlpha (alpha), c.x, c.y - body,
                               juce::Colour (0xff7a5a1c).withMultipliedAlpha (alpha), c.x, c.y + body, false);
    g.setGradientFill (ring);
    g.fillEllipse (c.x - body, c.y - body, body * 2.0f, body * 2.0f);
    const float cap = body - 3.0f;
    juce::ColourGradient grad (juce::Colour (0xff3a3833).withMultipliedAlpha (alpha), c.x, c.y - cap,
                               juce::Colour (0xff0d0c0a).withMultipliedAlpha (alpha), c.x, c.y + cap, false);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - cap, c.y - cap, cap * 2.0f, cap * 2.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.6f, -cap + 3.0f, 3.2f, cap * 0.5f, 1.6f);
    g.setColour (Palette::lcd.withMultipliedAlpha (alpha));
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    drawWindow (g, r, 4.0f);
    if (b.getToggleState())
    {
        auto lit = r.reduced (2.0f);
        g.setColour (Palette::led.withAlpha (0.25f));
        g.fillRoundedRectangle (lit.expanded (1.5f), 4.0f);
        juce::ColourGradient grad (Palette::led.brighter (0.25f), lit.getX(), lit.getY(), Palette::led.darker (0.25f), lit.getX(), lit.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (lit, 3.0f);
    }
    else if (highlighted)
    {
        g.setColour (Palette::windowHi);
        g.fillRoundedRectangle (r.reduced (2.0f), 3.0f);
    }
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? Palette::window : Palette::lcdDim);
    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
}

juce::Label* LookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (juce::FontOptions (13.0f, juce::Font::bold));
    l->setColour (juce::Label::textColourId, Palette::engrave);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::textWhenEditingColourId, Palette::engrave);
    l->setColour (juce::TextEditor::backgroundColourId, Palette::faceTop);
    return l;
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& n)
    : name (n.toUpperCase())
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (260);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, Palette::engrave);
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
}

void Knob::paint (juce::Graphics& g)
{
    if (! isEnabled()) g.beginTransparencyLayer (0.4f);
    drawEngraved (g, name, getLocalBounds().removeFromTop (16).toFloat(), juce::Justification::centred, 11.5f);
    if (! isEnabled()) g.endTransparencyLayer();
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (16);
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
    drawWindow (g, r, 6.0f);

    auto plot = r.reduced (34.0f, 16.0f).withTrimmedBottom (14.0f);
    const float xMin = -30.0f, xMax = 6.0f, yMin = -30.0f, yMax = 3.0f;
    auto px = [&] (float db) { return plot.getX() + (db - xMin) / (xMax - xMin) * plot.getWidth(); };
    auto py = [&] (float db) { return plot.getBottom() - (db - yMin) / (yMax - yMin) * plot.getHeight(); };

    g.setFont (juce::FontOptions (9.5f));
    for (float d = -30.0f; d <= 6.0f; d += 6.0f)
    {
        g.setColour (Palette::windowHi);
        g.drawVerticalLine ((int) px (d), plot.getY(), plot.getBottom());
        g.setColour (Palette::lcdDim);
        g.drawText (juce::String ((int) d), juce::Rectangle<float> (px (d) - 14.0f, plot.getBottom() + 2.0f, 28.0f, 12.0f), juce::Justification::centred);
    }
    for (float d = -30.0f; d <= 0.0f; d += 6.0f)
    {
        g.setColour (Palette::windowHi);
        g.drawHorizontalLine ((int) py (d), plot.getX(), plot.getRight());
        g.setColour (Palette::lcdDim);
        g.drawText (juce::String ((int) d), juce::Rectangle<float> (plot.getX() - 30.0f, py (d) - 6.0f, 26.0f, 12.0f), juce::Justification::centredRight);
    }
    g.setColour (Palette::lcdDim.withAlpha (0.6f));
    g.drawVerticalLine ((int) px (0.0f), plot.getY(), plot.getBottom());
    const float ceilDb = carat::linToDb (ceiling);
    g.setColour (Palette::red.withAlpha (0.6f));
    g.drawHorizontalLine ((int) py (ceilDb), plot.getX(), plot.getRight());

    {
        juce::Path unity;
        unity.startNewSubPath (px (xMin), py (xMin));
        unity.lineTo (px (yMax), py (yMax));
        const float dash[] = { 3.0f, 4.0f };
        juce::Path dashed;
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, unity, dash, 2);
        g.setColour (Palette::lcdDim.withAlpha (0.45f));
        g.fillPath (dashed);
    }

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
    g.setColour (Palette::led.withAlpha (0.18f));
    g.strokePath (curve, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (Palette::lcd);
    g.strokePath (curve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (inputPeakDb > xMin)
    {
        const float xDb = juce::jmin (xMax, inputPeakDb);
        const float y = carat::ConverterStage::staticTransfer (carat::dbToLin (xDb), drive, amount, sat) * ceiling;
        const float X = px (xDb), Y = py (carat::linToDb (y));
        g.setColour (Palette::led.withAlpha (0.3f));
        g.fillEllipse (X - 9.0f, Y - 9.0f, 18.0f, 18.0f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (X - 4.0f, Y - 4.0f, 8.0f, 8.0f);
    }
    g.restoreState();

    g.setColour (Palette::lcdDim);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("TRANSFER  (IN dBFS -> OUT dBFS)", r.reduced (10.0f, 4.0f).removeFromTop (14.0f), juce::Justification::centredRight);
}

//==============================================================================
void ReductionBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto top = r.removeFromTop (15.0f);
    drawEngraved (g, title, top, juce::Justification::centredLeft, 10.5f);
    drawEngraved (g, "-" + juce::String (value, 1) + " dB", top, juce::Justification::centredRight, 11.0f);

    auto bar = r.removeFromTop (11.0f);
    drawWindow (g, bar, 3.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, value / range);
    if (frac > 0.0f)
    {
        g.setColour (Palette::led);
        g.fillRoundedRectangle (bar.reduced (2.0f).withWidth ((bar.getWidth() - 4.0f) * frac), 2.0f);
    }
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto lbl = r.removeFromBottom (16.0f);
    drawEngraved (g, title, lbl, juce::Justification::centred, 10.0f);

    drawWindow (g, r, 3.0f);
    auto inner = r.reduced (3.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, (level + 48.0f) / 51.0f); // -48..+3 dBFS
    g.setColour (level > -0.05f ? Palette::red : Palette::led);
    g.fillRoundedRectangle (inner.withTop (inner.getBottom() - inner.getHeight() * frac), 2.0f);
    const float y0 = inner.getBottom() - inner.getHeight() * (48.0f / 51.0f);
    g.setColour (Palette::lcd.withAlpha (0.5f));
    g.drawHorizontalLine ((int) y0, inner.getX(), inner.getRight());
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
    resetButton.setBounds (getWidth() - 72, 10, 60, 22);
}

void LoudnessPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawWindow (g, r, 6.0f);

    auto area = r.reduced (14.0f, 10.0f);
    g.setColour (Palette::lcdDim);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("LOUDNESS  (OUT)", area.removeFromTop (22.0f), juce::Justification::centredLeft);

    auto row = [&] (const juce::String& name, const juce::String& value, const juce::String& unit, bool big, juce::Colour col)
    {
        auto line = area.removeFromTop (big ? 42.0f : 25.0f);
        g.setColour (Palette::lcdDim);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (name, line.removeFromLeft (100.0f), juce::Justification::centredLeft);
        g.drawText (unit, line.removeFromRight (40.0f), juce::Justification::centredLeft);
        g.setColour (col);
        g.setFont (juce::FontOptions (big ? 30.0f : 17.0f, juce::Font::bold));
        g.drawText (value, line.withTrimmedRight (6.0f), juce::Justification::centredRight);
    };

    row ("INTEGRATED", fmtLufs (integ), "LUFS", true, Palette::lcd);
    row ("SHORT-TERM", fmtLufs (st), "LUFS", false, Palette::lcd);
    row ("MOMENTARY", fmtLufs (mom), "LUFS", false, Palette::lcd);
    row ("TRUE PEAK MAX", tpMax <= -99.0f ? juce::String ("--.-") : juce::String (tpMax, 1), "dBTP", false,
         tpMax > ceil + 0.05f ? Palette::red : Palette::lcd);
    row ("LOUDER BY", (gainDb >= 0.0f ? "+" : "") + juce::String (gainDb, 1), "LU", false, Palette::led);
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

    satButtons.onChange = [this] (int s) { satAmount.setEnabled (s == 3); satAmount.repaint(); refreshCurve(); };
    satAmount.setEnabled (satButtons.getSelected() == 3);
    loudness.resetButton.onClick = [this] { proc.resetMeters.store (true); };

    setSize (976, 480);
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

    // diagnostica canali (visibile in alto a destra)
    const int bi = proc.hostInChannels.load(), bo = proc.hostOutChannels.load(), bb = proc.hostBufferChannels.load();
    const juce::String info = bb == 0 ? juce::String ("8x OVERSAMPLING")
                                      : "8x OVERSAMPLING   |   " + juce::String (bi) + " IN / " + juce::String (bo) + " OUT"
                                        + (bb != 2 ? "  (BUFFER " + juce::String (bb) + ")" : juce::String());
    if (info != channelInfo) { channelInfo = info; repaint (getLocalBounds().removeFromTop (60)); }
}

void CaratAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    // frontale in oro spazzolato
    juce::ColourGradient face (Palette::faceTop, 0.0f, 0.0f, Palette::faceBottom, 0.0f, b.getBottom(), false);
    face.addColour (0.45, Palette::faceMid.brighter (0.08f));
    g.setGradientFill (face);
    g.fillAll();
    juce::Random rng (1234);
    for (int y = 0; y < getHeight(); ++y)
    {
        const float a = 0.025f + 0.05f * rng.nextFloat();
        g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colour (0xff5a3d0c)).withAlpha (a));
        g.drawHorizontalLine (y, 0.0f, b.getRight());
    }
    // riflesso diagonale morbido
    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.0f), 0.0f, 0.0f, juce::Colours::white.withAlpha (0.0f), b.getRight(), b.getBottom(), false);
    sheen.addColour (0.35, juce::Colours::white.withAlpha (0.12f));
    sheen.addColour (0.5, juce::Colours::white.withAlpha (0.0f));
    g.setGradientFill (sheen);
    g.fillAll();
    // bordo
    g.setColour (juce::Colour (0xff6b4e17));
    g.drawRect (b, 2.0f);

    // viti agli angoli
    for (auto pt : { juce::Point<float> (14, 14), juce::Point<float> (b.getRight() - 14, 14),
                     juce::Point<float> (14, b.getBottom() - 14), juce::Point<float> (b.getRight() - 14, b.getBottom() - 14) })
    {
        juce::ColourGradient sg (juce::Colour (0xfff4e2b0), pt.x - 5, pt.y - 5, juce::Colour (0xff7d5c1f), pt.x + 5, pt.y + 5, false);
        g.setGradientFill (sg);
        g.fillEllipse (pt.x - 5.5f, pt.y - 5.5f, 11.0f, 11.0f);
        g.setColour (Palette::engrave.withAlpha (0.8f));
        g.drawEllipse (pt.x - 5.5f, pt.y - 5.5f, 11.0f, 11.0f, 0.8f);
        g.drawLine (pt.x - 3.5f, pt.y + 1.0f, pt.x + 3.5f, pt.y - 1.0f, 1.4f);
    }

    // intestazione incisa
    auto header = b.withHeight (58.0f).reduced (30.0f, 0.0f);
    drawEngraved (g, "CARAT", header, juce::Justification::centredLeft, 30.0f);
    drawEngraved (g, "MASTERING CONVERTER CLIPPER", header.withTrimmedLeft (122.0f), juce::Justification::centredLeft, 12.5f);
    drawEngraved (g, channelInfo.isEmpty() ? juce::String ("8x OVERSAMPLING") : channelInfo, header, juce::Justification::centredRight, 10.5f);
    g.setColour (Palette::engrave.withAlpha (0.5f));
    g.drawHorizontalLine (58, 24.0f, b.getRight() - 24.0f);
    g.setColour (Palette::highlight);
    g.drawHorizontalLine (59, 24.0f, b.getRight() - 24.0f);

    // cornice incisa della sezione controlli
    auto groove = juce::Rectangle<float> (20.0f, 70.0f, 262.0f, 338.0f);
    g.setColour (Palette::highlight);
    g.drawRoundedRectangle (groove.translated (0.0f, 1.0f), 8.0f, 1.0f);
    g.setColour (Palette::engrave.withAlpha (0.55f));
    g.drawRoundedRectangle (groove, 8.0f, 1.0f);

    drawEngraved (g, "SOFT SATURATION", juce::Rectangle<float> (34.0f, 206.0f, 220.0f, 14.0f), juce::Justification::centredLeft, 11.0f);
    drawEngraved (g, "LISTEN", juce::Rectangle<float> (24.0f, 418.0f, 120.0f, 14.0f), juce::Justification::centredLeft, 10.5f);
}

void CaratAudioProcessorEditor::resized()
{
    // colonna sinistra
    input.setBounds (30, 78, 116, 124);
    ceiling.setBounds (156, 78, 116, 124);
    satButtons.setBounds (30, 224, 242, 28);
    satAmount.setBounds (93, 262, 116, 128);

    // centro: curva e barre di riduzione
    transfer.setBounds (296, 70, 336, 270);
    clipBar.setBounds (298, 350, 332, 26);
    tpBar.setBounds (298, 380, 332, 26);

    // destra: loudness + meter
    loudness.setBounds (646, 70, 248, 258);
    inMeter.setBounds (902, 70, 24, 258);
    outMeter.setBounds (932, 70, 24, 258);
    tpButton.setBounds (646, 340, 310, 28);
    matchButton.setBounds (646, 374, 310, 28);

    // barra inferiore
    listenButtons.setBounds (22, 434, 300, 30);
    deltaButton.setBounds (646, 434, 150, 30);
    bypassButton.setBounds (806, 434, 150, 30);
}
