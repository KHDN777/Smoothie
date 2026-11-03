#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace smoothie
{

enum class Quality { lowLatency = 0, normal = 1, linearPhase = 2 };

struct EngineSettings
{
    float depth       = 0.5f;   // 0..1   how much of each detected resonance is removed
    float sharpness   = 0.5f;   // 0..1   narrowness of the applied cuts
    float selectivity = 0.5f;   // 0..1   how prominent a peak must be to be touched
    float detail      = 0.5f;   // 0..1   how local the "what is a resonance" reference is
    float attackMs    = 15.0f;
    float releaseMs   = 150.0f;
    bool  hard        = false;  // soft = static resonances, hard = also reacts to dynamics
    float tilt        = 0.0f;   // -1..1  shifts processing toward lows (-) or highs (+)
    float tiltFreq    = 1000.0f;
    float focusLow    = 20.0f;  // processing range
    float focusHigh   = 20000.0f;
    float stereoLink  = 1.0f;   // 0..1   0 = independent channels, 1 = identical reduction

    bool operator== (const EngineSettings& o) const noexcept
    {
        using juce::exactlyEqual;
        return exactlyEqual (depth, o.depth) && exactlyEqual (sharpness, o.sharpness)
            && exactlyEqual (selectivity, o.selectivity) && exactlyEqual (detail, o.detail)
            && exactlyEqual (attackMs, o.attackMs) && exactlyEqual (releaseMs, o.releaseMs)
            && hard == o.hard && exactlyEqual (tilt, o.tilt) && exactlyEqual (tiltFreq, o.tiltFreq)
            && exactlyEqual (focusLow, o.focusLow) && exactlyEqual (focusHigh, o.focusHigh)
            && exactlyEqual (stereoLink, o.stereoLink);
    }
};

/**
    Spectral resonance suppressor.

    A 75%-overlap STFT (sqrt-Hann analysis/synthesis) with real, zero-phase per-bin gains.
    Every frame, for every bin, the engine compares the bin level with a local spectral
    baseline, treats the excess as a resonance, and turns it into a smoothed (in time and
    frequency) gain reduction.
*/
class Engine
{
public:
    static constexpr int maxChannels      = 16;     // enough for 9.1.6
    static constexpr int maxFFT           = 4096;
    static constexpr int maxBins          = maxFFT / 2 + 1;
    static constexpr int numDisplayPoints = 128;

    Engine();

    void prepare (double sampleRate, Quality q);
    void setQuality (Quality q);
    Quality getQuality() const noexcept   { return quality; }
    int getLatencySamples() const noexcept { return fftSize; }

    void reset();
    void setSettings (const EngineSettings& s);

    /** Processes in place. `active[c] == false` leaves that channel spectrally untouched
        (but still delayed by the latency). `sidechain` (optional) drives detection. */
    void process (float* const* channels, int numChannels, const float* sidechain,
                  int numSamples, const bool* active);

    // GUI read-outs (thread-safe, written by the audio thread)
    float getDisplayLevelDb (int i) const noexcept     { return dispLevel[(size_t) i].load (std::memory_order_relaxed); }
    float getDisplayReductionDb (int i) const noexcept { return dispReduction[(size_t) i].load (std::memory_order_relaxed); }
    static float getDisplayFrequency (int i) noexcept;

private:
    struct ChannelState
    {
        std::vector<float> in, out;       // circular buffers (maxFFT)
        std::vector<float> spec;          // 2*maxFFT interleaved complex
        std::vector<float> magSm, slow;   // smoothed power (fast / slow), per bin
        std::vector<float> raw, gain;     // raw instantaneous gain dB / time-smoothed gain dB
    };

    void processFrame (int numChannels, bool hasSidechain, const bool* active);
    void computeRawGain (const float* spec, ChannelState& cs, float* outGainDb, bool updateDisplay);
    void updateDerived();
    void buildDisplayTables();

    std::array<std::unique_ptr<juce::dsp::FFT>, 5> ffts; // orders 8..12
    std::vector<ChannelState> channels;
    std::vector<float> scIn, scSpec, scRaw, window;
    ChannelState scState;

    // scratch
    std::vector<float> dbUse, dbRaw, dbSlowV, env, clipDb, tmpG, finalG, dbShow, meanG;
    std::vector<double> prefix;
    std::vector<float> sparse;   // range-minimum sparse table, 12 levels x maxBins

    // derived per-bin data
    std::vector<int> envHw, gainHw;
    std::vector<float> weight;

    EngineSettings settings;
    bool derivedDirty = true;

    double sampleRate = 44100.0;
    Quality quality = Quality::normal;
    int order = 10, fftSize = 1024, hop = 256, numBins = 513, mask = 1023;
    int pos = 0, hopCount = 0;
    float powerNorm = 1.0f;
    float magCoef = 0.0f, slowCoef = 0.0f, attackCoef = 0.0f, releaseCoef = 0.0f;
    float threshold = 4.0f, strength = 0.6f;

    std::array<std::atomic<float>, numDisplayPoints> dispLevel, dispReduction;
    std::array<int, numDisplayPoints> dispLo {}, dispHi {};
};

} // namespace smoothie
