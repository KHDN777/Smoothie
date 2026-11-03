#pragma once

#include "Engine.h"
#include "Parameters.h"
#include <juce_audio_processors/juce_audio_processors.h>

class SmoothieProcessor : public juce::AudioProcessor
{
public:
    SmoothieProcessor();
    ~SmoothieProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Smoothie"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioParameterBool* getBypassParameter() const override { return bypassParam; }

    const smoothie::Engine& getEngine() const noexcept { return engine; }

    juce::AudioProcessorValueTreeState apvts;

private:
    void processInternal (juce::AudioBuffer<float>&, bool hostBypassed);
    smoothie::EngineSettings readSettings() const;

    static constexpr int kChunk   = 2048;
    static constexpr int kRingLen = 8192; // > max latency (4096)

    smoothie::Engine engine;

    std::array<std::vector<float>, smoothie::Engine::maxChannels> wet, ring;
    std::vector<float> scMono, mixRamp, trimRamp, byRamp;
    int ringPos = 0;

    juce::SmoothedValue<float> mixSm, trimSm, bypassSm;

    std::atomic<float>* pMode = nullptr; std::atomic<float>* pQuality = nullptr;
    std::atomic<float>* pStereoMode = nullptr; std::atomic<float>* pMsProcess = nullptr;
    std::atomic<float>* pMix = nullptr; std::atomic<float>* pTrim = nullptr;
    std::atomic<float>* pDelta = nullptr; std::atomic<float>* pSidechain = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;

    int currentProgram = 0;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmoothieProcessor)
};
