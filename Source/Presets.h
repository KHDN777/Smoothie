#pragma once

#include "Parameters.h"
#include <vector>

namespace smoothie
{
struct PresetValue { const char* param; float value; }; // value in the parameter's own units / choice index

struct Preset
{
    const char* name;
    std::vector<PresetValue> values; // anything not listed is reset to its default
};

inline const std::vector<Preset>& getFactoryPresets()
{
    static const std::vector<Preset> presets
    {
        { "Init", {} },
        { "Gentle Smoothing",        { { id::depth, 30 }, { id::sharpness, 40 }, { id::selectivity, 60 }, { id::detail, 40 }, { id::release, 250 } } },
        { "Vocal - Tame Harshness",  { { id::depth, 55 }, { id::sharpness, 65 }, { id::selectivity, 45 }, { id::detail, 70 }, { id::attack, 8 }, { id::release, 120 }, { id::focusLow, 1500 }, { id::focusHigh, 9000 } } },
        { "Vocal - Sibilance Soften",{ { id::depth, 60 }, { id::mode, 1 }, { id::sharpness, 55 }, { id::selectivity, 40 }, { id::detail, 60 }, { id::attack, 2 }, { id::release, 60 }, { id::focusLow, 4500 } } },
        { "Vocal - Proximity & Mud", { { id::depth, 50 }, { id::sharpness, 35 }, { id::selectivity, 50 }, { id::detail, 55 }, { id::focusHigh, 700 }, { id::tilt, -40 }, { id::tiltFreq, 400 } } },
        { "Acoustic Guitar - De-Box",{ { id::depth, 45 }, { id::sharpness, 50 }, { id::selectivity, 55 }, { id::detail, 50 }, { id::focusLow, 150 }, { id::focusHigh, 1200 } } },
        { "Drum Overheads - Tame Ring", { { id::depth, 55 }, { id::mode, 1 }, { id::sharpness, 70 }, { id::selectivity, 50 }, { id::detail, 75 }, { id::attack, 3 }, { id::release, 90 }, { id::focusLow, 2000 } } },
        { "Bass - Clean Low End",    { { id::depth, 40 }, { id::sharpness, 30 }, { id::selectivity, 65 }, { id::detail, 35 }, { id::focusHigh, 600 }, { id::release, 300 }, { id::quality, 2 } } },
        { "Synth - Tame Resonance",  { { id::depth, 65 }, { id::sharpness, 80 }, { id::selectivity, 35 }, { id::detail, 80 }, { id::attack, 5 }, { id::release, 150 } } },
        { "Hard Dynamic Grab",       { { id::depth, 60 }, { id::mode, 1 }, { id::sharpness, 50 }, { id::selectivity, 40 }, { id::detail, 50 }, { id::attack, 1.5f }, { id::release, 80 } } },
        { "Wide Mix Bus Cleanup",    { { id::depth, 25 }, { id::sharpness, 45 }, { id::selectivity, 70 }, { id::detail, 40 }, { id::stereoMode, 1 }, { id::msProcess, 2 }, { id::link, 70 }, { id::quality, 2 } } },
        { "Tracking - Low Latency",  { { id::depth, 40 }, { id::sharpness, 50 }, { id::selectivity, 55 }, { id::detail, 40 }, { id::quality, 0 }, { id::release, 120 } } },
        { "Creative - Spectral Squash", { { id::depth, 100 }, { id::mode, 1 }, { id::sharpness, 20 }, { id::selectivity, 10 }, { id::detail, 90 }, { id::attack, 0.5f }, { id::release, 40 }, { id::mix, 70 } } },
    };
    return presets;
}
} // namespace smoothie
