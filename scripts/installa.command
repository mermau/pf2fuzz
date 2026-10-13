#!/bin/bash
# Installa Collateral Effects FYN-2 nelle cartelle di sistema dei plugin
# (/Library/Audio/Plug-Ins) e rimuove la quarantena. Chiede la password di amministratore.
cd "$(dirname "$0")"
set -e
VST3="/Library/Audio/Plug-Ins/VST3"
AU="/Library/Audio/Plug-Ins/Components"
echo "Installazione di FYN-2 (serve la password di amministratore)..."
sudo mkdir -p "$VST3" "$AU"

# rimuove la versione precedente "PF-2 Fuzz" ovunque fosse installata
for d in "$VST3" "$AU" "$HOME/Library/Audio/Plug-Ins/VST3" "$HOME/Library/Audio/Plug-Ins/Components"; do
  sudo rm -rf "$d/PF-2 Fuzz.vst3" "$d/PF-2 Fuzz.component"
done
sudo rm -rf "/Applications/PF-2 Fuzz.app" "$HOME/Applications/PF-2 Fuzz.app"

sudo rm -rf "$VST3/FYN-2.vst3" "$AU/FYN-2.component" "/Applications/FYN-2.app"
sudo cp -R "FYN-2.vst3" "$VST3/"
sudo cp -R "FYN-2.component" "$AU/"
sudo cp -R "FYN-2.app" "/Applications/"
sudo xattr -dr com.apple.quarantine "$VST3/FYN-2.vst3" "$AU/FYN-2.component" "/Applications/FYN-2.app" 2>/dev/null || true
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo ""
echo "Collateral Effects FYN-2 installato:"
echo "  VST3: $VST3/FYN-2.vst3"
echo "  AU:   $AU/FYN-2.component"
echo "  App:  /Applications/FYN-2.app"
echo "Riavvia la DAW per vederlo. Le chitarre salvate con PF-2 Fuzz vengono recuperate."
