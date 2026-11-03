// Renders the editor to a PNG with live audio having passed through the processor (dev tool).
#include "PluginProcessor.h"
#include <random>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    SmoothieProcessor p;
    p.setPlayConfigDetails (2, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);
    if (argc > 2) p.setCurrentProgram (std::atoi (argv[2]));

    std::mt19937 rng (7);
    std::uniform_real_distribution<float> d (-1.0f, 1.0f);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    double b0 = 0, b1 = 0, b2 = 0; long t = 0;
    for (int blk = 0; blk < 400; ++blk)
    {
        for (int i = 0; i < 512; ++i, ++t)
        {
            const double w = d (rng);
            b0 = 0.99765 * b0 + w * 0.0990460; b1 = 0.96300 * b1 + w * 0.2965164; b2 = 0.57000 * b2 + w * 1.0526913;
            float x = (float) ((b0 + b1 + b2 + w * 0.1848) * 0.06);
            x += 0.05f * (float) std::sin (2 * 3.14159265 * 400.0 * (double) t / 48000.0);
            x += 0.04f * (float) std::sin (2 * 3.14159265 * 2800.0 * (double) t / 48000.0);
            x += 0.03f * (float) std::sin (2 * 3.14159265 * 6200.0 * (double) t / 48000.0);
            buf.setSample (0, i, x); buf.setSample (1, i, x);
        }
        p.processBlock (buf, midi);
    }

    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (900, 580);
    struct Stopper : juce::Timer { void timerCallback() override { juce::MessageManager::getInstance()->stopDispatchLoop(); } } stopper;
    stopper.startTimer (800);
    juce::MessageManager::getInstance()->runDispatchLoop();
    auto img = ed->createComponentSnapshot (ed->getLocalBounds());
    juce::File out (argc > 1 ? argv[1] : "snapshot.png");
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    return 0;
}
