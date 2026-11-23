# Architektur

Die Pipeline liest beide Eingangssamples eines Frames, wendet Input Gain, optionalen Hochpass und Output Gain an und blendet für den Bypass auf das trockene Signal. Beide Kanäle verwenden dieselben geglätteten Steuerwerte. Der Filterzustand ist pro Kanal getrennt.

LV2 instanziiert den DSP-Kern mit Samplerate und Kanalzahl. `connect_port` übernimmt die Host-Puffer. `activate` setzt temporäre Filterzustände zurück. Beim ersten Audioframe übernimmt der DSP die aktuellen Ports. Anschließend liest `run` die Zielwerte blockweise und glättet sie sampleweise. Der Latenz-Kontrollport meldet auch bei einem Durchlauf mit null Frames null Samples.

Alle gespeicherten Bedieneinstellungen sind LV2-Control-Inputs mit stabilen Symbolen. Der Host speichert und restauriert diese Werte. Die Phase 1 benötigt keine separate State-Dictionary-Eigenschaft. A/B-Sätze werden später über LV2 State ergänzt.

`run` arbeitet mit fester Instanzgröße und lokalen Arrays von zwei Samples. Der Audiopfad verwendet keine Allokation, Dateizugriffe, Locks oder UI-Aufrufe. Die Freigabe der Instanz erfolgt im LV2-Lifecycle.
