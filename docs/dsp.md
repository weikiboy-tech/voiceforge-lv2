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

## Restauration

- De-Click sucht nach einem auffälligen lokalen Pegelsprung und einer zeitnahen Rückkehr zu stabilen Nachbarsamples. Abrupte Änderungen der Steigung an beiden Rändern unterscheiden einen Klick von einem steilen, glatten Sprachanteil. Bei Treffer werden die Samples linear interpoliert. Die Vorschau ist auf gerundete 0,65 ms plus zwei Samples begrenzt. Hohe Empfindlichkeit erhöht die Wahrscheinlichkeit, natürliche Transienten zu erfassen.
- De-Plosive erkennt einen Anstieg der Bassenergie im Verhältnis zu Breitbandenergie und langsamem Basspegel. Die resultierende kurzzeitige Bassabsenkung ist stereo-gekoppelt.
- Spracherkennung vergleicht breitbandige und auf das Sprachband begrenzte Quadratmittel über 10-ms-Fenster. Drei aufeinanderfolgende Treffer lösen Aktivität aus. Eine Haltezeit überbrückt kurze Sprachpausen. Der Wert hilft der spektralen Rauschprofilschätzung und wird als Kontrollport ausgegeben.
- Studio-Rauschreduktion nutzt eine STFT mit vierfacher Überlappung, Quadratwurzel-Hann-Fenstern und einer gemeinsamen Mono-/Stereo-Maske. Der Rauschboden wird aus Nicht-Sprachfenstern unter dem Lernschwellwert aktualisiert. Eine Untergrenze, zeitliche Glättung, Glättung benachbarter Bins und Sprachschutz begrenzen Artefakte. [Spektrale Subtraktion](https://ieeexplore.ieee.org/abstract/document/1163209/) · [Fensterung und Overlap-Add](https://www.dsprelated.com/freebooks/sasp/Overlap_Add_OLA_STFT_Processing.html)

Studio verzögert das Signal um die FFT-Größe plus die Klickvorschau. Bei 48 kHz sind das 1.058 Samples (22,04 ms), auch bei deaktivierten Studio-Modulen. Live hat keine zusätzliche Sample-Verzögerung.

Optionaler DC-Blocker und Polaritätsumkehr arbeiten vor der Restaurationskette. Die Überblendung des 5-Hz-DC-Blockers und der Vorzeichenwechsel werden mit 5 ms geglättet.

Der experimentelle Dereverb nutzt dieselbe Studio-STFT und benötigt daher keine zusätzliche Verzögerung. Im Abklingen nach lauten Ereignissen begrenzen gleitende spektrale und breitbandige Hüllkurven die späten Anteile. Der Sprachschutz reduziert den Eingriff bei erkannter Sprache. Das Verfahren ist konservativ, werkseitig ausgeschaltet und kann bei starkem Nachhall oder stark überlappenden Reflexionen zu wenig trennen. Bewertung mit realen Raumaufnahmen steht aus. [Hintergrund zur spektralen Nachhallabsenkung](https://www.ee.columbia.edu/~dpwe/papers/LebBD01-ssdereverv.pdf)

## Level Rider, Messung und Analyse

Der Level Rider kombiniert ein geglättetes Quadratmittel mit der Spracherkennung. Bei geringer Pegelenergie und in Pausen geht er zurück auf Einheitsverstärkung. Ziel, maximale Anhebung, maximale Absenkung, Zeitkonstante und Einfluss der Spracherkennung sind einstellbar.

Das Lautheitsmeter nutzt K-Weighting und summiert die Energie beider Kanäle. Momentary nutzt 400 ms, Short-Term drei Sekunden. Integrated verwendet 400-ms-Fenster bei 100-ms-Schritten, −70 LUFS absolutes und −10 LU relatives Gate. Ein Histogramm in 0,1-LU-Schritten hält den Speicherverbrauch über die Sitzungsdauer konstant. True Peak ist das Maximum über Samples und eine Fenster-Sinc-Interpolation, vierfach bis unter 96 kHz und zweifach ab 96 kHz. Die Tests enthalten Referenztöne und einen Inter-Sample-Peak. Eine vollständige EBU-Konformitätsprüfung mit dem offiziellen Prüfsatz steht aus. [ITU-R BS.1770-5](https://www.itu.int/rec/R-REC-BS.1770-5-202311-I/en) · [EBU Tech 3341](https://tech.ebu.ch/publications/tech3341/)

Die Sprachanalyse misst bis zu acht Sekunden der Eingangsspur. Sie schätzt den Rauschpegel nur anhand der von der Spracherkennung als Pause erkannten Samples. Bass, Höhen und Sibilanz sind Energieindikatoren. Die integrierte Lautheit der Analyse wird separat aus dem Eingang ermittelt. Die Analyse verändert keine Regler, bis der Nutzer die Empfehlungen übernimmt.
