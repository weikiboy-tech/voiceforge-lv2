#!/bin/sh
set -eu

root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
build=${1:-"$root/build-linux"}
type=${2:-Release}
cmake -S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE="$type" -DBUILD_TESTING=ON -DVOICEFORGE_LV2=ON
cmake --build "$build"
sh "$root/scripts/verify.sh" "$build"
