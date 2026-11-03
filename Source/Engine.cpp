#include "Engine.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace smoothie
{
namespace
{
    constexpr float kMaxReductionDb = 36.0f;
    constexpr float kLnToDb20       = 0.11512925465f; // ln(10)/20
    constexpr float kOlaScale       = 0.5f;           // sum of sqrt-Hann^2 at 75% overlap = 2

    inline float smoothstep01 (float x) noexcept
    {
        x = juce::jlimit (0.0f, 1.0f, x);
        return x * x * (3.0f - 2.0f * x);
    }

    // soft-knee "amount above threshold"
    inline float kneeOver (float x, float knee) noexcept
    {
        const float h = 0.5f * knee;
        if (x <= -h) return 0.0f;
        if (x >= h)  return x;
        const float t = x + h;
        return t * t / (2.0f * knee);
    }
}

Engine::Engine()
{
    for (int o = 8; o <= 12; ++o)
        ffts[(size_t) (o - 8)] = std::make_unique<juce::dsp::FFT> (o);

    auto initChannel = [] (ChannelState& c)
    {
        c.in.assign ((size_t) maxFFT, 0.0f);
        c.out.assign ((size_t) maxFFT, 0.0f);
        c.spec.assign ((size_t) maxFFT * 2, 0.0f);
        c.magSm.assign ((size_t) maxBins, 0.0f);
        c.slow.assign ((size_t) maxBins, 0.0f);
        c.raw.assign ((size_t) maxBins, 0.0f);
        c.gain.assign ((size_t) maxBins, 0.0f);
    };

    channels.resize ((size_t) maxChannels);
    for (auto& c : channels) initChannel (c);
    initChannel (scState);

    scIn.assign ((size_t) maxFFT, 0.0f);
    scSpec.assign ((size_t) maxFFT * 2, 0.0f);
    scRaw.assign ((size_t) maxBins, 0.0f);
    window.assign ((size_t) maxFFT, 0.0f);

    for (auto* v : { &dbUse, &dbRaw, &dbSlowV, &env, &clipDb, &tmpG, &finalG, &dbShow, &meanG })
        v->assign ((size_t) maxBins, 0.0f);
    prefix.assign ((size_t) maxBins + 1, 0.0);
    sparse.assign ((size_t) maxBins * 12, 0.0f);

    envHw.assign ((size_t) maxBins, 3);
    gainHw.assign ((size_t) maxBins, 1);
    weight.assign ((size_t) maxBins, 1.0f);

    for (auto& a : dispLevel)     a.store (-120.0f);
    for (auto& a : dispReduction) a.store (0.0f);
}

float Engine::getDisplayFrequency (int i) noexcept
{
    return 20.0f * std::pow (1000.0f, (float) i / (float) (numDisplayPoints - 1));
}

void Engine::prepare (double sr, Quality q)
{
    sampleRate = sr > 0 ? sr : 44100.0;
    setQuality (q);
}

void Engine::setQuality (Quality q)
{
    quality = q;
    order   = q == Quality::lowLatency ? 9 : (q == Quality::normal ? 10 : 12);
    fftSize = 1 << order;
    hop     = fftSize / 4;
    numBins = fftSize / 2 + 1;
    mask    = fftSize - 1;

    // periodic sqrt-Hann
    for (int j = 0; j < fftSize; ++j)
        window[(size_t) j] = std::sqrt (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) j / (float) fftSize));

    const float norm = 0.3183f * (float) fftSize; // full-scale sine peak magnitude => 0 dB
    powerNorm = 1.0f / (norm * norm);

    reset();
    derivedDirty = true;
    buildDisplayTables();
}

void Engine::reset()
{
    auto clear = [] (ChannelState& c)
    {
        std::fill (c.in.begin(), c.in.end(), 0.0f);
        std::fill (c.out.begin(), c.out.end(), 0.0f);
        std::fill (c.magSm.begin(), c.magSm.end(), 0.0f);
        std::fill (c.slow.begin(), c.slow.end(), 0.0f);
        std::fill (c.raw.begin(), c.raw.end(), 0.0f);
        std::fill (c.gain.begin(), c.gain.end(), 0.0f);
    };
    for (auto& c : channels) clear (c);
    clear (scState);
    std::fill (scIn.begin(), scIn.end(), 0.0f);
    pos = 0;
    hopCount = 0;
    for (auto& a : dispLevel)     a.store (-120.0f);
    for (auto& a : dispReduction) a.store (0.0f);
}

void Engine::setSettings (const EngineSettings& s)
{
    if (! (s == settings))
    {
        settings = s;
        derivedDirty = true;
    }
}

void Engine::buildDisplayTables()
{
    const float binHz = (float) sampleRate / (float) fftSize;
    for (int p = 0; p < numDisplayPoints; ++p)
    {
        const float fc = getDisplayFrequency (p);
        const float fLo = p == 0 ? fc : std::sqrt (fc * getDisplayFrequency (p - 1));
        const float fHi = p == numDisplayPoints - 1 ? fc : std::sqrt (fc * getDisplayFrequency (p + 1));
        int lo = (int) std::floor (fLo / binHz);
        int hi = (int) std::ceil  (fHi / binHz);
        lo = juce::jlimit (0, numBins - 1, lo);
        hi = juce::jlimit (lo, numBins - 1, hi);
        dispLo[(size_t) p] = lo;
        dispHi[(size_t) p] = hi;
    }
}

void Engine::updateDerived()
{
    derivedDirty = false;
    const float hopSec = (float) hop / (float) sampleRate;

    // detection averaging: at least ~3 frames, so random noise peaks aren't mistaken for resonances
    magCoef     = std::exp (-hopSec / std::max (0.015f, 3.0f * hopSec));
    slowCoef    = std::exp (-hopSec / 0.300f);
    attackCoef  = std::exp (-hopSec / (std::max (0.1f, settings.attackMs)  * 0.001f));
    releaseCoef = std::exp (-hopSec / (std::max (1.0f, settings.releaseMs) * 0.001f));

    threshold = (settings.hard ? 1.0f : 2.0f) + settings.selectivity * 8.0f;
    strength  = settings.depth * (settings.hard ? 1.8f : 1.2f);

    // detail: smaller baseline window => more local/surgical detection
    const float envFrac = juce::jmap (settings.detail, 0.35f, 0.08f);
    const int   envMin  = std::max (2, fftSize / 512);
    // sharpness: narrower smoothing of the applied cuts
    const float gFrac   = juce::jmap (settings.sharpness, 0.20f, 0.0f);
    // a cut must always span the analysis window's main lobe (~±2 bins at 1024) to be effective
    const int   gainMin = std::max (2, fftSize / 1024 + 1);

    const float binHz = (float) sampleRate / (float) fftSize;
    const bool lowFocusOn  = settings.focusLow  > 21.0f;
    const bool highFocusOn = settings.focusHigh < 19500.0f;

    for (int k = 0; k < numBins; ++k)
    {
        envHw[(size_t) k]  = juce::jlimit (envMin, numBins, (int) ((float) k * envFrac));
        gainHw[(size_t) k] = std::max (gainMin, (int) ((float) k * gFrac));

        const float f = std::max (0.5f, (float) k) * binHz;
        float w = 1.0f;

        // tilt: +1 doubles processing 3 octaves above pivot and halves it 3 octaves below
        const float oct = std::log2 (f / settings.tiltFreq);
        w *= juce::jlimit (0.1f, 3.0f, std::exp2 (settings.tilt * oct / 3.0f));

        // focus range with ~1 octave skirts
        if (lowFocusOn)  w *= smoothstep01 (std::log2 (f / settings.focusLow)  + 0.5f);
        if (highFocusOn) w *= smoothstep01 (std::log2 (settings.focusHigh / f) + 0.5f);

        weight[(size_t) k] = w;
    }
}

void Engine::process (float* const* ch, int numCh, const float* sc, int numSamples, const bool* active)
{
    numCh = std::min (numCh, maxChannels);
    if (derivedDirty) updateDerived();

    for (int i = 0; i < numSamples; ++i)
    {
        for (int c = 0; c < numCh; ++c)
        {
            auto& cs = channels[(size_t) c];
            cs.in[(size_t) pos] = ch[c][i];
            ch[c][i] = cs.out[(size_t) pos];
            cs.out[(size_t) pos] = 0.0f;
        }
        scIn[(size_t) pos] = sc != nullptr ? sc[i] : 0.0f;

        pos = (pos + 1) & mask;

        if (++hopCount >= hop)
        {
            hopCount = 0;
            processFrame (numCh, sc != nullptr, active);
        }
    }
}

void Engine::computeRawGain (const float* spec, ChannelState& cs, float* outG, bool updateDisplay)
{
    const int nb = numBins;
    const bool hard = settings.hard;

    for (int k = 0; k < nb; ++k)
    {
        const float re = spec[2 * k], im = spec[2 * k + 1];
        const float p = (re * re + im * im) * powerNorm;

        float& sm = cs.magSm[(size_t) k];
        sm = magCoef * sm + (1.0f - magCoef) * p;
        float& sl = cs.slow[(size_t) k];
        sl = slowCoef * sl + (1.0f - slowCoef) * p;

        const float dRaw = 10.0f * std::log10 (p + 1.0e-12f);
        dbRaw[(size_t) k] = dRaw;
        dbUse[(size_t) k] = hard ? dRaw : 10.0f * std::log10 (sm + 1.0e-12f);
        dbSlowV[(size_t) k] = 10.0f * std::log10 (sl + 1.0e-12f);
    }

    // pass 1: local baseline (moving average in dB, window grows with frequency)
    auto movingAverage = [&] (const float* src, float* dst, const std::vector<int>& hw)
    {
        prefix[0] = 0.0;
        for (int k = 0; k < nb; ++k) prefix[(size_t) k + 1] = prefix[(size_t) k] + (double) src[k];
        for (int k = 0; k < nb; ++k)
        {
            const int h  = hw[(size_t) k];
            const int lo = std::max (0, k - h), hi = std::min (nb - 1, k + h);
            dst[k] = (float) ((prefix[(size_t) hi + 1] - prefix[(size_t) lo]) / (double) (hi - lo + 1));
        }
    };

    movingAverage (dbUse.data(), env.data(), envHw);

    // pass 2: clip peaks to baseline+3 dB, then re-average => baseline ignores the resonances
    for (int k = 0; k < nb; ++k)
        clipDb[(size_t) k] = std::min (dbUse[(size_t) k], env[(size_t) k] + 3.0f);
    movingAverage (clipDb.data(), env.data(), envHw);

    const float knee = 6.0f;
    for (int k = 0; k < nb; ++k)
    {
        float excess = dbUse[(size_t) k] - env[(size_t) k];
        if (hard) excess += 0.7f * std::max (0.0f, dbRaw[(size_t) k] - dbSlowV[(size_t) k]);

        const float over = excess - threshold;
        float red = kneeOver (over, knee) * strength * weight[(size_t) k];

        // do not "clean" silence / noise floor
        red *= smoothstep01 ((dbUse[(size_t) k] + 95.0f) / 15.0f);

        outG[k] = -std::min (kMaxReductionDb, red);
    }

    if (updateDisplay)
        std::memcpy (dbShow.data(), dbUse.data(), sizeof (float) * (size_t) nb);
}

void Engine::processFrame (int numCh, bool hasSc, const bool* active)
{
    const int N = fftSize, nb = numBins;
    auto& fft = *ffts[(size_t) (order - 8)];

    // ---- analysis ----
    for (int c = 0; c < numCh; ++c)
    {
        auto& cs = channels[(size_t) c];
        float* s = cs.spec.data();
        for (int j = 0; j < N; ++j)
            s[j] = cs.in[(size_t) ((pos + j) & mask)] * window[(size_t) j];
        std::fill (s + N, s + 2 * N, 0.0f);
        fft.performRealOnlyForwardTransform (s, true);
    }

    if (hasSc)
    {
        float* s = scSpec.data();
        for (int j = 0; j < N; ++j)
            s[j] = scIn[(size_t) ((pos + j) & mask)] * window[(size_t) j];
        std::fill (s + N, s + 2 * N, 0.0f);
        fft.performRealOnlyForwardTransform (s, true);
    }

    // ---- detection ----
    int firstActive = -1, numActive = 0;
    for (int c = 0; c < numCh; ++c)
        if (active == nullptr || active[c]) { if (firstActive < 0) firstActive = c; ++numActive; }

    if (hasSc)
    {
        computeRawGain (scSpec.data(), scState, scRaw.data(), true);
        for (int c = 0; c < numCh; ++c)
            if (active == nullptr || active[c])
                std::memcpy (channels[(size_t) c].raw.data(), scRaw.data(), sizeof (float) * (size_t) nb);
    }
    else
    {
        for (int c = 0; c < numCh; ++c)
        {
            auto& cs = channels[(size_t) c];
            if (active == nullptr || active[c])
                computeRawGain (cs.spec.data(), cs, cs.raw.data(), c == firstActive);
        }
    }

    // ---- stereo / multichannel link ----
    if (! hasSc && numActive > 1 && settings.stereoLink > 0.0f)
    {
        std::fill (meanG.begin(), meanG.begin() + nb, 0.0f);
        for (int c = 0; c < numCh; ++c)
            if (active == nullptr || active[c])
                for (int k = 0; k < nb; ++k) meanG[(size_t) k] += channels[(size_t) c].raw[(size_t) k];

        const float inv = 1.0f / (float) numActive, L = settings.stereoLink;
        for (int c = 0; c < numCh; ++c)
            if (active == nullptr || active[c])
            {
                auto& r = channels[(size_t) c].raw;
                for (int k = 0; k < nb; ++k)
                    r[(size_t) k] += L * (meanG[(size_t) k] * inv - r[(size_t) k]);
            }
    }

    // ---- time smoothing, frequency smoothing, apply ----
    for (int c = 0; c < numCh; ++c)
    {
        auto& cs = channels[(size_t) c];
        float* s = cs.spec.data();
        const bool on = active == nullptr || active[c];

        if (on)
        {
            for (int k = 0; k < nb; ++k)
            {
                const float g = cs.raw[(size_t) k];
                float& st = cs.gain[(size_t) k];
                const float a = g < st ? attackCoef : releaseCoef;
                st = a * st + (1.0f - a) * g;
            }

            // widen each cut (running minimum over +-h bins, via a sparse table), then
            // smooth it (box blur over +-h/2) so the cut keeps its full depth at the peak
            std::memcpy (sparse.data(), cs.gain.data(), sizeof (float) * (size_t) nb);
            int levels = 1;
            for (int l = 1; (1 << l) <= nb; ++l, ++levels)
            {
                const float* prev = sparse.data() + (size_t) (l - 1) * maxBins;
                float* cur = sparse.data() + (size_t) l * maxBins;
                const int half = 1 << (l - 1);
                for (int k = 0; k + (1 << l) <= nb; ++k)
                    cur[k] = std::min (prev[k], prev[k + half]);
            }

            for (int k = 0; k < nb; ++k)
            {
                const int h  = gainHw[(size_t) k];
                const int lo = std::max (0, k - h), hi = std::min (nb - 1, k + h);
                const int len = hi - lo + 1;
                int l = 0; while ((2 << l) <= len) ++l;
                const float* t = sparse.data() + (size_t) l * maxBins;
                tmpG[(size_t) k] = std::min (t[lo], t[hi - (1 << l) + 1]);
            }

            prefix[0] = 0.0;
            for (int k = 0; k < nb; ++k) prefix[(size_t) k + 1] = prefix[(size_t) k] + (double) tmpG[(size_t) k];

            for (int k = 0; k < nb; ++k)
            {
                const int h = std::max (1, gainHw[(size_t) k] / 2);
                const int lo = std::max (0, k - h), hi = std::min (nb - 1, k + h);
                const float g = (float) ((prefix[(size_t) hi + 1] - prefix[(size_t) lo]) / (double) (hi - lo + 1));
                finalG[(size_t) k] = g;
                const float lin = std::exp (g * kLnToDb20);
                s[2 * k]     *= lin;
                s[2 * k + 1] *= lin;
            }

            if (c == firstActive)
            {
                for (int p = 0; p < numDisplayPoints; ++p)
                {
                    float lvl = -120.0f, red = 0.0f;
                    for (int k = dispLo[(size_t) p]; k <= dispHi[(size_t) p]; ++k)
                    {
                        lvl = std::max (lvl, dbShow[(size_t) k]);
                        red = std::min (red, finalG[(size_t) k]);
                    }
                    dispLevel[(size_t) p].store (lvl, std::memory_order_relaxed);
                    dispReduction[(size_t) p].store (red, std::memory_order_relaxed);
                }
            }
        }

        // ---- synthesis ----
        fft.performRealOnlyInverseTransform (s);
        for (int j = 0; j < N; ++j)
            cs.out[(size_t) ((pos + j) & mask)] += s[j] * window[(size_t) j] * kOlaScale;
    }
}

} // namespace smoothie
