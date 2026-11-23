# VoiceForge LV2

VoiceForge LV2 ist ein Offline-Sprachprozessor für Ardour unter Debian. Die Vorabversion 0.2.0 enthält Mono und Stereo, Input Gain und Output Gain, geglätteten Bypass, Hochpass, Sechsband-EQ, Expander, De-Esser, Kompressor und Sample-Peak-Limiter. Der Host stellt die Bedienelemente und Messports bereit. Die Audiokette meldet null Samples Verzögerung.

## Installation und Verwendung

Das Debian-Paket und das Benutzerarchiv werden nach erfolgreichem Build und Installationstest im [Debian-Workflow](https://github.com/weikiboy-tech/voiceforge-lv2/actions) bereitgestellt. Für die Installation und Verwendung in Ardour siehe [Installation](docs/installation.md).

## Build

Debian-Basis ist Bookworm. Die benötigten Pakete stehen im [Dockerfile](packaging/debian/Dockerfile). Unter Debian erzeugt `sh scripts/build.sh` das getestete Bundle und `sh scripts/package.sh` die Installationsartefakte.

Unter Windows kann der C++20-DSP-Kern mit einem Visual-Studio-CMake-Generator gebaut werden.

```powershell
cmake -S . -B build-win -DVOICEFORGE_LV2=OFF
cmake --build build-win --config Release
ctest --test-dir build-win -C Release --output-on-failure
```

Die [Testdokumentation](docs/testing.md) nennt den Prüfstand. Ardour wurde noch nicht manuell geprüft. Spektrale Restauration, Analyse, eigene Oberfläche mit EQ-Kurve und Presets stehen auf der [Prüfliste](TASKS.md).
