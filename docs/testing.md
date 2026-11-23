# Prüfstand

Entwicklung unter Windows 11, Build 26200. Visual Studio Build Tools 2022 und das mitgelieferte CMake sind vorhanden. Docker wurde im Suchpfad nicht gefunden. WSL ist laut Systemmeldung nicht installiert.

- Bestanden unter Windows mit MSVC 19.44 und CTest (Release). Der DSP-Test prüft 8, 44,1, 48, 96 und 192 kHz mit Stille, Sinus, wechselnden Blockgrößen, In-Place-Verarbeitung, dynamischem Gain und Bypass sowie nicht endlichen Eingangswerten. Alle vier Shell-Skripte bestanden `bash -n`.
- Der Debian-Workflow prüft beim Lauf die Turtle-Dateien und instanziiert Mono und Stereo mit Lilv. Er verifiziert Portzahl, Gain, Wiederherstellung der Host-Portwerte und Null-Latenz. Die Paketstufe prüft Debian- und Benutzerinstallation, Wiederholungsinstallation und Deinstallation.
- Reale Ardour-Integration, bewertete Sprachaufnahmen, Langzeittests und ARM64 stehen aus. Die [Ardour-Checkliste](ardour-testing.md) ist offen.
