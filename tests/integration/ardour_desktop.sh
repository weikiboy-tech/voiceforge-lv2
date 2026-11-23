#!/bin/sh
set -eu
output=$1
mkdir -p "$output"
export LC_ALL=C ARDOUR_TRY_AUTOSTART_ENGINE=1
config=$XDG_CONFIG_HOME/ardour7
mkdir -p "$config"
touch "$config/.a7"
# The headless and desktop tools can assign different names to the same dummy
# device. Omit the hint only from the disposable test session.
python3 - "$VOICEFORGE_ARDOUR_SESSION/VoiceForgeQA.ardour" <<'PY'
from pathlib import Path
import re
import sys
session = Path(sys.argv[1])
source = session.read_text(encoding="utf-8")
session.write_text(re.sub(r"<EngineHints\b[^>]*/>", "", source), encoding="utf-8")
PY
cat > "$config/config" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<Ardour>
 <Config>
  <Option name="try-autostart-engine" value="1"/>
  <Option name="hide-dummy-backend" value="0"/>
 </Config>
 <Extra>
  <AudioMIDISetup>
   <EngineStates>
    <State backend="None (Dummy)" driver="Normal Speed" device="Sine Wave" input-device="Sine Wave" output-device="Sine Wave" sample-rate="48000" buffer-size="1024" n-periods="0" input-latency="0" output-latency="0" active="1" use-buffered-io="0" midi-option="None"/>
   </EngineStates>
  </AudioMIDISetup>
 </Extra>
</Ardour>
EOF
ardour -a -n "$VOICEFORGE_ARDOUR_SESSION" > "$output/ardour-desktop.log" 2>&1 &
app=$!
trap 'kill "$app" 2>/dev/null || :; wait "$app" 2>/dev/null || :' EXIT HUP INT TERM
# A newly configured session can still show Ardour's audio setup dialog.
for attempt in $(seq 1 60); do
    id=$(xdotool search --onlyvisible --name 'VoiceForgeQA.*Ardour|Ardour.*VoiceForgeQA' 2>/dev/null | head -n1 || :)
    if [ -n "$id" ]; then
        xdotool windowfocus "$id"
        # The rightmost mode buttons sit twenty pixels from the content edge.
        width=$(xdotool getwindowgeometry --shell "$id" | sed -n 's/^WIDTH=//p')
        xdotool mousemove --window "$id" "$((width - 21))" 72 click 1
        sleep .4
        break
    fi
    dialog=$(xdotool search --onlyvisible --name 'Audio/MIDI Setup|Audio.*Setup' 2>/dev/null | head -n1 || :)
    if [ -n "$dialog" ]; then
        xdotool windowfocus "$dialog" key Return
    fi
    if ! kill -0 "$app" 2>/dev/null; then
        cat "$output/ardour-desktop.log" >&2
        exit 1
    fi
    sleep .25
done
xwininfo -root -tree > "$output/ardour-window-tree.txt"
build-linux/voiceforge_ui_test --desktop "$output/ardour-desktop.png"
test -n "$id"
echo "Ardour desktop opened VoiceForgeQA"

for x in 340 448; do
    xdotool mousemove --window "$id" "$x" 207 click --repeat 2 --delay 80 1
    panel=
    for attempt in $(seq 1 20); do
        panel=$(xdotool search --onlyvisible --name '^VoiceForge LV2 Panel$' 2>/dev/null | head -n1 || :)
        [ -z "$panel" ] || break
        sleep .2
    done
    test -n "$panel"
    build-linux/voiceforge_ui_test --desktop "$output/voiceforge-in-ardour-$x.png"
    build-linux/voiceforge_ui_test --close-editor "$panel"
    sleep .3
    if xdotool search --onlyvisible --name '^VoiceForge LV2 Panel$' >/dev/null 2>&1; then
        echo 'Ardour did not close the native VoiceForge editor.' >&2
        exit 1
    fi
done
echo "Ardour opened and closed both embedded VoiceForge editors"

xdotool mousemove --window "$id" 305 332 click 1
xdotool mousemove --window "$id" 209 43 click 1
xdotool mousemove --window "$id" 159 43 click 1
sleep 2
xdotool mousemove --window "$id" 182 43 click 1
sleep .4
xdotool windowfocus "$id" key ctrl+s
sleep .5
test -n "$(find "$VOICEFORGE_ARDOUR_SESSION/interchange" -type f -name '*.wav' -size +96000c -print -quit)"
echo "Ardour recorded two seconds from its virtual audio device"
