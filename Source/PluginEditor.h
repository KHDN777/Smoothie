#pragma once

#include "PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace smoothie::ui
{
    namespace col
    {
        const juce::Colour bg      { 0xff14101c };
        const juce::Colour panel   { 0xff1f1930 };
        const juce::Colour panelHi { 0xff2a2240 };
        const juce::Colour text    { 0xffeee8f7 };
        const juce::Colour dim     { 0xff8f86a6 };
        const juce::Colour pink    { 0xffff5fa2 };
        const juce::Colour mint    { 0xff62f0c8 };
        const juce::Colour banana  { 0xffffd166 };
    }

    inline juce::Font font (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    }

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider&) override;
        void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
        void drawComboBox (juce::Graphics&, int w, int h, bool, int, int, int, int, juce::ComboBox&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override { return font (14.0f); }
        juce::Font getLabelFont (juce::Label&) override { return font (12.0f); }
    };

    /** Knob + title; bound to a parameter. */
    class Knob : public juce::Component
    {
    public:
        Knob (juce::AudioProcessorValueTreeState&, const char* paramID, const juce::String& title,
              const juce::String& unit = {}, bool bipolar = false, juce::Colour accent = col::pink);
        void resized() override;
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    private:
        juce::Label titleLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    /** Segmented selector bound to a choice parameter. */
    class SegmentedChoice : public juce::Component
    {
    public:
        SegmentedChoice (juce::AudioProcessorValueTreeState&, const char* paramID,
                         juce::StringArray options, const juce::String& caption);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
    private:
        juce::StringArray options;
        juce::String caption;
        int selected = 0;
        std::unique_ptr<juce::ParameterAttachment> attachment;
    };

    /** Input spectrum + live reduction curve + focus range. */
    class SpectrumView : public juce::Component, private juce::Timer
    {
    public:
        SpectrumView (SmoothieProcessor&);
        ~SpectrumView() override { stopTimer(); }
        void paint (juce::Graphics&) override;
    private:
        void timerCallback() override;
        float xForFreq (float f, float w) const;

        SmoothieProcessor& proc;
        std::array<float, smoothie::Engine::numDisplayPoints> level {}, reduction {};
    };
}

class SmoothieEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SmoothieEditor (SmoothieProcessor&);
    ~SmoothieEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void stepPreset (int delta);

    SmoothieProcessor& proc;
    smoothie::ui::LookAndFeel laf;

    smoothie::ui::SpectrumView spectrum;

    smoothie::ui::Knob depth, sharpness, selectivity, detail, attack, release, mix, trim;
    smoothie::ui::Knob link, tilt, tiltFreq, focusLow, focusHigh;
    smoothie::ui::SegmentedChoice mode, quality, stereoMode, msProcess;

    juce::ToggleButton bypassBtn { "BYPASS" }, deltaBtn { "DELTA" }, scBtn { "SIDECHAIN" };
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAtt, deltaAtt, scAtt;

    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };

    juce::ComponentBoundsConstrainer constrainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmoothieEditor)
};
