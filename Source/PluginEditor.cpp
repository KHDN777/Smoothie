#include "PluginEditor.h"
#include "Presets.h"

using namespace smoothie;
using namespace smoothie::ui;

// ============================================================ LookAndFeel
LookAndFeel::LookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,       col::text);
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,               col::text);
    setColour (juce::ComboBox::textColourId,            col::text);
    setColour (juce::PopupMenu::backgroundColourId,     col::panel);
    setColour (juce::PopupMenu::textColourId,           col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::pink.withAlpha (0.35f));
    setColour (juce::TextButton::buttonColourId,        col::panelHi);
    setColour (juce::TextButton::textColourOffId,       col::text);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float a0, float a1, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float r = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float arcR = r - 3.0f;
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const auto accent = juce::Colour ((juce::uint32) (int) s.getProperties().getWithDefault ("accent", (int) col::pink.getARGB()));
    const float ang = a0 + pos * (a1 - a0);

    // knob body
    g.setColour (col::panelHi);
    g.fillEllipse (c.x - r + 6, c.y - r + 6, 2 * (r - 6), 2 * (r - 6));

    // track
    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, a1, true);
    g.setColour (col::bg);
    g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value
    juce::Path val;
    const float from = bipolar ? (a0 + 0.5f * (a1 - a0)) : a0;
    val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, std::min (from, ang), std::max (from, ang), true);
    g.setColour (accent);
    g.strokePath (val, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // pointer
    juce::Path p;
    const float len = r - 10.0f;
    p.addRoundedRectangle (-1.5f, -len, 3.0f, len * 0.45f, 1.5f);
    g.setColour (col::text);
    g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hl, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const bool isBypass = b.getButtonText() == "BYPASS";
    const auto accent = isBypass ? col::banana : (b.getButtonText() == "DELTA" ? col::mint : col::pink);

    g.setColour (on ? accent.withAlpha (0.9f) : (hl ? col::panelHi.brighter (0.15f) : col::panelHi));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (on ? col::bg : col::text);
    g.setFont (font (12.0f, true));
    g.drawText (b.getButtonText(), r.toNearestInt(), juce::Justification::centred);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    g.setColour (col::panelHi);
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, (float) h * 0.5f);
}

// ============================================================ Knob
Knob::Knob (juce::AudioProcessorValueTreeState& apvts, const char* paramID, const juce::String& title,
            const juce::String& unit, bool bipolar, juce::Colour accent)
{
    slider.getProperties().set ("bipolar", bipolar);
    slider.getProperties().set ("accent", (int) accent.getARGB());
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setColour (juce::Slider::textBoxTextColourId,       col::text);
    slider.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxHighlightColourId,  col::pink.withAlpha (0.4f));
    addAndMakeVisible (slider);

    titleLabel.setText (title.toUpperCase(), juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setFont (font (11.0f, true));
    titleLabel.setColour (juce::Label::textColourId, col::dim);
    addAndMakeVisible (titleLabel);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramID, slider);

    if (unit.isNotEmpty())
    {
        auto prev = slider.textFromValueFunction;
        slider.textFromValueFunction = [prev, unit] (double v) { return prev (v) + unit; };
        slider.updateText();
    }
}

void Knob::resized()
{
    auto r = getLocalBounds();
    titleLabel.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

// ============================================================ SegmentedChoice
SegmentedChoice::SegmentedChoice (juce::AudioProcessorValueTreeState& apvts, const char* paramID,
                                  juce::StringArray opts, const juce::String& cap)
    : options (std::move (opts)), caption (cap)
{
    attachment = std::make_unique<juce::ParameterAttachment> (*apvts.getParameter (paramID),
                                                              [this] (float v) { selected = juce::roundToInt (v); repaint(); });
    attachment->sendInitialUpdate();
}

void SegmentedChoice::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::dim);
    g.setFont (font (11.0f, true));
    g.drawText (caption.toUpperCase(), r.removeFromTop (16.0f).toNearestInt(), juce::Justification::centredLeft);

    g.setColour (col::bg);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);

    const float w = r.getWidth() / (float) options.size();
    for (int i = 0; i < options.size(); ++i)
    {
        auto seg = juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight()).reduced (2.0f);
        if (i == selected)
        {
            g.setColour (col::pink);
            g.fillRoundedRectangle (seg, seg.getHeight() * 0.5f);
        }
        g.setColour (i == selected ? col::bg : col::text);
        g.setFont (font (12.0f, i == selected));
        g.drawText (options[i], seg.toNearestInt(), juce::Justification::centred);
    }
}

void SegmentedChoice::mouseDown (const juce::MouseEvent& e)
{
    if (e.y < 16) return;
    const int idx = juce::jlimit (0, options.size() - 1, (int) (e.position.x / ((float) getWidth() / (float) options.size())));
    attachment->setValueAsCompleteGesture ((float) idx);
}

// ============================================================ SpectrumView
SpectrumView::SpectrumView (SmoothieProcessor& p) : proc (p)
{
    level.fill (-120.0f);
    startTimerHz (30);
}

float SpectrumView::xForFreq (float f, float w) const
{
    return w * std::log (std::max (20.0f, f) / 20.0f) / std::log (1000.0f);
}

void SpectrumView::timerCallback()
{
    const auto& e = proc.getEngine();
    for (int i = 0; i < Engine::numDisplayPoints; ++i)
    {
        const float lv = e.getDisplayLevelDb (i), rd = e.getDisplayReductionDb (i);
        level[(size_t) i]     = lv > level[(size_t) i] ? lv : level[(size_t) i] + (lv - level[(size_t) i]) * 0.25f;
        reduction[(size_t) i] = rd < reduction[(size_t) i] ? rd : reduction[(size_t) i] + (rd - reduction[(size_t) i]) * 0.2f;
    }
    repaint();
}

void SpectrumView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (b, 10.0f);

    auto r = b.reduced (10.0f, 8.0f);
    const float W = r.getWidth(), H = r.getHeight();

    g.saveState();
    g.reduceClipRegion (r.toNearestInt());
    g.setOrigin (r.getPosition().toInt());

    // grid
    g.setFont (font (10.0f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xForFreq (f, W);
        g.setColour (col::bg.withAlpha (0.8f));
        g.drawVerticalLine ((int) x, 0.0f, H);
        if (juce::exactlyEqual (f, 100.0f) || juce::exactlyEqual (f, 1000.0f) || juce::exactlyEqual (f, 10000.0f))
        {
            g.setColour (col::dim);
            g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f),
                        juce::Rectangle<float> (x + 3.0f, H - 14.0f, 40.0f, 12.0f), juce::Justification::left);
        }
    }

    auto yForLevel = [H] (float db) { return H * (1.0f - juce::jlimit (0.0f, 1.0f, (db + 120.0f) / 100.0f)); };

    // input spectrum (3-tap smoothed)
    const int P = Engine::numDisplayPoints;
    juce::Path spec;
    for (int i = 0; i < P; ++i)
    {
        const float v = (level[(size_t) std::max (0, i - 1)] + level[(size_t) i] + level[(size_t) std::min (P - 1, i + 1)]) / 3.0f;
        const float x = xForFreq (Engine::getDisplayFrequency (i), W), y = yForLevel (v);
        if (i == 0) spec.startNewSubPath (x, y); else spec.lineTo (x, y);
    }
    juce::Path fill (spec);
    fill.lineTo (W, H); fill.lineTo (0, H); fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (col::pink.withAlpha (0.45f), 0, 0, col::pink.withAlpha (0.02f), 0, H, false));
    g.fillPath (fill);
    g.setColour (col::pink);
    g.strokePath (spec, juce::PathStrokeType (1.5f));

    // reduction: hangs from the top, 0..-24 dB over 45% of height
    const float redScale = 0.45f * H / 24.0f;
    juce::Path red;
    red.startNewSubPath (0, 0);
    for (int i = 0; i < P; ++i)
    {
        const float v = (reduction[(size_t) std::max (0, i - 1)] + reduction[(size_t) i] + reduction[(size_t) std::min (P - 1, i + 1)]) / 3.0f;
        red.lineTo (xForFreq (Engine::getDisplayFrequency (i), W), -v * redScale);
    }
    red.lineTo (W, 0); red.closeSubPath();
    g.setColour (col::mint.withAlpha (0.35f));
    g.fillPath (red);
    g.setColour (col::mint);
    g.strokePath (red, juce::PathStrokeType (1.5f));

    // focus range shading
    const float lo = proc.apvts.getRawParameterValue (id::focusLow)->load();
    const float hi = proc.apvts.getRawParameterValue (id::focusHigh)->load();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    if (lo > 21.0f) g.fillRect (0.0f, 0.0f, xForFreq (lo, W), H);
    if (hi < 19500.0f) { const float x = xForFreq (hi, W); g.fillRect (x, 0.0f, W - x, H); }

    g.setColour (col::dim);
    g.setFont (font (10.0f, true));
    g.drawText ("INPUT", juce::Rectangle<float> (W - 160, H - 14, 70, 12), juce::Justification::right);
    g.setColour (col::mint);
    g.drawText ("REDUCTION", juce::Rectangle<float> (W - 85, H - 14, 85, 12), juce::Justification::right);

    g.restoreState();
}

// ============================================================ Editor
SmoothieEditor::SmoothieEditor (SmoothieProcessor& p)
    : AudioProcessorEditor (&p), proc (p), spectrum (p),
      depth       (p.apvts, id::depth,       "Depth",       "%", false, col::pink),
      sharpness   (p.apvts, id::sharpness,   "Sharpness",   "%", false, col::pink),
      selectivity (p.apvts, id::selectivity, "Selectivity", "%", false, col::pink),
      detail      (p.apvts, id::detail,      "Detail",      "%", false, col::pink),
      attack      (p.apvts, id::attack,      "Attack",      " ms", false, col::banana),
      release     (p.apvts, id::release,     "Release",     " ms", false, col::banana),
      mix         (p.apvts, id::mix,         "Mix",         "%", false, col::mint),
      trim        (p.apvts, id::trim,        "Trim",        " dB", true, col::mint),
      link        (p.apvts, id::link,        "Stereo Link", "%", false, col::mint),
      tilt        (p.apvts, id::tilt,        "Tilt",        "%", true, col::banana),
      tiltFreq    (p.apvts, id::tiltFreq,    "Tilt Freq",   " Hz", false, col::banana),
      focusLow    (p.apvts, id::focusLow,    "Focus Low",   " Hz", false, col::mint),
      focusHigh   (p.apvts, id::focusHigh,   "Focus High",  " Hz", false, col::mint),
      mode        (p.apvts, id::mode,        { "Soft", "Hard" }, "Mode"),
      quality     (p.apvts, id::quality,     { "Low Latency", "Normal", "Linear Phase" }, "Quality"),
      stereoMode  (p.apvts, id::stereoMode,  { "Stereo", "Mid/Side" }, "Channels"),
      msProcess   (p.apvts, id::msProcess,   { "Both", "Mid", "Side" }, "M/S Process"),
      bypassAtt (p.apvts, id::bypass, bypassBtn),
      deltaAtt  (p.apvts, id::delta, deltaBtn),
      scAtt     (p.apvts, id::sidechain, scBtn)
{
    setLookAndFeel (&laf);

    for (auto* c : std::initializer_list<juce::Component*> { &spectrum, &depth, &sharpness, &selectivity, &detail,
                                                              &attack, &release, &mix, &trim, &link, &tilt, &tiltFreq,
                                                              &focusLow, &focusHigh, &mode, &quality, &stereoMode,
                                                              &msProcess, &bypassBtn, &deltaBtn, &scBtn, &presetBox,
                                                              &prevBtn, &nextBtn })
        addAndMakeVisible (c);

    int idx = 1;
    for (const auto& pr : getFactoryPresets()) presetBox.addItem (pr.name, idx++);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (+1); };
    scBtn.setTooltip ("Use the plugin's sidechain input for resonance detection");

    constrainer.setFixedAspectRatio (900.0 / 580.0);
    constrainer.setSizeLimits (675, 435, 1350, 870);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (900, 580);

    startTimerHz (10);
}

SmoothieEditor::~SmoothieEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SmoothieEditor::timerCallback()
{
    const int want = proc.getCurrentProgram() + 1;
    if (presetBox.getSelectedId() != want)
        presetBox.setSelectedId (want, juce::dontSendNotification);

    const bool ms = proc.apvts.getRawParameterValue (id::stereoMode)->load() > 0.5f;
    msProcess.setAlpha (ms ? 1.0f : 0.35f);
    msProcess.setEnabled (ms);
}

void SmoothieEditor::stepPreset (int d)
{
    const int n = proc.getNumPrograms();
    proc.setCurrentProgram ((proc.getCurrentProgram() + d + n) % n);
}

void SmoothieEditor::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (col::bg.brighter (0.06f), 0, 0, col::bg, 0, b.getHeight(), false));
    g.fillAll();

    const float s = (float) getWidth() / 900.0f;

    // wordmark
    const auto wordFont = font (30.0f * s, true);
    juce::GlyphArrangement ga;
    ga.addLineOfText (wordFont, "smoothie", 0.0f, 0.0f);
    const float wordW = ga.getBoundingBox (0, -1, true).getWidth();
    g.setColour (col::text);
    g.setFont (wordFont);
    g.drawText ("smoothie", juce::Rectangle<int> ((int) (18 * s), (int) (8 * s), (int) (200 * s), (int) (44 * s)), juce::Justification::centredLeft);
    g.setColour (col::pink);
    g.fillEllipse (18 * s + wordW + 3 * s, 37 * s, 8 * s, 8 * s);

    // panels behind the control rows
    g.setColour (col::panel);
    g.fillRoundedRectangle (14 * s, 322 * s, 872 * s, 126 * s, 10 * s);
    g.fillRoundedRectangle (14 * s, 458 * s, 872 * s, 112 * s, 10 * s);
}

void SmoothieEditor::resized()
{
    const float s = (float) getWidth() / 900.0f;
    auto S = [s] (float v) { return (int) std::round (v * s); };

    // top bar
    prevBtn.setBounds (S (300), S (15), S (28), S (28));
    presetBox.setBounds (S (332), S (15), S (230), S (28));
    nextBtn.setBounds (S (566), S (15), S (28), S (28));
    scBtn.setBounds (S (640), S (15), S (96), S (28));
    deltaBtn.setBounds (S (742), S (15), S (70), S (28));
    bypassBtn.setBounds (S (818), S (15), S (68), S (28));

    spectrum.setBounds (S (14), S (56), S (872), S (258));

    // main knob row
    const int rowY = S (326), rowH = S (118), rowX = S (14), rowW = S (872);
    juce::Component* mains[] = { &depth, &sharpness, &selectivity, &detail, &attack, &release, &mix, &trim };
    const int kw = rowW / 8;
    for (int i = 0; i < 8; ++i) mains[i]->setBounds (rowX + i * kw, rowY, kw, rowH);

    // lower row: choices left, small knobs right
    const int y2 = S (462), h2 = S (104);
    mode.setBounds       (S (30),  y2 + S (6),  S (150), S (46));
    quality.setBounds    (S (192), y2 + S (6),  S (240), S (46));
    stereoMode.setBounds (S (30),  y2 + S (54), S (150), S (46));
    msProcess.setBounds  (S (192), y2 + S (54), S (240), S (46));

    juce::Component* smalls[] = { &link, &tilt, &tiltFreq, &focusLow, &focusHigh };
    const int sx = S (450), sw = S (436) / 5;
    for (int i = 0; i < 5; ++i) smalls[i]->setBounds (sx + i * sw, y2, sw, h2);
}
