#!/bin/sh
set -eu

build=${1:-build-linux}
bundle=$build/lv2/voiceforge.lv2
ctest --test-dir "$build" --output-on-failure --no-tests=error -R '^dsp(_voice)?$'
ctest --test-dir "$build" --output-on-failure --no-tests=error -R '^lv2_host$'
for file in "$bundle"/manifest.ttl "$bundle"/voiceforge-mono.ttl "$bundle"/voiceforge-stereo.ttl; do
    serdi -i turtle "$file" >/dev/null
done
if command -v lv2_validate >/dev/null && command -v sord_validate >/dev/null; then
    lv2_validate "$bundle"/*.ttl
fi
ldd "$bundle/voiceforge.so" >"$build/runtime-dependencies.txt"
if grep -q 'not found' "$build/runtime-dependencies.txt"; then
    cat "$build/runtime-dependencies.txt" >&2
    exit 1
fi
cat "$build/runtime-dependencies.txt"
