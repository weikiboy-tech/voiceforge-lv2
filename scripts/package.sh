#!/bin/sh
set -eu

root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
build=${1:-"$root/build-linux"}
dist=${2:-"$root/dist"}
mkdir -p -- "$dist"
cpack --config "$build/CPackConfig.cmake" -G DEB -B "$dist"
stage=$(mktemp -d)
trap 'rm -rf -- "$stage"' EXIT
cp -R "$build/lv2/voiceforge.lv2" "$stage/"
cp "$root/scripts/install.sh" "$root/scripts/uninstall.sh" "$root/LICENSE" "$stage/"
arch=$(dpkg --print-architecture)
tar -C "$stage" -czf "$dist/voiceforge-lv2-linux-$arch.tar.gz" .
