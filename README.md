# Smoothie

A dynamic resonance suppressor plug-in (VST3 / AU / Standalone), built with JUCE. Smoothie listens to the
spectrum in real time, finds resonances and harshness that stick out of the local spectral balance, and turns
them down only where and when they occur.

## Controls
| Control | What it does |
|---|---|
| **Depth** | How much of each detected resonance is removed |
| **Sharpness** | Narrow, surgical cuts (high) vs. broad, gentle cuts (low) |
| **Selectivity** | How prominent a peak must be before it is touched |
| **Detail** | How local the "what counts as a resonance" reference is: high = finer, more surgical |
| **Attack / Release** | How fast reduction engages / lets go |
| **Soft / Hard** | Soft = transparent, static resonances. Hard = also reacts to dynamics, compressor-like |
| **Tilt + Tilt Freq** | Shifts processing toward the lows (-) or highs (+) around a pivot frequency |
| **Focus Low / High** | Limits processing to a frequency range (visible as shading in the display) |
| **Quality** | Low Latency (≈11 ms @ 48k) · Normal (≈21 ms) · Linear Phase (≈85 ms, zero-phase STFT, highest resolution) |
| **Channels / M/S Process** | Stereo, or Mid/Side with Both / Mid only / Side only |
| **Stereo Link** | 0 = every channel reduced independently, 100 = identical reduction |
| **Sidechain** | Detect resonances in the sidechain input instead of the main signal |
| **Mix / Trim** | Dry-wet and output gain (-24..+24 dB) |
| **Delta** | Listen to only what is being removed |
| **Bypass** | Latency-compensated, click-free |

* Multichannel: any layout with matching input/output up to 16 channels (9.1.6).
* Latency is reported to the host for automatic delay compensation; the dry path is aligned for Mix and Delta.
* 13 factory presets (host program list + preset browser in the GUI). All parameters are automatable.
* The display shows the input spectrum (pink) and live gain reduction (mint, hanging from the top).

## Build on a Mac
```bash
xcode-select --install      # once
brew install cmake          # once
./build_mac.sh
```
This builds a universal (Apple Silicon + Intel) VST3, AU and Standalone app, installs the plug-ins to
`~/Library/Audio/Plug-Ins/`, and ad-hoc signs them. Rescan plug-ins in your DAW.

No Mac handy? Push this folder to a GitHub repo: the included workflow (`.github/workflows/build.yml`)
builds the universal macOS binaries on GitHub's Mac runners and publishes them as a downloadable artifact.

To share binaries with other people, sign with a Developer ID certificate and notarize them (Apple requirement).

## Developing
* `Tests/dsp_test.cpp`: headless DSP tests (transparency at depth 0, resonance reduction, sidechain, 16 ch).
  Build target `SmoothieTests`.
* `Source/Engine.*`: the spectral engine. `Source/PluginProcessor.*`: plug-in shell. `Source/PluginEditor.*`: GUI.
* JUCE is downloaded automatically by CMake (or drop it into `./JUCE`).

## Licence note
JUCE is dual-licensed (AGPLv3 or a commercial licence). Distributing Smoothie binaries requires complying with
one of those; selling closed-source binaries requires a JUCE commercial licence.
