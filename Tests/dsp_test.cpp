// Headless DSP tests for the Smoothie engine.
#include "Engine.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

using namespace smoothie;

static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++failures; std::printf ("  FAIL: "); std::printf (__VA_ARGS__); std::printf ("\n"); } else { std::printf ("  ok:   "); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

static const double kSR = 48000.0;

static double toneDb (const std::vector<float>& x, size_t start, size_t len, double freq)
{
    double re = 0, im = 0, wsum = 0;
    for (size_t i = 0; i < len; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * M_PI * (double) i / (double) len);
        const double ph = 2.0 * M_PI * freq * (double) (start + i) / kSR;
        re += w * x[start + i] * std::cos (ph);
        im -= w * x[start + i] * std::sin (ph);
        wsum += w;
    }
    return 20.0 * std::log10 (std::sqrt (re * re + im * im) / wsum + 1e-12);
}

static std::vector<float> makePink (size_t n, unsigned seed, float gain)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> d (-1.0f, 1.0f);
    std::vector<float> x (n);
    double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    for (auto& s : x)
    {
        const double w = d (rng);
        b0 = 0.99886 * b0 + w * 0.0555179; b1 = 0.99332 * b1 + w * 0.0750759;
        b2 = 0.96900 * b2 + w * 0.1538520; b3 = 0.86650 * b3 + w * 0.3104856;
        b4 = 0.55000 * b4 + w * 0.5329522; b5 = -0.7616 * b5 - w * 0.0168980;
        s = (float) ((b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362) * 0.11) * gain;
        b6 = w * 0.115926;
    }
    return x;
}

static void runEngine (Engine& e, std::vector<float>& x, const std::vector<float>* sc = nullptr, int block = 480)
{
    float* p = x.data();
    for (size_t i = 0; i < x.size(); i += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, x.size() - i);
        float* ch[1] = { p + i };
        const float* s = sc ? sc->data() + i : nullptr;
        bool act[1] = { true };
        e.process (ch, 1, s, n, act);
    }
}

int main()
{
    const char* names[] = { "LowLatency", "Normal", "LinearPhase" };
    const size_t N = (size_t) (kSR * 8);

    for (int qi = 0; qi < 3; ++qi)
    {
        std::printf ("== quality: %s ==\n", names[qi]);
        Engine e;
        e.prepare (kSR, (Quality) qi);
        const int lat = e.getLatencySamples();

        // 1) unity at depth 0 -> exact delayed copy
        {
            EngineSettings s; s.depth = 0.0f; e.setSettings (s);
            auto x = makePink (N, 1, 0.3f), ref = x;
            runEngine (e, x);
            double maxErr = 0;
            for (size_t i = (size_t) lat; i < N; ++i) maxErr = std::max (maxErr, (double) std::abs (x[i] - ref[i - (size_t) lat]));
            CHECK (maxErr < 1e-4, "depth 0 is transparent, delayed by %d samples (max err %.2e)", lat, maxErr);
        }

        // 2) resonance (1 kHz tone on pink noise) is reduced, neighbours are not
        {
            e.reset();
            EngineSettings s; s.depth = 0.8f; s.selectivity = 0.3f; s.attackMs = 5; s.releaseMs = 100;
            e.setSettings (s);
            auto x = makePink (N, 2, 0.1f);
            for (size_t i = 0; i < N; ++i) x[i] += 0.05f * (float) std::sin (2.0 * M_PI * 1000.0 * (double) i / kSR);
            auto ref = x;
            runEngine (e, x);
            const size_t st = (size_t) (kSR * 5), len = (size_t) (kSR * 2);
            const double before = toneDb (ref, st - (size_t) lat, len, 1000.0), after = toneDb (x, st, len, 1000.0);
            double nBefore = 0, nAfter = 0;
            for (double f : { 300.0, 450.0, 2300.0, 3700.0 })
            {
                nBefore += toneDb (ref, st - (size_t) lat, len, f);
                nAfter  += toneDb (x, st, len, f);
            }
            nBefore /= 4; nAfter /= 4;
            CHECK (before - after > 6.0, "1 kHz resonance reduced by %.1f dB", before - after);
            CHECK (std::abs (nBefore - nAfter) < 2.0, "neighbouring noise changed by only %.2f dB", nAfter - nBefore);
        }

        // 3) hard mode, no NaN, no blow-up
        {
            e.reset();
            EngineSettings s; s.hard = true; s.depth = 1.0f; s.tilt = 0.7f; s.focusLow = 200; s.focusHigh = 8000; s.stereoLink = 0.5f;
            e.setSettings (s);
            auto x = makePink (N, 3, 0.5f);
            runEngine (e, x);
            bool ok = true; float peak = 0;
            for (auto v : x) { if (! std::isfinite (v)) ok = false; peak = std::max (peak, std::abs (v)); }
            CHECK (ok && peak < 4.0f, "hard mode + tilt + focus stable (peak %.2f)", peak);
        }
    }

    // 4) sidechain: detection from sidechain tone reduces 1 kHz in the main signal
    {
        std::printf ("== sidechain ==\n");
        Engine e; e.prepare (kSR, Quality::normal);
        EngineSettings s; s.depth = 1.0f; s.selectivity = 0.2f; e.setSettings (s);
        auto sc = makePink (N, 5, 0.02f);
        for (size_t i = 0; i < N; ++i) sc[i] += 0.1f * (float) std::sin (2.0 * M_PI * 1000.0 * (double) i / kSR);
        auto x = makePink (N, 6, 0.1f); auto ref = x;
        runEngine (e, x, &sc);
        const int lat = e.getLatencySamples();
        const size_t st = (size_t) (kSR * 5), len = (size_t) (kSR * 2);
        const double d = toneDb (ref, st - (size_t) lat, len, 1000.0) - toneDb (x, st, len, 1000.0);
        CHECK (d > 3.0, "sidechain-driven reduction at 1 kHz: %.1f dB", d);
    }

    // 5) 16 channels, CPU
    {
        std::printf ("== 16ch / CPU (LinearPhase, worst case) ==\n");
        Engine e; e.prepare (kSR, Quality::linearPhase);
        EngineSettings s; s.depth = 0.7f; e.setSettings (s);
        const int nCh = 16, block = 512, blocks = (int) (kSR * 4 / block);
        std::vector<std::vector<float>> bufs;
        for (int c = 0; c < nCh; ++c) bufs.push_back (makePink ((size_t) block, (unsigned) c + 10, 0.2f));
        std::vector<float*> ptrs; for (auto& b : bufs) ptrs.push_back (b.data());
        std::vector<char> act (nCh, 1); bool actb[16]; for (int c = 0; c < nCh; ++c) actb[c] = true;
        auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < blocks; ++b) e.process (ptrs.data(), nCh, nullptr, block, actb);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        bool ok = true; for (auto& b : bufs) for (auto v : b) if (! std::isfinite (v)) ok = false;
        CHECK (ok, "no NaN/inf in 16 channels");
        std::printf ("  info: 4 s of 16-ch audio in %.2f s (%.1f%% of one core)\n", secs, secs / 4.0 * 100.0);
        CHECK (secs < 4.0, "16 channels run faster than real time");
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
