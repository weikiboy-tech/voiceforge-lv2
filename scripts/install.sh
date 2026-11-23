#!/bin/sh
set -eu

fail() { printf 'VoiceForge: %s\n' "$*" >&2; exit 1; }
here=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
source=${1:-"$here/voiceforge.lv2"}
[ "$(uname -s)" = Linux ] || fail 'Der Installer erfordert Linux.'
files='VERSION manifest.ttl voiceforge-mono.ttl voiceforge-stereo.ttl voiceforge.so voiceforge-ui.ttl voiceforge_ui.so presets.ttl'
for file in $files; do
    [ -f "$source/$file" ] || fail "Datei fehlt im Bundle: $file"
done
version=$(cat "$source/VERSION")
valid_version() { printf '%s\n' "$1" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+(~preview[0-9]+)?$'; }
valid_version "$version" || fail 'Ungültige Bundle-Version.'
command -v dpkg >/dev/null || fail 'Für die Versionsprüfung wird dpkg benötigt.'
case "$(uname -m)" in
    x86_64) machine='3e00' ;;
    aarch64) machine='b700' ;;
    *) fail "Nicht unterstützte Architektur: $(uname -m)" ;;
esac
header=$(od -An -tx1 -N 20 "$source/voiceforge.so" | tr -d ' \n')
case "$header" in
    7f454c460201????????????????????????"$machine") ;;
    *) fail 'Die Plugin-Datei passt nicht zur Linux-Architektur.' ;;
esac

dir=${HOME:?HOME fehlt}/.lv2
target=$dir/voiceforge.lv2
mkdir -p -- "$dir"
if [ -e "$target" ] || [ -L "$target" ]; then
    [ -d "$target" ] && [ ! -L "$target" ] && [ -f "$target/VERSION" ] &&
        [ -f "$target/manifest.ttl" ] && grep -q 'weikiboy-tech.github.io/voiceforge-lv2/mono' "$target/manifest.ttl" ||
        fail "Vorhandenes Verzeichnis ist kein verwaltetes VoiceForge-Bundle: $target"
    old=$(cat "$target/VERSION")
    valid_version "$old" || fail 'Vorhandenes Bundle hat eine ungültige Version.'
    if dpkg --compare-versions "$old" gt "$version"; then
        fail "Vorhandene Version $old ist neuer als $version."
    fi
fi
work=$(mktemp -d "$dir/.voiceforge-install.XXXXXX")
cleanup() {
    if [ -d "$work/previous" ]; then
        if [ ! -e "$target" ] && [ ! -L "$target" ]; then
            mv -T -- "$work/previous" "$target" || return
        elif [ -d "$work/next" ]; then
            printf 'VoiceForge: Vorheriges Bundle bleibt unter %s erhalten.\n' "$work/previous" >&2
            return
        fi
    fi
    rm -rf -- "$work"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
mkdir -- "$work/next"
for file in $files; do
    install -m 644 -- "$source/$file" "$work/next/$file"
done
if [ -d "$target" ]; then
    mv -T -- "$target" "$work/previous"
fi
mv -T -- "$work/next" "$target"
printf 'VoiceForge %s installiert unter %s\n' "$version" "$target"
