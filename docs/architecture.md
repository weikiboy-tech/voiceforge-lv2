# Architektur

Die Pipeline liest beide Eingangssamples eines Frames. Die Reihenfolge ist Input Gain → Hochpass → Expander → EQ → De-Esser → Kompressor → Output Gain → Limiter → Bypass-Mischung. Beide Kanäle verwenden dieselben geglätteten Steuerwerte. Filterzustände sind kanalgetrennt. Expander, De-Esser, Kompressor und Limiter teilen ihren Gain je Modul zwischen den Kanälen.

LV2 instanziiert den DSP-Kern mit Samplerate und Kanalzahl. `connect_port` übernimmt die Host-Puffer. `activate` setzt temporäre Filterzustände zurück. Beim ersten Audioframe übernimmt der DSP die aktuellen Ports. Anschließend liest `run` die Zielwerte blockweise und glättet sie sampleweise. Der Latenz-Kontrollport meldet auch bei einem Durchlauf mit null Frames null Samples.

Alle gespeicherten Bedieneinstellungen sind LV2-Control-Inputs mit stabilen Symbolen. Der Host speichert und restauriert diese Werte. 0.2.0 hängt optionale Ports an die bestehenden Indizes an. Bei fehlenden Ports greifen neutrale Vorgaben. A/B-Sätze werden später über LV2 State ergänzt.

`run` arbeitet mit fester Instanzgröße und lokalen Arrays von zwei Samples. Der Audiopfad verwendet keine Allokation, Dateizugriffe, Locks oder UI-Aufrufe. Die Freigabe der Instanz erfolgt im LV2-Lifecycle.
