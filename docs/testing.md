# Prüfstand

Entwicklung unter Windows 11, Build 26200. Visual Studio Build Tools 2022 und das mitgelieferte CMake sind vorhanden. Docker wurde im Suchpfad nicht gefunden. WSL ist laut Systemmeldung nicht installiert.

- Bestanden unter Windows mit MSVC 19.44 und CTest (Debug und Release). Der DSP-Test prüft 8, 44,1, 48, 96 und 192 kHz mit Stille, Sinus, wechselnden Blockgrößen, In-Place-Verarbeitung, dynamischem Gain und Bypass sowie nicht endlichen Eingangswerten. Der Sample-Peak-Limiter wurde auf Ceiling, Erholungszeit, Stereo-Kopplung und Gain-Reduction geprüft. Alle vier Shell-Skripte bestanden `bash -n`.
- Bestanden im [Debian-12-amd64-Lauf](https://github.com/weikiboy-tech/voiceforge-lv2/actions/runs/45233929919) für Commit `bd0c4c1` mit GCC 12.2, CMake 3.25.1, LV2 1.18.4 und Lilv 0.24.14. `lv2_validate` meldete null Fehler. Lilv lud beide Varianten und prüfte Portzahl, Gain, Wiederherstellung eines Host-Portwerts, Bypass, Null-Latenz und Limiter. Debian-Paket, Benutzerinstallation, Wiederholungsinstallation und Deinstallation bestanden. Das Paket deklariert `libc6 (>= 2.27)` und `libstdc++6 (>= 5)`.
- Reale Ardour-Integration, bewertete Sprachaufnahmen, Langzeittests und ARM64 stehen aus. Die [Ardour-Checkliste](ardour-testing.md) ist offen.
