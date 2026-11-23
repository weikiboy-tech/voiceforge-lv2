# Änderungsverlauf

## 0.3.0~preview1

- De-Click mit lokalem Impulsdetektor und Interpolation. De-Plosive mit dynamischer Bassabsenkung.
- Adaptive spektrale Rauschreduktion und klassische Spracherkennung mit Hysterese.
- Live verarbeitet ohne zusätzliche Samples Verzögerung. Studio stellt De-Click und spektrale Rauschreduktion bereit und meldet die Verzögerung des Puffers an den Host.

## 0.2.0~preview2

- LV2-Minor-Version für die neu hinzugekommenen optionalen Ports erhöht.
- Offline-Regressionstest mit realer Sprachaufnahme und reproduzierbare Störsignale für Phase 3.

## 0.2.0~preview1

- Low Shelf, vier parametrische EQ-Bänder und High Shelf zusätzlich zum Hochpass.
- Stereo-gekoppelter Expander, De-Esser und Soft-Knee-Kompressor. De-Esser und Kompressor liefern Gain-Reduction-Werte.
- Neue Module starten ausgeschaltet. Die Portsymbole und Indizes der Erstversion bleiben für bestehende Sessions stabil.
- Signaltests und automatisierte Prüfungen mit AddressSanitizer und UndefinedBehaviorSanitizer.

## 0.1.0~preview1

- LV2-Adapter für Mono und Stereo.
- Input Gain, Output Gain, geglätteter Bypass und regelbarer Hochpass.
- Optionaler Sample-Peak-Limiter mit Stereo-Kopplung, einstellbarem Ceiling und Gain-Reduction-Port.
- Kontrollportbasierte Session-Speicherung und Null-Latenzmeldung.
- Debian-CI mit Bundle-, Paket- und Installationstests.
