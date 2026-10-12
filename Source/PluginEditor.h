#pragma once

#include "PluginProcessor.h"

namespace carat::ui
{
struct Palette
{
    static inline const juce::Colour bg      { 0xff131315 };
    static inline const juce::Colour panel   { 0xff1c1c1f };
    static inline const juce::Colour panelHi { 0xff29292d };
    static inline const juce::Colour text    { 0xffede7db };
    static inline const juce::Colour textDim { 0xff8d877c };
    static inline const juce::Colour gold    { 0xffd8ac52 };
    static inline const juce::Colour goldDim { 0xff5e4a24 };
    static inline const juce::Colour red     { 0xffe0563f };
};

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
    juce::Slider slider;

private:
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Fila di pulsanti per un parametro a scelta (uno acceso alla volta)
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

// Curva di trasferimento statica (ingresso dBFS -> uscita dBFS) + punto live
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

// Barra orizzontale di riduzione (0..range dB)
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CaratAudioProcessorEditor)
};
