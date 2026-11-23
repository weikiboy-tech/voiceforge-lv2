# Ardour-Testprotokoll

Status **offen**. Pro Lauf Debian-Version, Architektur, Ardour-Version, Paketname, Sample-Rate, Blockgröße und JACK- oder PipeWire-Konfiguration protokollieren.

1. Paket installieren, Ardour öffnen und den Plugin-Scan ausführen. Beide Varianten im Plugin-Manager suchen.
2. Eine Mono- und eine Stereospur mit bekannten Signalen anlegen. Jeweils die passende Instanz laden und jeden Kanal mit einem eigenen Signal prüfen.
3. Die generische Oberfläche öffnen und schließen. Input Gain, Output Gain, Enabled und Hochpass bedienen.
4. Gain automatisieren und Bypass bei laufender Wiedergabe umschalten. Übergänge auf Klicks abhören.
5. Die Session speichern, Ardour schließen und die Session neu öffnen. Ports und Automation mit den vorherigen Werten vergleichen.
6. Eine Live-Aufnahme prüfen und dieselbe Passage offline exportieren. Bei identischen Einstellungen Pegel und Kanalzuordnung vergleichen.
7. Einen Impuls auf eine bearbeitete und eine trockene Spur legen. Gemeldete Latenz und Position der exportierten Impulse vergleichen.
8. Preset-Auswahl, eigene Presets, A/B, Analyse und eigene GUI prüfen, sobald diese Funktionen implementiert sind.
