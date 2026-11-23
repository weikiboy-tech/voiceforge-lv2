# Architektur

Die Pipeline liest beide Eingangssamples eines Frames. Die Reihenfolge ist Input Gain → Restauration → Hochpass → Expander → EQ → De-Esser → Kompressor → Output Gain → Limiter → Bypass-Mischung. Die Spracherkennung analysiert das Signal nach Input Gain. Beide Kanäle verwenden dieselben geglätteten Steuerwerte. Filterzustände sind kanalgetrennt. Expander, De-Esser, Kompressor, De-Plosive und Limiter teilen ihren Gain je Modul zwischen den Kanälen.

LV2 instanziiert den DSP-Kern mit Samplerate und Kanalzahl. `connect_port` übernimmt die Host-Puffer. `activate` setzt die temporären Zustände zurück. Beim ersten Audioframe übernimmt der DSP die aktuellen Ports. Anschließend liest `run` die Zielwerte blockweise und glättet sie sampleweise. Der Latenz-Kontrollport meldet auch bei einem Durchlauf mit null Frames den gewählten Modus. Studio verzögert das trockene Bypass-Signal um dieselbe Anzahl Samples. Beim Moduswechsel mischt eine 5-ms-Zeitkonstante die beiden Signalpfade. Die exakte zeitliche Ausrichtung gilt vor und nach der Überblendung. Die Bedienanleitung empfiehlt deshalb den Wechsel bei gestopptem Transport.

Alle gespeicherten Bedieneinstellungen sind LV2-Control-Inputs mit stabilen Symbolen. Der Host speichert und restauriert diese Werte. 0.2.0 hängt optionale Ports an die bestehenden Indizes an. Bei fehlenden Ports greifen neutrale Vorgaben. A/B-Sätze werden später über LV2 State ergänzt.

`run` arbeitet mit vorbereiteten Puffern für Klickerkennung, Studio-Verzögerung und FFT. Größe und FFT-Tabellen werden bei der Instanziierung festgelegt. Live nutzt keine FFT, wenn der Modus auf Live steht. Der Audiopfad verwendet keine Allokation, Dateizugriffe, Locks oder UI-Aufrufe. Die Freigabe der Instanz erfolgt im LV2-Lifecycle.
