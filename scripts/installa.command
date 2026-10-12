#!/bin/bash
# Installa PF-2 Fuzz nelle cartelle dei plugin dell'utente e rimuove la quarantena
cd "$(dirname "$0")"
set -e
VST3="$HOME/Library/Audio/Plug-Ins/VST3"
AU="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3" "$AU"
rm -rf "$VST3/PF-2 Fuzz.vst3" "$AU/PF-2 Fuzz.component"
cp -R "PF-2 Fuzz.vst3" "$VST3/"
cp -R "PF-2 Fuzz.component" "$AU/"
rm -rf "/Applications/PF-2 Fuzz.app" 2>/dev/null || true
if ! cp -R "PF-2 Fuzz.app" "/Applications/" 2>/dev/null; then
  mkdir -p "$HOME/Applications"; rm -rf "$HOME/Applications/PF-2 Fuzz.app"; cp -R "PF-2 Fuzz.app" "$HOME/Applications/"
fi
xattr -dr com.apple.quarantine "$VST3/PF-2 Fuzz.vst3" "$AU/PF-2 Fuzz.component" 2>/dev/null || true
xattr -dr com.apple.quarantine "/Applications/PF-2 Fuzz.app" "$HOME/Applications/PF-2 Fuzz.app" 2>/dev/null || true
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo ""
echo "PF-2 Fuzz installato:"
echo "  VST3: $VST3"
echo "  AU:   $AU"
echo "Riavvia la DAW per vederlo."
