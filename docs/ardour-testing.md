# Ardour-Testprotokoll

Automatisiert geprüft in Debian 12 mit Ardour 7.3, virtuellem Audiogerät und 48 kHz. Beide Plugin-Varianten, Werkspreset und Session-Wiederherstellung bestanden auf amd64 und arm64. Der amd64-Desktop-Test öffnete und schloss beide eingebetteten Plugin-Fenster, nahm ein Testsignal auf und verglich den Offline-Export in Live und Studio. Pro Lauf am Zielgerät Debian-Version, Architektur, Ardour-Version, Paketname, Sample-Rate, Blockgröße und JACK- oder PipeWire-Konfiguration protokollieren. [Debian-Prüflauf](https://github.com/weikiboy-tech/voiceforge-lv2/actions/runs/45246651264)

1. Paket installieren, Ardour öffnen und den Plugin-Scan ausführen. Beide Varianten im Plugin-Manager suchen.
2. Eine Mono- und eine Stereospur mit bekannten Signalen anlegen. Jeweils die passende Instanz laden und jeden Kanal mit einem eigenen Signal prüfen.
3. Die generische Oberfläche öffnen und schließen. Input Gain, Output Gain, Enabled und Hochpass bedienen. Jedes EQ-Band und Dynamikmodul separat einschalten. Gain-Reduction-Ports für Kompressor, De-Esser und Limiter bei anliegendem Signal prüfen.
4. Gain automatisieren und Bypass bei laufender Wiedergabe umschalten. Übergänge auf Klicks abhören.
5. Die Session speichern, Ardour schließen und die Session neu öffnen. Ports und Automation mit den vorherigen Werten vergleichen.
6. Eine Live-Aufnahme prüfen und dieselbe Passage offline exportieren. Bei identischen Einstellungen Pegel und Kanalzuordnung vergleichen.
7. Einen Impuls auf eine bearbeitete und eine trockene Spur legen. Live muss null zusätzliche Samples melden. Studio bei gestopptem Transport wählen und die Position der exportierten Impulse mit Latenzausgleich vergleichen. Bypass im Studio-Modus muss zeitlich ausgerichtet bleiben. Zusätzlich die Sprachaktivitätsanzeige und die Restaurationsmodule mit einer bekannten Aufnahme prüfen.
8. Preset-Auswahl, eigene Presets, A/B, Analyse und die eigene GUI in Ardour mit echtem Audiomaterial prüfen.
