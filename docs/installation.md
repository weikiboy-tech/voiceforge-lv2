# Installation unter Debian

Lade das vorab getestete amd64-Artefakt unter **Artifacts** eines erfolgreichen [GitHub-Actions-Laufs](https://github.com/weikiboy-tech/voiceforge-lv2/actions) herunter und entpacke den Actions-Download.

## Debian-Paket

```sh
sudo apt install ./voiceforge-lv2_0.1.0~preview1_amd64.deb
```

Das Paket installiert unter `/usr/lib/lv2/voiceforge.lv2`. Entfernen lässt es sich mit `sudo apt remove voiceforge-lv2`.

## Ohne Root-Rechte

Entpacke das Linux-Archiv in ein Verzeichnis und führe dort `sh install.sh` aus. Derselbe Befehl aktualisiert eine vorhandene Benutzerinstallation. `sh uninstall.sh` entfernt sie. VoiceForge liegt unter `~/.lv2/voiceforge.lv2`. Verwende für dieselbe Plugin-Version einen der beiden Installationswege.

## In Ardour

1. Ardour neu starten und den Plugin-Manager öffnen.
2. Nach **VoiceForge LV2** suchen.
3. Im Plugin-Insert der Spur die passende Mono- oder Stereo-Variante hinzufügen.
4. Die generische Plugin-Oberfläche öffnen und Ein- und Ausgangspegel einstellen. Den Hochpass bei Bedarf auf eine positive Frequenz setzen. **Enabled** schaltet die Bearbeitung um.
5. Mit Sprache abhören und die Session speichern.
