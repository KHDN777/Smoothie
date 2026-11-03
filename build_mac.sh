#!/usr/bin/env bash
# Build & install Smoothie (VST3 + AU + Standalone, universal arm64/x86_64) on macOS.
# Requirements: Xcode command line tools (xcode-select --install) and CMake (brew install cmake).
set -euo pipefail
cd "$(dirname "$0")"

command -v cmake >/dev/null || { echo "CMake not found. Run: brew install cmake"; exit 1; }
xcode-select -p >/dev/null 2>&1 || { echo "Xcode command line tools missing. Run: xcode-select --install"; exit 1; }

cmake -S . -B build -G Xcode -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target Smoothie_VST3 Smoothie_AU Smoothie_Standalone -j "$(sysctl -n hw.ncpu)"

# Ad-hoc sign so macOS (especially Apple Silicon) will load the bundles.
for b in "$HOME/Library/Audio/Plug-Ins/VST3/Smoothie.vst3" \
         "$HOME/Library/Audio/Plug-Ins/Components/Smoothie.component"; do
  [ -d "$b" ] && codesign --force --deep --sign - "$b" && xattr -cr "$b" || true
done

# Make Logic / GarageBand / Ableton pick up the new AU without a reboot.
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo
echo "Installed:"
echo "  ~/Library/Audio/Plug-Ins/VST3/Smoothie.vst3"
echo "  ~/Library/Audio/Plug-Ins/Components/Smoothie.component"
echo "Standalone app: build/Smoothie_artefacts/Release/Standalone/Smoothie.app"
echo "Rescan plug-ins in your DAW. (Logic: run 'auval -v aufx Smo1 Smth' to validate the AU.)"
