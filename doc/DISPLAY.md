# Anzeige und Bildrate

Unter Optionen → Grafik werden Fenstergröße und Bildrate eingestellt. OK wendet
die Auswahl an und speichert sie; Abbrechen verwirft die Auswahl.

- **Fenster:** frei skalierbar. Die angebotenen Startgrößen müssen in die nutzbare
  Fläche des Monitors passen, auf dem das Spielfenster liegt. Die Liste enthält
  übliche Fenstergrößen und passende, von SDL gemeldete Displayauflösungen, ohne
  Duplikate. Fenstergrößen brauchen keinen eigenen exklusiven Bildschirmmodus.
- **Fullscreen:** weiterhin randloses Vollbild in der aktuellen Desktopauflösung
  und mit der aktuellen Desktopfrequenz. Die Fenstergrößen-Auswahl ist ausgeblendet.
  Es gibt keinen zusätzlichen exklusiven Vollbildmodus.
- **Bildrate / Auto:** übernimmt die aktuelle Monitorfrequenz und fordert VSync
  im gewählten SDL- oder OpenGL-Renderer an. Ein zusätzlicher Software-Limiter
  begrenzt die Ausgabe auch dann, wenn VSync vom Treiber nicht unterstützt wird.
- **Bildrate / manuell:** wählbares FPS-Limit, höchstens bis zur aktuellen
  Monitorfrequenz. VSync wird ausgeschaltet; der Software-Limiter übernimmt.
  Ein FPS-Limit ist keine Umschaltung der Bildschirmfrequenz. Beispielsweise sind
  30 FPS auch auf einem Bildschirm möglich, der nur im 60-Hz-Modus betrieben wird.
- Beim Wechsel des Monitors oder seiner Desktopfrequenz werden die Daten erneut
  abgefragt (spätestens nach etwa 500 ms); wirksame FPS-Limits werden angepasst.
  Wenn der Treiber keine Frequenz meldet, werden vorsichtig 60 Hz angenommen.
- Die tatsächliche Leistung hängt weiterhin von CPU/GPU und Treibereinstellungen
  ab. Eine Monitorangabe garantiert keine erreichte Render-FPS-Zahl.
- Das Spielfeld und die logischen UI-Koordinaten bleiben 800×600 beziehungsweise
  4:3. Anders geformte Fenster bekommen schwarze Ränder. Mauspositionen werden
  entsprechend umgerechnet. Unter Windows bleibt DPI-Awareness aktiv.

## Simulation und Darstellung

`gamefps` bleibt das bestehende Spieltempo: normal 75 feste Physikschritte pro
Sekunde, unabhängig von `render_fps`. `render_fps=0` bedeutet Auto. Ein hoher
Renderwert erhöht nicht mehr das Spieltempo; bei niedriger Renderbildrate werden
mehrere Physikschritte zwischen zwei Bildern berechnet.

Die Spielschleife verwendet einen hochauflösenden Zeitgeber und einen Akkumulator.
Nach langen Unterbrechungen werden höchstens 250 ms nachgeholt, damit etwa das
Verschieben/Blockieren eines Fensters keinen minutenlangen Aufholvorgang auslöst.
Normale Renderverzögerungen werden mit unveränderten Physikschritten nachgeholt.
Server, Netzwerkpakete, Bots, Replay-Aufzeichnung und Replay-Geschwindigkeit
bleiben an Simulationsschritte gebunden.

Für die Darstellung werden Kopien zweier aufeinanderfolgender Spielzustände
interpoliert: Ballposition, Ballwinkel und Blobpositionen. Die Interpolation läuft
einen Physikschritt hinter der Simulation; bei 75 Schritten sind das etwa 13,3 ms.
Sie verändert weder Kollisionen noch gespeicherte oder versendete Zustände.
Aufschlag-/Punktwechsel und große Positionssprünge werden direkt dargestellt.
Nahtwinkel werden über den kürzesten Winkelweg interpoliert. Die Spritephasen
der Blobs bleiben ihre bestehenden Einzelbilder.

Menülogik läuft ebenfalls im Simulationsrhythmus. Die letzte Liste von
Zeichenbefehlen wird für zusätzliche Renderbilder wiederverwendet. Ereignisse
werden zwischen den Schritten gepuffert und nach einem Schritt verbraucht;
ein Tastendruck wird daher weder durch ein Zwischenbild verloren noch bei
mehreren nachgeholten Schritten mehrfach als Menüaktion verarbeitet.

## Prüfung

Der Asset-/Renderingtest enthält den Framerate-Regressionstest:

- Identische Ball- und Blobpositionen, Geschwindigkeiten und Ballwinkel für
  identische Eingaben bei 30/60/75/144/240 Render-FPS und 30/75/120 Spielschritten.
- Begrenztes Aufholen nach Unterbrechungen, unveränderte Simulationszustände bei
  Interpolation, korrekter Winkelübergang und direktes Zurücksetzen bei Punkten.
- Gültige FPS-Limits auch bei 59/144/165 Hz, gepufferte Eingabe und wiederholte
  UI-Ausgabe ohne erneuten Spielschritt.
- Echte SDL-Spielzustands-Interpolation sowie Grafikmenü mit Auto, manuellem Limit
  und ausgeblendeter Fenstergröße bei gewähltem Vollbild.
- Gespeicherte und erneut geladene Replay-Datei mit identischer Wiedergabe und
  bytegleichen serialisierten Netzwerkzuständen bei 30/60/75/144 Render-FPS.

SDL- und OpenGL-Darstellung sowie Auto-/manuelle VSync-Ansteuerung wurden zusätzlich
mit den echten Windows-Grafiktreibern geprüft. Die SDL-Abfrage erkennt auf dem
Testrechner beide 2560×1440-Monitore mit 165 Hz; auch die Zuordnung beim Verschieben
eines Testfensters auf den zweiten Monitor wurde geprüft. SDL erhält bei bewegten
Objekten die interpolierten Nachkommastellen bis zum Zeichnen.

Ein LAN-Gegentest mit zweitem Rechner, Monitore mit unterschiedlichen Frequenzen
und erzwungene Treibereinstellungen ergänzen die automatisierten Prüfungen.
