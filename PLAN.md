# Entwicklungsplan

Ziel ist ein offline arbeitendes C++20-LV2-Plugin für Ardour unter Debian. Debian 12 dient als Build-Basis für amd64. Windows baut und testet denselben DSP-Kern ohne Host.

1. Mono/Stereo, Gain, Bypass, Host-Zustand, Tests und Paketierung.
2. Hochpass, EQ, Expander, Kompressor, De-Esser und Sample-Peak-Limiter mit deterministischen Signaltests.
3. De-Click, De-Plosive, VAD und spektrale Rauschreduktion anhand lizenzierter Sprachaufnahmen bewerten.
4. Level Rider, standardbasierte Lautheit und True Peak, Studio-Modus mit verifizierter Latenz. Dereverb zunächst experimentell evaluieren.
5. LV2-UI, Messanzeigen, Presets und persistente A/B-Sätze. GUI und DSP bleiben getrennt.
6. Begrenzte Eingangsanalyse und bestätigungspflichtige Übernahme von Empfehlungen.
7. Debian-Paket und Benutzerarchiv über geprüfte CI-Builds bereitstellen. ARM64 auf einem nativen Debian-Runner prüfen.
8. Artefakte auf Debian installieren und die Ardour-Checkliste vollständig ausführen. Erst danach kommt 1.0 infrage.
