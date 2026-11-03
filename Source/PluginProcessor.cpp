#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

using namespace smoothie;

SmoothieProcessor::SmoothieProcessor()
    : AudioProcessor (BusesProperties()
                         .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                         .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                         .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "SMOOTHIE", createParameterLayout())
{
    pMode       = apvts.getRawParameterValue (id::mode);
    pQuality    = apvts.getRawParameterValue (id::quality);
    pStereoMode = apvts.getRawParameterValue (id::stereoMode);
    pMsProcess  = apvts.getRawParameterValue (id::msProcess);
    pMix        = apvts.getRawParameterValue (id::mix);
    pTrim       = apvts.getRawParameterValue (id::trim);
    pDelta      = apvts.getRawParameterValue (id::delta);
    pSidechain  = apvts.getRawParameterValue (id::sidechain);
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (id::bypass));

    for (auto& w : wet)  w.assign ((size_t) kChunk, 0.0f);
    for (auto& r : ring) r.assign ((size_t) kRingLen, 0.0f);
    scMono.assign ((size_t) kChunk, 0.0f);
    mixRamp.assign ((size_t) kChunk, 0.0f);
    trimRamp.assign ((size_t) kChunk, 0.0f);
    byRamp.assign ((size_t) kChunk, 0.0f);

    engine.prepare (44100.0, Quality::normal);
}

bool SmoothieProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (in.isDisabled() || in != out)           return false;
    if (in.size() > Engine::maxChannels)        return false;

    const auto sc = l.getChannelSet (true, 1);
    return sc.isDisabled() || sc == juce::AudioChannelSet::mono() || sc == juce::AudioChannelSet::stereo();
}

void SmoothieProcessor::prepareToPlay (double sr, int)
{
    currentSampleRate = sr;
    engine.prepare (sr, (Quality) juce::roundToInt (pQuality->load()));
    setLatencySamples (engine.getLatencySamples());

    for (auto& r : ring) std::fill (r.begin(), r.end(), 0.0f);
    ringPos = 0;

    mixSm.reset (sr, 0.02);   mixSm.setCurrentAndTargetValue (pMix->load() * 0.01f);
    trimSm.reset (sr, 0.02);  trimSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pTrim->load()));
    bypassSm.reset (sr, 0.01); bypassSm.setCurrentAndTargetValue (1.0f);
}

EngineSettings SmoothieProcessor::readSettings() const
{
    auto get = [this] (const char* idStr) { return apvts.getRawParameterValue (idStr)->load(); };

    EngineSettings s;
    s.depth       = get (id::depth) * 0.01f;
    s.sharpness   = get (id::sharpness) * 0.01f;
    s.selectivity = get (id::selectivity) * 0.01f;
    s.detail      = get (id::detail) * 0.01f;
    s.attackMs    = get (id::attack);
    s.releaseMs   = get (id::release);
    s.hard        = juce::roundToInt (pMode->load()) == 1;
    s.tilt        = get (id::tilt) * 0.01f;
    s.tiltFreq    = get (id::tiltFreq);
    s.focusLow    = get (id::focusLow);
    s.focusHigh   = get (id::focusHigh);
    s.stereoLink  = get (id::link) * 0.01f;
    return s;
}

void SmoothieProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    processInternal (buffer, false);
}

void SmoothieProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    processInternal (buffer, true);
}

void SmoothieProcessor::processInternal (juce::AudioBuffer<float>& buffer, bool hostBypassed)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainBus = getBusBuffer (buffer, true, 0);
    const int nCh = std::min (mainBus.getNumChannels(), Engine::maxChannels);
    const int total = buffer.getNumSamples();
    if (nCh == 0 || total == 0) return;

    // quality change => new FFT size => new latency
    const auto q = (Quality) juce::roundToInt (pQuality->load());
    if (q != engine.getQuality())
    {
        engine.setQuality (q);
        for (auto& r : ring) std::fill (r.begin(), r.end(), 0.0f);
        setLatencySamples (engine.getLatencySamples());
    }
    engine.setSettings (readSettings());
    const int latency = engine.getLatencySamples();

    // stereo mode
    const bool midSide = nCh == 2 && juce::roundToInt (pStereoMode->load()) == 1;
    const int  msProc  = juce::roundToInt (pMsProcess->load());
    bool active[Engine::maxChannels];
    for (int c = 0; c < nCh; ++c) active[c] = true;
    if (midSide) { active[0] = msProc != 2; active[1] = msProc != 1; }

    // sidechain
    juce::AudioBuffer<float> scBus;
    bool useSc = false;
    if (pSidechain->load() > 0.5f)
        if (auto* b = getBus (true, 1))
            if (b->isEnabled() && b->getNumberOfChannels() > 0) { scBus = getBusBuffer (buffer, true, 1); useSc = true; }

    const bool delta = pDelta->load() > 0.5f;
    mixSm.setTargetValue (pMix->load() * 0.01f);
    trimSm.setTargetValue (juce::Decibels::decibelsToGain (pTrim->load()));
    bypassSm.setTargetValue ((hostBypassed || (bypassParam != nullptr && bypassParam->get())) ? 0.0f : 1.0f);

    for (int start = 0; start < total; start += kChunk)
    {
        const int n = std::min (kChunk, total - start);

        for (int i = 0; i < n; ++i)
        {
            mixRamp[(size_t) i]  = mixSm.getNextValue();
            trimRamp[(size_t) i] = trimSm.getNextValue();
            byRamp[(size_t) i]   = bypassSm.getNextValue();
        }

        float* io[Engine::maxChannels];
        float* wp[Engine::maxChannels];
        for (int c = 0; c < nCh; ++c)
        {
            io[c] = mainBus.getWritePointer (c) + start;
            wp[c] = wet[(size_t) c].data();
            std::copy (io[c], io[c] + n, wp[c]);
        }

        if (midSide)
            for (int i = 0; i < n; ++i)
            {
                const float l = wp[0][i], r = wp[1][i];
                wp[0][i] = 0.5f * (l + r);
                wp[1][i] = 0.5f * (l - r);
            }

        const float* scPtr = nullptr;
        if (useSc)
        {
            const int scCh = scBus.getNumChannels();
            for (int i = 0; i < n; ++i)
            {
                float s = 0.0f;
                for (int c = 0; c < scCh; ++c) s += scBus.getReadPointer (c)[start + i];
                scMono[(size_t) i] = s / (float) scCh;
            }
            scPtr = scMono.data();
        }

        engine.process (wp, nCh, scPtr, n, active);

        if (midSide)
            for (int i = 0; i < n; ++i)
            {
                const float m = wp[0][i], s = wp[1][i];
                wp[0][i] = m + s;
                wp[1][i] = m - s;
            }

        // latency-aligned dry path, then mix / delta / trim / bypass
        for (int c = 0; c < nCh; ++c)
        {
            auto& rg = ring[(size_t) c];
            int w = ringPos;
            for (int i = 0; i < n; ++i)
            {
                const float dryNow = io[c][i];
                rg[(size_t) w] = dryNow;
                const float dry = rg[(size_t) ((w - latency) & (kRingLen - 1))];
                w = (w + 1) & (kRingLen - 1);

                const float mix = mixRamp[(size_t) i];
                const float mixed = dry + mix * (wp[c][i] - dry);        // dry*(1-mix) + wet*mix
                const float processed = (delta ? (dry - mixed) : mixed) * trimRamp[(size_t) i];
                io[c][i] = dry + byRamp[(size_t) i] * (processed - dry);
            }
        }
        ringPos = (ringPos + n) & (kRingLen - 1);
    }

    // clear any channels beyond what we process (e.g. unused sidechain-adjacent outputs)
    for (int c = nCh; c < mainBus.getNumChannels(); ++c) mainBus.clear (c, 0, total);
}

// ---- presets ----
int SmoothieProcessor::getNumPrograms() { return (int) getFactoryPresets().size(); }

const juce::String SmoothieProcessor::getProgramName (int index)
{
    const auto& p = getFactoryPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? juce::String (p[(size_t) index].name) : juce::String();
}

void SmoothieProcessor::setCurrentProgram (int index)
{
    const auto& presets = getFactoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    currentProgram = index;

    // keep the session-level controls (trim, delta, sidechain, bypass) as they are
    const juce::StringArray keep { id::trim, id::delta, id::sidechain, id::bypass };
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! keep.contains (rp->paramID))
                rp->setValueNotifyingHost (rp->getDefaultValue());

    for (const auto& v : presets[(size_t) index].values)
        if (auto* rp = apvts.getParameter (v.param))
            rp->setValueNotifyingHost (rp->convertTo0to1 (v.value));
}

// ---- state ----
void SmoothieProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void SmoothieProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentProgram = juce::jlimit (0, getNumPrograms() - 1, (int) state.getProperty ("program", 0));
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* SmoothieProcessor::createEditor() { return new SmoothieEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SmoothieProcessor(); }
