# DSP

Input und Output Gain reichen von −24 bis +24 dB. Die linearen Verstärkungen und der Bypass-Anteil folgen einem Einpol-Glätter mit 5 ms Zeitkonstante. Bypass verwendet die unverstärkte Quelle.

Der Hochpass subtrahiert den Einpol-Tiefpass vom Direktsignal. Der Pol folgt der diskretisierten Zeitkonstante des Grenzfrequenzreglers. 0 Hz bedeutet aus, bei positiven Werten gilt intern 20 bis 300 Hz. Filterkoeffizient und Effektanteil werden geglättet. NaN und Infinity am Eingang werden zu Stille, arithmetische Zwischenwerte werden auf den darstellbaren Float-Bereich begrenzt.

Der optionale Sample-Peak-Limiter nutzt den höchsten Betrag beider Kanäle und wendet denselben Gain an. Die Absenkung reagiert sofort, die Erholung folgt einem Einpol-Glätter mit 80 ms Zeitkonstante. Der Gain-Reduction-Port meldet die höchste angewandte Absenkung pro Block. Das Ceiling wirkt bei vollständig aktivem Limiter auf Samples. True-Peak-Messung und Lookahead stehen aus.

## EQ

Neben dem Hochpass stehen zwei Shelves und vier Glockenfilter zur Verfügung. Jedes Band hat Schalter, Frequenz, Pegel und Güte. Die Filter nutzen doppelt genaue trapezintegrierte Zustandsvariablen. Der DSP glättet die Koeffizienten und begrenzt die Arbeitsfrequenz auf 47,5 % der Samplerate. [Herleitung des Zustandsvariablenfilters](https://www.cytomic.com/files/dsp/SvfLinearTrapOptimised.pdf)

## Dynamik

- Der Expander senkt Pegel unterhalb der Schwelle mit einem 6-dB-Soft-Knee ab. Die maximale Absenkung ist einstellbar. Attack bestimmt das Öffnen, Release das Schließen.
- Der Kompressor verwendet die höhere Kanalamplitude zur Peak-Erkennung. Er bildet die Übersteuerung mit einer quadratischen Soft-Knee-Kurve auf Gain Reduction ab und glättet die Absenkung in dB. Das automatische Makeup schätzt den Verlust bei einem Referenzpegel von −12 dBFS und ist auf 12 dB begrenzt. Dry/Wet und manuelles Makeup sind unabhängig einstellbar. [Grundlagen zur Kompressorkennlinie und Ballistik](https://aes.org/publications/elibrary/elibrary-page/?id=16354)
- Der De-Esser erkennt Pegel in einem abstimmbaren Bandpass. Er mischt bei Überschreitung der Schwelle bis zu 24 dB Absenkung im bearbeiteten Band ein. Der Reduktionsport zeigt den stärksten angewandten Wert des Blocks.
