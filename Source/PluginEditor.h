#pragma once

#include "PluginProcessor.h"

namespace carat::ui
{
// Frontale in oro spazzolato, finestre "display" scure, scritte incise.
struct Palette
{
    static inline const juce::Colour faceTop    { 0xffe6c574 };
    static inline const juce::Colour faceMid    { 0xffd0a94f };
    static inline const juce::Colour faceBottom { 0xffb08634 };
    static inline const juce::Colour engrave    { 0xff33260e };   // scritte incise
    static inline const juce::Colour engraveDim { 0xff5f4a1f };
    static inline const juce::Colour highlight  { 0x66fff3cf };   // luce sotto l'incisione
    static inline const juce::Colour window     { 0xff15120d };   // finestre display
    static inline const juce::Colour windowHi   { 0xff2b251b };
    static inline const juce::Colour lcd        { 0xfff4d48a };   // testo/valori nel display
    static inline const juce::Colour lcdDim     { 0xff9a875f };
    static inline const juce::Colour led        { 0xffffc94a };
    static inline const juce::Colour red        { 0xffe0563f };
};

// Testo inciso sul frontale (scuro con riflesso chiaro)
void drawEngraved (juce::Graphics&, const juce::String&, juce::Rectangle<float>, juce::Justification, float size, bool bold = true);
// Finestra scura incassata
void drawWindow (juce::Graphics&, juce::Rectangle<float>, float corner = 6.0f);

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& name);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;

private:
    juce::String name;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class ChoiceButtons : public juce::Component
{
public:
    ChoiceButtons (juce::AudioProcessorValueTreeState&, const juce::String& paramId, juce::StringArray labels);
    void resized() override;
    std::function<void (int)> onChange;
    int getSelected() const { return selected; }

private:
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int selected = 0;
};

class TransferDisplay : public juce::Component
{
public:
    void setCurve (float driveLin, bool satOn, float satAmountDb, float ceilingLin);
    void setInputPeak (float db) { inputPeakDb = db; repaint(); }
    void paint (juce::Graphics&) override;

private:
    float drive = 1.0f, amount = 3.0f, ceiling = 1.0f, inputPeakDb = -100.0f;
    bool sat = false;
};

class ReductionBar : public juce::Component
{
public:
    ReductionBar (juce::String t, float r) : title (std::move (t)), range (r) {}
    void setValue (float db) { if (std::abs (db - value) > 0.02f) { value = db; repaint(); } }
    void paint (juce::Graphics&) override;

private:
    juce::String title;
    float range, value = 0.0f;
};

class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (juce::String t) : title (std::move (t)) {}
    void setValue (float db) { level = db; repaint(); }
    void paint (juce::Graphics&) override;

private:
    juce::String title;
    float level = -100.0f;
};

class LoudnessPanel : public juce::Component
{
public:
    LoudnessPanel();
    void setValues (float m, float s, float i, float tp, float gain, float ceilingDb);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::TextButton resetButton { "RESET" };

private:
    float mom = -100, st = -100, integ = -100, tpMax = -100, gainDb = 0, ceil = -1;
};
} // namespace carat::ui

//==============================================================================
class CaratAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CaratAudioProcessorEditor (CaratAudioProcessor&);
    ~CaratAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshCurve();

    CaratAudioProcessor& proc;
    carat::ui::LookAndFeel lnf;

    carat::ui::Knob input, satAmount, ceiling;
    carat::ui::ChoiceButtons satButtons, listenButtons;
    juce::TextButton tpButton { "TRUE PEAK LIMIT" }, deltaButton { "DELTA" },
                     bypassButton { "BYPASS" }, matchButton { "LEVEL MATCH" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> tpAttach, deltaAttach, bypassAttach, matchAttach;

    carat::ui::TransferDisplay transfer;
    carat::ui::ReductionBar clipBar { "CLIP / SATURATION", 12.0f }, tpBar { "TRUE PEAK LIMITER", 3.0f };
    carat::ui::LevelMeter inMeter { "IN" }, outMeter { "OUT" };
    carat::ui::LoudnessPanel loudness;

    float inS = -100.0f, outS = -100.0f, clipS = 0.0f, tpS = 0.0f, curvePeak = -100.0f;
    juce::String channelInfo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CaratAudioProcessorEditor)
};
