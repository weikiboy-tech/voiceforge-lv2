#!/bin/sh
set -eu

target=${HOME:?HOME fehlt}/.lv2/voiceforge.lv2
if [ ! -e "$target" ] && [ ! -L "$target" ]; then
    printf 'VoiceForge ist im Benutzerverzeichnis nicht installiert.\n'
    exit 0
fi
if [ -L "$target" ] || [ ! -f "$target/VERSION" ] || [ ! -f "$target/manifest.ttl" ] ||
   ! grep -q 'weikiboy-tech.github.io/voiceforge-lv2/mono' "$target/manifest.ttl"; then
    printf 'VoiceForge: Kein verwaltetes Bundle unter %s.\n' "$target" >&2
    exit 1
fi
rm -f -- "$target/VERSION" "$target/manifest.ttl" "$target/voiceforge-mono.ttl" \
    "$target/voiceforge-stereo.ttl" "$target/voiceforge.so" "$target/voiceforge-ui.ttl" \
    "$target/voiceforge_ui.so" "$target/presets.ttl"
rmdir -- "$target"
printf 'VoiceForge wurde deinstalliert.\n'
