#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace smoothie::id
{
    inline constexpr const char* depth       = "depth";
    inline constexpr const char* sharpness   = "sharpness";
    inline constexpr const char* selectivity = "selectivity";
    inline constexpr const char* detail      = "detail";
    inline constexpr const char* attack      = "attack";
    inline constexpr const char* release     = "release";
    inline constexpr const char* mode        = "mode";        // Soft / Hard
    inline constexpr const char* quality     = "quality";     // Low Latency / Normal / Linear Phase
    inline constexpr const char* stereoMode  = "stereoMode";  // Stereo / Mid-Side
    inline constexpr const char* msProcess   = "msProcess";   // Both / Mid / Side
    inline constexpr const char* link        = "link";
    inline constexpr const char* tilt        = "tilt";
    inline constexpr const char* tiltFreq    = "tiltFreq";
    inline constexpr const char* focusLow    = "focusLow";
    inline constexpr const char* focusHigh   = "focusHigh";
    inline constexpr const char* mix         = "mix";
    inline constexpr const char* trim        = "trim";
    inline constexpr const char* delta       = "delta";
    inline constexpr const char* sidechain   = "sidechain";
    inline constexpr const char* bypass      = "bypass";
}

namespace smoothie
{
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    using FP = AudioParameterFloat;
    using BP = AudioParameterBool;
    using CP = AudioParameterChoice;
    auto pid = [] (const char* s) { return ParameterID { s, 1 }; };

    auto pct = [] (const char* label = "%")
    {
        return AudioParameterFloatAttributes().withLabel (label)
            .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)); })
            .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue(); });
    };
    auto hz = []
    {
        return AudioParameterFloatAttributes().withLabel ("Hz")
            .withStringFromValueFunction ([] (float v, int) { if (v < 1000.0f) return String (roundToInt (v));
                auto t = String (v / 1000.0f, 2);
                while (t.endsWithChar ('0')) t = t.dropLastCharacters (1);
                if (t.endsWithChar ('.')) t = t.dropLastCharacters (1);
                return t + "k"; })
            .withValueFromStringFunction ([] (const String& s)
            {
                auto t = s.trim().toLowerCase();
                return t.endsWith ("k") ? t.getFloatValue() * 1000.0f : t.getFloatValue();
            });
    };
    auto ms = []
    {
        return AudioParameterFloatAttributes().withLabel ("ms")
            .withStringFromValueFunction ([] (float v, int) { return v < 10.0f ? String (v, 1) : String (roundToInt (v)); });
    };
    auto db = []
    {
        return AudioParameterFloatAttributes().withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return String (v, 1); });
    };

    auto freqRange = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi, 0.0f, 1.0f);
        r.setSkewForCentre (centre);
        return r;
    };

    AudioProcessorValueTreeState::ParameterLayout l;

    l.add (std::make_unique<FP> (pid (id::depth),       "Depth",       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, pct()));
    l.add (std::make_unique<FP> (pid (id::sharpness),   "Sharpness",   NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, pct()));
    l.add (std::make_unique<FP> (pid (id::selectivity), "Selectivity", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, pct()));
    l.add (std::make_unique<FP> (pid (id::detail),      "Detail",      NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, pct()));

    { auto r = NormalisableRange<float> (0.5f, 200.0f, 0.1f); r.setSkewForCentre (15.0f);
      l.add (std::make_unique<FP> (pid (id::attack),  "Attack",  r, 15.0f, ms())); }
    { auto r = NormalisableRange<float> (10.0f, 2000.0f, 1.0f); r.setSkewForCentre (200.0f);
      l.add (std::make_unique<FP> (pid (id::release), "Release", r, 150.0f, ms())); }

    l.add (std::make_unique<CP> (pid (id::mode),       "Mode",         StringArray { "Soft", "Hard" }, 0));
    l.add (std::make_unique<CP> (pid (id::quality),    "Quality",      StringArray { "Low Latency", "Normal", "Linear Phase" }, 1));
    l.add (std::make_unique<CP> (pid (id::stereoMode), "Stereo Mode",  StringArray { "Stereo", "Mid/Side" }, 0));
    l.add (std::make_unique<CP> (pid (id::msProcess),  "M/S Process",  StringArray { "Both", "Mid", "Side" }, 0));
    l.add (std::make_unique<FP> (pid (id::link),       "Stereo Link",  NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pct()));

    l.add (std::make_unique<FP> (pid (id::tilt),     "Tilt",      NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, pct()));
    l.add (std::make_unique<FP> (pid (id::tiltFreq), "Tilt Freq", freqRange (100.0f, 8000.0f, 1000.0f), 1000.0f, hz()));
    l.add (std::make_unique<FP> (pid (id::focusLow),  "Focus Low",  freqRange (20.0f, 20000.0f, 1000.0f), 20.0f, hz()));
    l.add (std::make_unique<FP> (pid (id::focusHigh), "Focus High", freqRange (20.0f, 20000.0f, 1000.0f), 20000.0f, hz()));

    l.add (std::make_unique<FP> (pid (id::mix),  "Mix",  NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pct()));
    l.add (std::make_unique<FP> (pid (id::trim), "Trim", NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, db()));

    l.add (std::make_unique<BP> (pid (id::delta),     "Delta", false));
    l.add (std::make_unique<BP> (pid (id::sidechain), "Sidechain", false));
    l.add (std::make_unique<BP> (pid (id::bypass),    "Bypass", false));

    return l;
}
} // namespace smoothie
