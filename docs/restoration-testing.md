# Klangprüfung für Phase 3

Die Testsignale basieren auf einer deutschen CC0-Sprachaufnahme. Aus derselben Referenz erzeugt der Generator getrennte Varianten mit stationärem Lüftergeräusch, vier Samples langen Klicks und einem tieffrequenten Plosivstoß. Er gibt die betroffenen Samplepositionen aus. Vor und nach der Wiederherstellung bleibt die Referenz mit den Varianten ausgerichtet. Bei späteren Modulen mit Lookahead muss der Vergleich die gemeldete Latenz berücksichtigen. [Quelle und Prüfsumme der Aufnahme](../tests/fixtures/README.md)

```sh
python tests/audio/make_restoration_fixtures.py build-fixtures
```

1. Für Rauschreduktion den RMS-Pegel im sprachfreien Vorlauf messen. Das verbleibende Rauschen in den Sprachpausen und mögliche tonale Artefakte abhören.
2. Für De-Click die bekannten Impulsfenster mit der sauberen Referenz vergleichen. Den Fehler außerhalb der Fenster separat messen und natürliche Konsonanten abhören.
3. Für De-Plosive die Energie unter 120 Hz im markierten Fenster vergleichen. Den Unterschied außerhalb des Fensters prüfen und tiefe Vokale abhören.
4. Für VAD Aufnahme und Störungen getrennt prüfen. Fehlalarme im Vorlauf, an Klicks und am Plosivstoß sowie übersehene leise Silben protokollieren.
5. A/B-Dateien bei vergleichbarem Pegel abhören. Erst danach Standardeinstellungen festlegen und weitere Stimmen und Aufnahmen ergänzen.

Die Störungen sind für Regressionstests kontrolliert hinzugefügt. Reale Klicks, Windstöße und verrauschte Raumaufnahmen bleiben Teil der Hörprüfung.
