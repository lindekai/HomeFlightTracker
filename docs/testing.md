# Tests

## Teststrategie

| Ebene | Werkzeug | Inhalt |
|---|---|---|
| Build | Arduino IDE / `arduino-cli compile` | fehlerfrei, keine Warnungen im eigenen Code |
| Host-Unit-Tests | C++-Compiler auf dem Entwicklungsrechner: `tests/host/run.sh` | Geometrie, Kontingentlogik (ab M2); JSON-Auswertung mit Mock-Daten (ab M3) |
| Hardwaretests | Firmware auf dem Gerät + serieller Monitor | Display, Touch, Speicher, WLAN, TLS, OpenSky |
| Langzeittest (M5) | Gerät ≥ 24 h im Dauerbetrieb | Heap-Verlauf, Reconnects, Token-Erneuerung |

## M1 – Hardware-Machbarkeitstest

**Vorbereitung:** Firmware gemäß [setup.md](setup.md) flashen und den seriellen Monitor mit 115200 Baud öffnen.

**Bedienung der Test-Firmware:**

| Bildschirm | Inhalt | Bedienung |
|---|---|---|
| Startbild | Hinweis „Neu kalibrieren“ | innerhalb von 2,5 s antippen erzwingt die Kalibrierung |
| Kalibrierung | vier Eckmarken nacheinander | jeweils genau antippen |
| Testbild | Randlinie, farbige Eckmarken, Pfeil „OBEN“, Farbbalken, Graustufen | Tippen → Touch-Test |
| Touch-Test | Raster aus Trefferzonen (80 × 80 px), Koordinaten, Rohwerte, Druck, IRQ-Pegel | Zone antippen; Schaltflächen Testbild / Kalibr. / Drehen / Info |
| Info | Chip, Flash, PSRAM, Rotation, Kalibrierwerte, Sprite-Test, Heap, Laufzeit | Touch / Licht / Testbild |

Serielle Befehle: `c` kalibrieren, `r` drehen, `b` Licht-Test, `h` Heap, `k` Kalibrierwerte, `x` Einstellungen löschen und neu starten, `?` Hilfe.

### Prüfschritte und Abnahmekriterien

| Nr. | Prüfung | Erwartung | Ergebnis |
|---|---|---|---|
| 1.1 | Platinenrevision und ESP32 | V01-03, ESP32-WROOM-32 | ✅ abgelesen |
| 1.2 | Pinprüfung | seriell: „Pins stimmen mit BoardPins.h ueberein“ | ✅ TFT_eSPI 2.5.43, Treiber 0x9341, 27 MHz |
| 2.1 | Testbild in Rotation 0 | Randlinie an allen vier Kanten vollständig, „OBEN“ zeigt im Gehäuse nach oben | ✅ Sichtprüfung am Gerät |
| 2.2 | Farben | Balken R, G, B, W, Y, C, M in der beschrifteten Farbe; Graustufen gleichmäßig | ✅ Sichtprüfung am Gerät |
| 2.3 | Rotation | mit „Drehen“ alle vier Rotationen; Eckmarken wandern korrekt mit; Rotation bleibt nach Neustart erhalten | ✅ Sichtprüfung am Gerät |
| 2.4 | Hintergrundbeleuchtung | an nach dem Start; „Licht“ schaltet 1,5 s aus und wieder an | ✅ Sichtprüfung am Gerät |
| 3.1 | Kalibrierung | wird beim ersten Start automatisch angeboten; Werte plausibel (seriell `k`) | ✅ R0: 244 3564 247 3468 0x02 (x invertiert) |
| 3.2 | Trefferzonen | jede Zone (Hochformat 3 × 3, Querformat 4 × 2) wird beim Antippen der Mitte korrekt erkannt; die Punktspur liegt unter dem Finger | ✅ Sichtprüfung am Gerät |
| 3.3 | Persistenz | nach Strom aus/an keine erneute Kalibrierung nötig, Trefferzonen weiterhin korrekt | ✅ Sichtprüfung am Gerät |
| 3.4 | Recovery | Antippen beim Start erzwingt die Kalibrierung; `x` setzt zurück | ✅ Sichtprüfung am Gerät |
| 3.5 | IRQ-Leitung | Anzeige wechselt bei Berührung zwischen „aktiv“ und „inaktiv“ (nur informativ) | ✅ Sichtprüfung am Gerät |
| 7.1 | Heap-Basiswert | Werte „Start“, „nach Display-Init“ und „nach Sprite-Test“ protokolliert | ✅ siehe Messprotokoll |
| 7.2 | Sprite-Machbarkeit | Ergebnis 240 × 240 bei 8 und 16 Bit protokolliert | ✅ 8 Bit ok, 16 Bit nicht möglich |
| 7.3 | Stabilität | 30 min Laufzeit: Werte „periodisch“ ohne fallenden Trend | ⏭️ nicht separat gemessen; Langzeittest folgt in M3/M5 |

**Abnahme M1:** am 2026-11-08 durch den Projektverantwortlichen erteilt.

### Messprotokoll M1

| Messpunkt | frei (B) | min (B) | max. Block (B) |
|---|---|---|---|
| Start | 333 404 | 327 620 | 110 580 |
| nach Display-Init | 333 156 | 327 256 | 110 580 |
| nach Sprite-Test | 333 156 | 273 236 | 110 580 |
| periodisch nach 30 min | | | |

| Sprite 240 × 240 | Ergebnis | belegter Heap |
|---|---|---|
| 8 Bit | ✅ OK | 59 396 B |
| 16 Bit | ❌ nicht allozierbar | – (benötigt 115 200 B, größter freier Block nur 110 580 B) |

Messung vom 2026-11-08 ohne aktives WLAN. **Folgerung:** Ein 16-Bit-Vollsprite für das Radar ist auf dem WROOM-32 nicht möglich. Für M4 ist ein 8-Bit-Sprite (ca. 58 KB) vorgesehen; nach den TLS-Messungen in M3 wird erneut geprüft.

## Host-Unit-Tests

```bash
tests/host/run.sh
```

Stand M3: 84 Prüfungen, 0 Fehler. Geprüft werden:

- Distanz: Nullstrecke, 1° Breite, Berlin–München, Symmetrie
- Peilung: Nord, Süd, Ost, West, Nordwest, Wertebereich
- Bounding Box: 100 km bei 52,3° N, Punkte auf dem Kreis, Datumsgrenze, Polnähe, hohe Breiten
- Gültigkeit von Koordinaten, Normalisierung des Längengrads
- Credits je Flächenstufe, Tagesverbrauch, kürzestes zulässiges Intervall (authentifiziert und anonym)
- Parser:
  - vollständiger Datensatz
  - Null-Felder
  - fehlende Koordinaten
  - außerhalb des Radius
  - doppelte Callsigns
  - Kurs 0° und 359°
  - `states: null` und leeres Array
  - abgebrochener Datenstrom, HTML statt JSON, defektes JSON
- Flugliste: Kapazitätsgrenze (die nächsten bleiben), Sortierung

Zusätzlich wurde eine reale OpenSky-Antwort (68 State-Vektoren) auf dem Entwicklungsrechner geparst.

## M2 – Konfiguration und WLAN-Einrichtung

| Nr. | Prüfung | Erwartung | Ergebnis |
|---|---|---|---|
| 2.1 | Start ohne Konfiguration | „Setup erforderlich“ mit SSID, zufälligem Passwort, Adresse; seriell „Setup-AP … aktiv“ | ✅ seriell bestätigt |
| 2.2 | AP-Schutz | Anmeldung am Setup-WLAN nur mit dem angezeigten Passwort; neues Passwort nach jedem Setup-Start | ☐ |
| 2.3 | Captive Portal | Einrichtungsseite öffnet sich nach der Anmeldung automatisch (iOS/Android); sonst `http://192.168.4.1` | ☐ |
| 2.4 | WLAN-Suche | „Suchen“ listet die Netze der Umgebung | ☐ |
| 2.5 | Verbindungstest | richtiges Passwort → „Verbindung erfolgreich“; falsches → Fehlermeldung | ☐ |
| 2.6 | Plausibilitätsprüfung | ungültige Koordinaten, zu kurzes Geräte-Passwort, zu kurzes Intervall werden mit Meldung abgelehnt | ☐ |
| 2.7 | Credit-Schätzung | 50 km / 60 s / mit Zugang → ca. 1 440 Credits (36 %) | ☐ |
| 2.8 | Speichern + Neustart | Gerät startet im Normalbetrieb, verbindet sich, Statusbildschirm zeigt IP und Standort | ☐ |
| 2.9 | Stromunterbrechung | nach Strom aus/an wieder Normalbetrieb ohne Setup-WLAN | ☐ |
| 2.10 | Weboberfläche im Heimnetz | ohne Anmeldung kein Zugriff; mit `admin` + Geräte-Passwort Zugriff; Geheimnisse nicht sichtbar | ☐ |
| 2.11 | Änderung im Heimnetz | z. B. Reichweite ändern → gespeichert, Neustart, neuer Wert aktiv; leere Secret-Felder lassen Werte unverändert | ☐ |
| 2.12 | mDNS | `http://homeflighttracker.local` erreichbar (falls vom Netz/Client unterstützt) | ☐ |
| 2.13 | Setup über Touch | Status → Setup → Ja startet den Setup-Modus; „Abbrechen“ kehrt zurück | ☐ |
| 2.14 | WLAN-Ausfall | Router aus/an: Status „nicht verbunden“, danach automatische Wiederverbindung | ☐ |
| 2.15 | Werksreset | 8 s halten beim Start löscht alles, danach „Setup erforderlich“ | ☐ |
| 2.16 | Diagnose | Status → Diagnose zeigt den Hardwaretest; Info → Neustart kehrt zurück | ☐ |
| 2.17 | Setup-Timeout | nach 15 min ohne Speichern Neustart (mit Konfiguration: Normalbetrieb) | ☐ |

**Abnahme M2:** am 2026-11-08 durch den Projektverantwortlichen erteilt (Einrichtung, Kalibrierung, Statusanzeige und WLAN-Verbindung am Gerät geprüft).

## M3 – OpenSky-Abruf

| Nr. | Prüfung | Erwartung | Ergebnis |
|---|---|---|---|
| 3.1 | NTP | Uhrzeit in der Statusleiste korrekt (MEZ/MESZ) | ☐ |
| 3.2 | TLS + OAuth2 | seriell „Token: ok (HTTP 200)“, keine Zugangsdaten im Log | ✅ 2026-11-08 |
| 3.3 | `/states/all` | HTTP 200, 1 Credit je Abruf, `X-Rate-Limit-Remaining` sinkt um 1 | ✅ 3999 → 3995 |
| 3.4 | Takt | Abstand der Abrufe = Intervall (60 s) | ✅ seriell bestätigt |
| 3.5 | Reale Daten | Flugzeuge mit plausiblen Werten (Gerät bei Flugverkehr) | ☐ (nachts kein Verkehr; Parser mit realer Antwort am Rechner ✅) |
| 3.6 | Falsches Secret | „Zugang testen“ meldet „Anmeldung abgelehnt“; im Betrieb Auth-Fehlerstatus ohne Endlosschleife | ☐ |
| 3.7 | Token-Erneuerung | nach > 25 min neuer Token ohne Unterbrechung | ☐ |
| 3.8 | Netzwerkausfall | Router aus: Status „warte auf WLAN“ bzw. Fehler mit Backoff; danach automatische Fortsetzung | ☐ |
| 3.9 | Heap-Stabilität | ≥ 100 Abrufe ohne fallenden Trend des freien Heaps; ablesbar im Menü (Laufzeit, Abrufe, Fehler, Heap/min) | ☐ (erste Abrufe stabil bei ca. 104 KB) |
| 3.10 | Kein Abruf durch Touch | beliebige Bedienung ändert den Abruftakt nicht | ☐ |

Hinweis: Das Öffnen der seriellen Schnittstelle setzt das Gerät über den USB-Seriell-Adapter zurück (`POWERON_RESET`). Langläufe deshalb über das Menü ablesen, nicht per seriellem Monitor.

## M4 – Radar-Oberfläche

| Nr. | Prüfung | Erwartung | Ergebnis |
|---|---|---|---|
| 4.1 | Darstellung | Ringe, N oben, Symbole zeigen in Flugrichtung, Positionen passen zur Realität (z. B. Vergleich mit einer Flugverfolgungs-Website) | ☐ |
| 4.2 | Geometrie | Flugzeug nördlich erscheint oben, östlich rechts; Entfernung in Kurzinfo = Position im Ringraster | ☐ |
| 4.3 | Auswahl | ohne Auswahl Kurzinfo am nächstgelegenen Flugzeug; Antippen wählt (orange), Kurzinfo öffnet Details; bei überlappenden Symbolen schaltet erneutes Antippen durch; leere Stelle hebt Auswahl auf | ☐ |
| 4.4 | Details | Einheiten m, km/h, °, m/s, km; fehlende Werte „-“; Hinweis zu Typ/Start/Ziel; „Letzter Kontakt“ zählt hoch | ☐ |
| 4.5 | Liste | sortiert nach Entfernung, Blättern, Zeile öffnet Details | ☐ |
| 4.6 | Zoom | + / − wechselt 25/50/100 km, bleibt nach Neustart erhalten, löst keinen Abruf aus | ☐ |
| 4.7 | Veraltete Daten | WLAN aus: nach 3 Intervallen „Daten veraltet“ (grau), nach 15 min „Keine aktuellen Daten“ | ☐ |
| 4.8 | Umlaute | „Menü“, „Höhe“, „Credits übrig“, „Nächster Abruf“ korrekt lesbar | ☐ |
| 4.9 | Keine Flugzeuge | leeres Radar mit „0 Flugz.“, Liste „Keine Flugzeuge im Radius“ | ✅ nachts beobachtet (seriell) |

## Spätere Testkataloge (Übersicht)

- **M5:** Langzeittest ≥ 24 h, Stromunterbrechung, WLAN-Ausfall, Token-Ablauf, unabhängiges Code-Review.
