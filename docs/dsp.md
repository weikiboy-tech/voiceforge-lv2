# DSP

Input und Output Gain reichen von −24 bis +24 dB. Die linearen Verstärkungen und der Bypass-Anteil folgen einem Einpol-Glätter mit 5 ms Zeitkonstante. Bypass verwendet die unverstärkte Quelle.

Der Hochpass subtrahiert den Einpol-Tiefpass vom Direktsignal. Der Pol folgt der diskretisierten Zeitkonstante des Grenzfrequenzreglers. 0 Hz bedeutet aus, bei positiven Werten gilt intern 20 bis 300 Hz. Filterkoeffizient und Effektanteil werden geglättet. NaN und Infinity am Eingang werden zu Stille, arithmetische Zwischenwerte werden auf den darstellbaren Float-Bereich begrenzt.

Der optionale Sample-Peak-Limiter nutzt den höchsten Betrag beider Kanäle und wendet denselben Gain an. Die Absenkung reagiert sofort, die Erholung folgt einem Einpol-Glätter mit 80 ms Zeitkonstante. Der Gain-Reduction-Port meldet die höchste angewandte Absenkung pro Block. Das Ceiling wirkt bei vollständig aktivem Limiter auf Samples. True-Peak-Messung und Lookahead stehen aus.
