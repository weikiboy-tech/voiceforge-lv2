# Installation für Ardour unter Debian 12

Lade das vorab getestete Artefakt für amd64 oder arm64 unter **Artifacts** des erfolgreichen [GitHub-Actions-Laufs](https://github.com/weikiboy-tech/voiceforge-lv2/actions/runs/45246651264) herunter und entpacke den Actions-Download. `dpkg --print-architecture` zeigt die Architektur des Zielrechners.

## Debian-Paket

```sh
sudo apt install ./voiceforge-lv2_0.5.0~preview1_amd64.deb
```

Auf arm64 verwende entsprechend `voiceforge-lv2_0.5.0~preview1_arm64.deb`. Das Paket installiert unter `/usr/lib/lv2/voiceforge.lv2`. Entfernen lässt es sich mit `sudo apt remove voiceforge-lv2`.

## Ohne Root-Rechte

Entpacke das Linux-Archiv in ein Verzeichnis und führe dort `sh install.sh` aus. Derselbe Befehl aktualisiert eine vorhandene Benutzerinstallation. `sh uninstall.sh` entfernt sie. VoiceForge liegt unter `~/.lv2/voiceforge.lv2`. Verwende einen Installationsweg pro Rechner. Für die eigene Oberfläche benötigt das Benutzerarchiv `libcairo2` und `libx11-6`. Beim Debian-Paket löst APT die Abhängigkeiten auf.

## In Ardour

1. Ardour neu starten und den Plugin-Manager öffnen.
2. Nach **VoiceForge LV2** suchen.
3. Im Plugin-Insert der Spur die passende Mono- oder Stereo-Variante hinzufügen.
4. VoiceForge doppelt anklicken. Unter **Presets** das Werkspreset **Natural Voice** wählen und auf **Load** klicken. Für Monitoring **Live** wählen.
5. Mit Sprache abhören, den Ausgangspegel einstellen und die Session speichern. Falls Ardour stattdessen generische Regler öffnet, prüfe den Start in einer Linux-Sitzung mit X11 oder XWayland.

Für **De-Click** und **Spectral Noise Reduction** stelle bei gestopptem Transport **Processing Mode** auf **Studio** und aktiviere die gewünschten Module. Die Rauschreduktion lernt aus leisen Passagen. Spiele bei Bedarf zuerst ein kurzes Stück mit dem Raum- oder Lüftergeräusch ab. Für Sprachmonitoring bleibt **Live** bei null zusätzlicher Sample-Verzögerung. **De-Plosive** steht in beiden Modi zur Verfügung.

## Analyse und Vergleich

Spiele einen Abschnitt ab und klicke im Tab **Analyze Voice** auf **Analyze Voice**. Nach bis zu acht Sekunden erscheint eine Auswertung. **Stop analysis** wertet einen kürzeren Abschnitt aus. **Apply recommendations** übernimmt die Änderungen. **Compare** wechselt danach zwischen den ursprünglichen und den vorgeschlagenen Werten. **Reset** leert die Analyse.

Die Buttons **A**, **B** und **Copy** vergleichen und kopieren Parametersätze. Verwende beim Vergleich denselben Live-/Studio-Modus, wenn der Latenzausgleich unverändert bleiben soll. Eigene Presets lassen sich im Menü speichern, umbenennen und löschen.

**A/B differences** zeigt die Werte aller Regler, die sich zwischen A und B unterscheiden. **Invert Input Polarity** und **Remove DC Offset** befinden sich in **Voice processing → Input / EQ**. Die experimentelle Hallreduktion steht unter **Restoration → Dereverb** im Studio-Modus zur Verfügung.
